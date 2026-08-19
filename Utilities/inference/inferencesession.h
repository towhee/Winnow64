#ifndef INFERENCESESSION_H
#define INFERENCESESSION_H

#include <memory>
#include <QString>
#include "Utilities/inference/inferencebackend.h"

/*
    Front door to the inference layer. A feature constructs one InferenceSession per model
    (from a path resolved the usual way -- applicationDirPath() + a ReleaseExtras/ model file,
    warn-if-absent) and calls Run(). The session owns an InferenceBackend; today it always
    builds an OrtBackend, but this is the single place backend selection happens, so a native
    CoreMLBackend (for generative fill, say) can be chosen here later without any caller change.

    Thin by design: it forwards to the backend and adds only construction/ownership. See
    Utilities/inference/inferencebackend.h for the Tensor / InferenceDevice types.
*/
class InferenceSession
{
public:
    explicit InferenceSession(const QString &onnxPath,
                              InferenceDevice pref = InferenceDevice::Auto);
    ~InferenceSession();

    bool IsLoaded() const;

    /* Can ANY model load in this build (i.e. is an inference backend compiled in)? Static
       and free -- no session, no file read -- so a feature can grey its UI up front rather
       than after a first failed run. False means every InferenceSession here is inert. */
    static bool BackendCompiledIn();

    bool Run(const std::vector<std::string> &inNames,
             const std::vector<Tensor> &inputs,
             const std::vector<std::string> &outNames,
             std::vector<Tensor> &outputs);

    std::vector<std::string> InputNames() const;
    std::vector<std::string> OutputNames() const;
    QString BackendName() const;

private:
    std::unique_ptr<InferenceBackend> backend;
};

#endif // INFERENCESESSION_H
