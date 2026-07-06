#include "Utilities/inference/inferencesession.h"
#include "Utilities/inference/ortbackend.h"

/*
    Backend selection lives here and nowhere else (the escape-hatch seam). For now every model
    runs on ONNX Runtime via OrtBackend; a future native backend would be chosen per model here.
*/
InferenceSession::InferenceSession(const QString &onnxPath, InferenceDevice pref)
    : backend(std::make_unique<OrtBackend>(onnxPath, pref))
{
}

InferenceSession::~InferenceSession() = default;

bool InferenceSession::IsLoaded() const { return backend && backend->IsLoaded(); }

bool InferenceSession::Run(const std::vector<std::string> &inNames,
                           const std::vector<Tensor> &inputs,
                           const std::vector<std::string> &outNames,
                           std::vector<Tensor> &outputs)
{
    return backend && backend->Run(inNames, inputs, outNames, outputs);
}

std::vector<std::string> InferenceSession::InputNames() const
{
    return backend ? backend->InputNames() : std::vector<std::string>{};
}

std::vector<std::string> InferenceSession::OutputNames() const
{
    return backend ? backend->OutputNames() : std::vector<std::string>{};
}

QString InferenceSession::BackendName() const
{
    return backend ? backend->BackendName() : QString("none");
}
