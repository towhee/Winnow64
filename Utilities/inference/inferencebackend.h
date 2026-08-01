#ifndef INFERENCEBACKEND_H
#define INFERENCEBACKEND_H

#include <cstdint>
#include <string>
#include <vector>
#include <QString>

/*
    Unified model-inference seam. Every Winnow feature that runs a neural model (raw denoise,
    masks, generative fill, ...) talks to an InferenceBackend, never to a concrete runtime. The
    default backend is OrtBackend (ONNX Runtime, with the CoreML EP on macOS -> Apple Neural
    Engine, DirectML on Windows, CPU everywhere as the fallback). This abstract class is the
    escape hatch: a native CoreMLBackend (or any other) can be dropped in later -- e.g. for the
    generative-fill model where maximal ANE utilisation matters -- without touching callers.

    See notes/Documentation.txt "INFERENCE" and the plan
    let-s-add-the-denoise-sequential-crown.md. This deliberately supersedes the old cv::dnn /
    "no onnxruntime" approach for new models; the existing cv::dnn predictors keep working and
    migrate opportunistically.

    Header is runtime-agnostic on purpose: it pulls in NO onnxruntime headers, so the whole app
    can include it. All ORT contact is confined to ortbackend.cpp (PIMPL), and the layer builds
    and links even when ORT is absent (IsLoaded() then returns false and Run() is a no-op).
*/

/* Where the caller would like the model to run. Auto lets the backend pick the best available
   (NPU -> GPU -> CPU). The backend downgrades silently if a preference is unavailable. */
enum class InferenceDevice { Auto, NPU, GPU, CPU };

/*
    A dense float tensor in row-major order over `shape` (typically NCHW, e.g. {1,4,H,W}).
    Inputs and outputs are passed as named tensors so a model's I/O can be addressed without
    hard-coding positions.
*/
struct Tensor {
    std::vector<int64_t> shape;
    std::vector<float>   data;

    int64_t count() const {
        if (shape.empty()) return 0;
        int64_t n = 1;
        for (int64_t d : shape) n *= d;
        return n;
    }
    bool isConsistent() const { return static_cast<int64_t>(data.size()) == count(); }
};

class InferenceBackend
{
public:
    virtual ~InferenceBackend() = default;

    /* True once a model is loaded and ready. False if the runtime is unavailable (ORT not
       built in) or the model failed to load -- callers then no-op, matching the existing
       warn-if-absent model convention. */
    virtual bool IsLoaded() const = 0;

    /* Run one forward pass. inputs are matched to inNames positionally (inNames[i] names
       inputs[i]); outputs are filled in outNames order. Returns false on any failure (not
       loaded, shape/name mismatch, runtime error) without throwing. */
    virtual bool Run(const std::vector<std::string> &inNames,
                     const std::vector<Tensor> &inputs,
                     const std::vector<std::string> &outNames,
                     std::vector<Tensor> &outputs) = 0;

    /* Model I/O names discovered at load, so callers/tilers need not hard-code them. */
    virtual std::vector<std::string> InputNames() const = 0;
    virtual std::vector<std::string> OutputNames() const = 0;

    /* Human-readable active backend for logging: "CoreML", "DirectML", "CPU", or "none". */
    virtual QString BackendName() const = 0;
};

#endif // INFERENCEBACKEND_H
