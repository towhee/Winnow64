#ifndef ORTBACKEND_H
#define ORTBACKEND_H

#include <memory>
#include "Utilities/inference/inferencebackend.h"

/*
    ONNX Runtime implementation of InferenceBackend. Per-platform execution providers are
    selected at load (guarded): CoreML EP on macOS (dispatches to the Apple Neural Engine /
    GPU), DirectML on Windows, and the CPU EP as the universal fallback. A failed or
    unavailable EP degrades to CPU rather than failing the load.

    All onnxruntime contact lives in ortbackend.cpp behind an opaque Impl (PIMPL), so this
    header -- and every translation unit that includes it -- stays free of ORT headers. When
    Winnow is built without ORT (WINNOW_HAVE_ORT undefined), the class still compiles and links;
    IsLoaded() is false and Run() no-ops.
*/
class OrtBackend : public InferenceBackend
{
public:
    /* Load onnxPath. pref biases EP selection (Auto/NPU/GPU pick the platform accelerator;
       CPU forces the CPU EP). Never throws: on failure the backend is simply not loaded. */
    explicit OrtBackend(const QString &onnxPath, InferenceDevice pref = InferenceDevice::Auto);
    ~OrtBackend() override;

    bool IsLoaded() const override;
    bool Run(const std::vector<std::string> &inNames,
             const std::vector<Tensor> &inputs,
             const std::vector<std::string> &outNames,
             std::vector<Tensor> &outputs) override;
    std::vector<std::string> InputNames() const override;
    std::vector<std::string> OutputNames() const override;
    QString BackendName() const override;

private:
    struct Impl;
    std::unique_ptr<Impl> d;
};

#endif // ORTBACKEND_H
