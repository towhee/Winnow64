#ifndef IMAGEEMBEDDER_H
#define IMAGEEMBEDDER_H

#include <QImage>
#include <QString>
#include <QVector>

/*
    SigLIP 2 IMAGE EMBEDDINGS, for Audit Keywords (Utilities/keywordaudit.h).

    One 768-d, L2-normalised vector per image from the SigLIP 2 base/16 image tower at
    256 px (siglip2_image.onnx, ModelStore::Model::SigLipImage, exported by
    tools/export_siglip.py). Two images look alike to the audit exactly when the dot
    product of their vectors is high; nothing else about the network matters here.

    PREPROCESSING IS PART OF THE CONTRACT and must match the export and the Phase 0
    prototype (tools/keyword_audit_proto.py) that the audit's thresholds were measured
    with: RGB, squashed to 256 x 256 with no crop, bicubic, then (x / 255 - 0.5) / 0.5.
    The L2 normalisation is inside the graph.

    THE INPUT IS A THUMBNAIL. 256 px is what the model sees, so the browsing thumbnail
    (ThumbCache, ~256 px) is the right source -- decoding a full-size image would cost
    a hundred times more for no better vector.

    THREADING: Embed() may be called from any thread; the session is shared and ORT's
    Run is thread-safe. The session is created on first use and RETRIED while the model
    is absent, because the model is downloaded on demand (see MiganFill::SharedSession).
*/
namespace ImageEmbedder {

constexpr int kSize = 256;
constexpr int kDim = 768;

/* Identifies the vectors this build produces, stored with every image_embedding row.
   Changes whenever the model or the preprocessing does, so old vectors become misses. */
QString ModelTag();

/* An inference backend is compiled into this build. */
bool IsSupportedBuild();

/* The model file is present and current (ModelStore). Cheap; any thread. */
bool IsModelPresent();

/* Embed a batch. out gets one entry per input: kDim floats, or empty for a null image.
   Returns false when the model cannot be loaded or a run fails. */
bool Embed(const QVector<QImage> &images, QVector<QVector<float>> &out);

/* True when the first Embed() will first compile the model for this machine -- macOS,
   CoreML, ~90 s once per model version -- so the UI can say so instead of looking
   hung. Cheap; any thread. */
bool NeedsFirstCompile();

/* The backend actually in use ("CoreML/GPU", "DirectML", "CPU"...), for diagnostics. */
QString BackendName();

}   // namespace ImageEmbedder

#endif // IMAGEEMBEDDER_H
