#include "Utilities/inference/ortbackend.h"

#include <QDir>
#include "Main/global.h"

/*
    ONNX Runtime backend. Everything ORT-specific is confined to this file. When WINNOW_HAVE_ORT
    is undefined (ORT not vendored yet / build option off) the whole file collapses to a stub so
    the app still builds and links -- IsLoaded() is false and Run() no-ops, exactly like a
    missing model.
*/

#ifdef WINNOW_HAVE_ORT

#include <onnxruntime_cxx_api.h>
#ifdef Q_OS_MAC
#include <coreml_provider_factory.h>
#endif
#ifdef Q_OS_WIN
#include <dml_provider_factory.h>
#include <string>
#endif

namespace {
/* One process-wide ORT environment (thread-safe, created once). */
Ort::Env &ortEnv()
{
    static Ort::Env env(ORT_LOGGING_LEVEL_WARNING, "Winnow");
    return env;
}

/* ORT wants the native path char type: wchar_t on Windows, char elsewhere. */
#ifdef Q_OS_WIN
std::wstring nativePath(const QString &p) { return p.toStdWString(); }
#else
std::string  nativePath(const QString &p) { return p.toStdString(); }
#endif
} // namespace

struct OrtBackend::Impl {
    Ort::Session session{nullptr};
    std::vector<std::string> inNames;
    std::vector<std::string> outNames;
    QString backend = "none";
    bool loaded = false;
    QString path;               // the model, for the issue log (Run has no other name)

    // the constructor's work, in a try there: see OrtBackend::OrtBackend
    void load(const QString &onnxPath, InferenceDevice pref,
              const QString &compileCacheDir, const FixedDims &fixedDims);
};

OrtBackend::OrtBackend(const QString &onnxPath, InferenceDevice pref,
                       const QString &compileCacheDir, const FixedDims &fixedDims)
    : d(std::make_unique<Impl>())
{
    if (G::isLogger) G::log("OrtBackend::OrtBackend", onnxPath);
    d->path = onnxPath;

    /*  NOTHING ESCAPES THE CONSTRUCTOR. Its callers build it inside std::call_once on a
        decode or develop thread, where a throw is std::terminate. The Env, the session
        options and the dimension overrides all throw Ort::Exception, and any allocation
        can throw bad_alloc: each leaves an unloaded backend, which every caller already
        handles as a missing model. */
    try {
        d->load(onnxPath, pref, compileCacheDir, fixedDims);
    } catch (const std::exception &e) {
        G::issueDedup("Error", QString("Model runtime could not be set up: ") + e.what(),
                      "OrtBackend", -1, onnxPath);
        d->loaded = false;
    }
}

void OrtBackend::Impl::load(const QString &onnxPath, InferenceDevice pref,
                            const QString &compileCacheDir, const FixedDims &fixedDims)
{
    /* Create the Env FIRST: it initializes ORT's logging manager, which the CoreML EP's
       HasNeuralEngine() probe consults via DefaultLogger(). Appending the EP before any Env
       exists makes ORT throw (OnnxRuntimeException) from inside the provider factory -- a raw
       C++ throw that bypasses the C-API status, so it must not be allowed to happen. */
    Ort::Env &env = ortEnv();

    Ort::SessionOptions so;
    so.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);
    // C API: the C++ wrapper only gained this in newer ORT (Ubuntu ships 1.23)
    for (const auto &[name, value] : fixedDims)
        Ort::ThrowOnError(Ort::GetApi().AddFreeDimensionOverrideByName(so, name.c_str(),
                                                                       value));

    /* Try the platform accelerator EP first; on any failure fall through to a plain CPU
       session (ORT runs every node on CPU when no EP claims it). */
    if (pref != InferenceDevice::CPU) {
        try {
#ifdef Q_OS_MAC
            /* Map the requested device onto CoreML compute units. flags 0 lets CoreML
               use the full set (ANE -> GPU -> CPU), the right default for Auto/NPU. GPU
               forces the Apple GPU (Metal / MPSGraph) and excludes the ANE -- needed for
               graphs whose ops the ANE cannot service (e.g. FFT-based FFC blocks). It
               pairs the GPU compute-unit flag with the ML Program format, which supports
               the broader op set. Note this only sets the target device: ops the
               CoreML EP cannot map are still partitioned to the CPU EP regardless of
               the flag. */
            if (pref == InferenceDevice::GPU && !compileCacheDir.isEmpty()
                && QDir().mkpath(compileCacheDir)) {
                /* The same GPU + ML Program target as the flags below, through the
                   provider-options API, which is the only one that takes a compiled-
                   model cache. See the header. */
                so.AppendExecutionProvider("CoreML", {
                    {"ModelFormat", "MLProgram"},
                    {"MLComputeUnits", "CPUAndGPU"},
                    {"ModelCacheDirectory", compileCacheDir.toStdString()}});
                backend = "CoreML/GPU (cached)";
            }
            else {
                uint32_t coremlFlags = 0;
                const char *coremlName = "CoreML";
                if (pref == InferenceDevice::GPU) {
                    coremlFlags = COREML_FLAG_USE_CPU_AND_GPU | COREML_FLAG_CREATE_MLPROGRAM;
                    coremlName = "CoreML/GPU";
                }
                Ort::ThrowOnError(OrtSessionOptionsAppendExecutionProvider_CoreML(so,
                                                                                  coremlFlags));
                backend = coremlName;
            }
#elif defined(Q_OS_WIN)
            /* DirectML requires sequential execution and no memory pattern. */
            so.DisableMemPattern();
            so.SetExecutionMode(ORT_SEQUENTIAL);
            Ort::ThrowOnError(OrtSessionOptionsAppendExecutionProvider_DML(so, 0));
            backend = "DirectML";
#endif
        } catch (const std::exception &e) {
            /* Every ONNX model (raw denoise, LaMa, MI-GAN, depth, SigLIP) passes here:
               the one place their load and run failures are logged. Info: the CPU still
               runs it, only slower. */
            G::issueDedup("Info", QString("Model runs on the CPU, the GPU/NPU was not "
                          "available: ") + e.what(), "OrtBackend", -1, onnxPath);
            backend = "none";
        }
    }

    try {
        const auto p = nativePath(onnxPath);
        session = Ort::Session(env, p.c_str(), so);

        Ort::AllocatorWithDefaultOptions alloc;
        const size_t nIn = session.GetInputCount();
        for (size_t i = 0; i < nIn; ++i) {
            auto name = session.GetInputNameAllocated(i, alloc);
            inNames.emplace_back(name.get());
        }
        const size_t nOut = session.GetOutputCount();
        for (size_t i = 0; i < nOut; ++i) {
            auto name = session.GetOutputNameAllocated(i, alloc);
            outNames.emplace_back(name.get());
        }
        if (backend == "none") backend = "CPU";
        loaded = true;
    } catch (const std::exception &e) {         // Ort::Exception, bad_alloc
        G::issueDedup("Error", QString("Model could not be loaded: ") + e.what(),
                      "OrtBackend", -1, onnxPath);
        loaded = false;
    }
}

OrtBackend::~OrtBackend() = default;

bool OrtBackend::IsLoaded() const { return d && d->loaded; }
std::vector<std::string> OrtBackend::InputNames() const { return d ? d->inNames : std::vector<std::string>{}; }
std::vector<std::string> OrtBackend::OutputNames() const { return d ? d->outNames : std::vector<std::string>{}; }
QString OrtBackend::BackendName() const { return d ? d->backend : QString("none"); }

bool OrtBackend::Run(const std::vector<std::string> &inNames,
                     const std::vector<Tensor> &inputs,
                     const std::vector<std::string> &outNames,
                     std::vector<Tensor> &outputs)
{
    if (!IsLoaded()) return false;          // the load failure is already logged
    if (inNames.size() != inputs.size() || outNames.empty()) {
        G::issueDedup("Error", "Model run refused: the input names and tensors do not "
                      "match (caller bug)", "OrtBackend::Run", -1, d->path);
        return false;
    }
    for (const Tensor &t : inputs)
        if (!t.isConsistent()) {
            G::issueDedup("Error", "Model run refused: an input tensor's shape and data "
                          "disagree (caller bug)", "OrtBackend::Run", -1, d->path);
            return false;
        }

    try {
        const Ort::MemoryInfo mem =
            Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);

        std::vector<Ort::Value> inVals;
        inVals.reserve(inputs.size());
        for (const Tensor &t : inputs) {
            inVals.push_back(Ort::Value::CreateTensor<float>(
                mem, const_cast<float *>(t.data.data()), t.data.size(),
                t.shape.data(), t.shape.size()));
        }

        std::vector<const char *> inN, outN;
        inN.reserve(inNames.size());
        outN.reserve(outNames.size());
        for (const std::string &s : inNames)  inN.push_back(s.c_str());
        for (const std::string &s : outNames) outN.push_back(s.c_str());

        auto outVals = d->session.Run(Ort::RunOptions{nullptr},
                                      inN.data(), inVals.data(), inVals.size(),
                                      outN.data(), outN.size());

        outputs.clear();
        outputs.resize(outVals.size());
        for (size_t i = 0; i < outVals.size(); ++i) {
            if (!outVals[i].IsTensor()) {
                G::issueDedup("Error", "Model run failed: an output is not a tensor",
                              "OrtBackend::Run", -1, d->path);
                outputs.clear();
                return false;
            }
            const auto info = outVals[i].GetTensorTypeAndShapeInfo();
            outputs[i].shape = info.GetShape();
            const size_t n = static_cast<size_t>(info.GetElementCount());
            const float *src = outVals[i].GetTensorData<float>();
            outputs[i].data.assign(src, src + n);
        }
        return true;
    } catch (const std::exception &e) {         // Ort::Exception, bad_alloc
        G::issueDedup("Error", QString("Model run failed: ") + e.what(), "OrtBackend::Run",
                      -1, d->path);
        outputs.clear();
        return false;
    }
}

bool OrtBackend::IsCompiledIn() { return true; }

#else  // !WINNOW_HAVE_ORT -- stub so the layer builds without ONNX Runtime.

struct OrtBackend::Impl {};

OrtBackend::OrtBackend(const QString &onnxPath, InferenceDevice, const QString &,
                       const FixedDims &)
{
    if (G::isLogger) G::log("OrtBackend::OrtBackend (ORT not built in)", onnxPath);
    qWarning("OrtBackend: built without ONNX Runtime (WINNOW_HAVE_ORT off); "
             "neural inference disabled for %s", onnxPath.toUtf8().constData());
}
OrtBackend::~OrtBackend() = default;
bool OrtBackend::IsLoaded() const { return false; }
bool OrtBackend::Run(const std::vector<std::string> &, const std::vector<Tensor> &,
                     const std::vector<std::string> &, std::vector<Tensor> &) { return false; }
std::vector<std::string> OrtBackend::InputNames() const { return {}; }
std::vector<std::string> OrtBackend::OutputNames() const { return {}; }
QString OrtBackend::BackendName() const { return "none"; }
bool OrtBackend::IsCompiledIn() { return false; }

#endif // WINNOW_HAVE_ORT
