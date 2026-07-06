#include "Utilities/inference/ortbackend.h"
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
};

OrtBackend::OrtBackend(const QString &onnxPath, InferenceDevice pref)
    : d(std::make_unique<Impl>())
{
    if (G::isLogger) G::log("OrtBackend::OrtBackend", onnxPath);

    /* Create the Env FIRST: it initializes ORT's logging manager, which the CoreML EP's
       HasNeuralEngine() probe consults via DefaultLogger(). Appending the EP before any Env
       exists makes ORT throw (OnnxRuntimeException) from inside the provider factory -- a raw
       C++ throw that bypasses the C-API status, so it must not be allowed to happen. */
    Ort::Env &env = ortEnv();

    Ort::SessionOptions so;
    so.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);

    /* Try the platform accelerator EP first; on any failure fall through to a plain CPU
       session (ORT runs every node on CPU when no EP claims it). */
    if (pref != InferenceDevice::CPU) {
        try {
#ifdef Q_OS_MAC
            /* flags 0 = let CoreML use the full compute unit set (ANE -> GPU -> CPU). */
            Ort::ThrowOnError(OrtSessionOptionsAppendExecutionProvider_CoreML(so, 0));
            d->backend = "CoreML";
#elif defined(Q_OS_WIN)
            /* DirectML requires sequential execution and no memory pattern. */
            so.DisableMemPattern();
            so.SetExecutionMode(ORT_SEQUENTIAL);
            Ort::ThrowOnError(OrtSessionOptionsAppendExecutionProvider_DML(so, 0));
            d->backend = "DirectML";
#endif
        } catch (const std::exception &e) {
            qWarning("OrtBackend: accelerator EP unavailable (%s); using CPU", e.what());
            d->backend = "none";
        }
    }

    try {
        const auto p = nativePath(onnxPath);
        d->session = Ort::Session(env, p.c_str(), so);

        Ort::AllocatorWithDefaultOptions alloc;
        const size_t nIn = d->session.GetInputCount();
        for (size_t i = 0; i < nIn; ++i) {
            auto name = d->session.GetInputNameAllocated(i, alloc);
            d->inNames.emplace_back(name.get());
        }
        const size_t nOut = d->session.GetOutputCount();
        for (size_t i = 0; i < nOut; ++i) {
            auto name = d->session.GetOutputNameAllocated(i, alloc);
            d->outNames.emplace_back(name.get());
        }
        if (d->backend == "none") d->backend = "CPU";
        d->loaded = true;
    } catch (const Ort::Exception &e) {
        qWarning("OrtBackend: failed to load %s: %s",
                 onnxPath.toUtf8().constData(), e.what());
        d->loaded = false;
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
    if (!IsLoaded()) return false;
    if (inNames.size() != inputs.size() || outNames.empty()) return false;
    for (const Tensor &t : inputs)
        if (!t.isConsistent()) return false;

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
            if (!outVals[i].IsTensor()) return false;
            const auto info = outVals[i].GetTensorTypeAndShapeInfo();
            outputs[i].shape = info.GetShape();
            const size_t n = static_cast<size_t>(info.GetElementCount());
            const float *src = outVals[i].GetTensorData<float>();
            outputs[i].data.assign(src, src + n);
        }
        return true;
    } catch (const Ort::Exception &e) {
        qWarning("OrtBackend::Run failed: %s", e.what());
        return false;
    }
}

#else  // !WINNOW_HAVE_ORT -- stub so the layer builds without ONNX Runtime.

struct OrtBackend::Impl {};

OrtBackend::OrtBackend(const QString &onnxPath, InferenceDevice)
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

#endif // WINNOW_HAVE_ORT
