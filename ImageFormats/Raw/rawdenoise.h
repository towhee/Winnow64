#ifndef RAWDENOISE_H
#define RAWDENOISE_H

#include "Develop/editparams.h"

struct WorkingImage;
class InferenceSession;

/*
    "Denoise raw": the Base-layer global noise reduction behind EditParams denoiseLuma/
    denoiseChroma. It runs a learned denoiser on the scene-linear WorkingImage produced by the
    raw decode (post-demosaic, before Develop), so it is baked into the pre-develop WorkingImage
    and is global / not maskable, matching the documented design.

    Model: RawRefinery "TreeNet" (github.com/rymuelle/RawRefinery) via the unified inference layer
    (Utilities/inference/) -- ANE on macOS through ORT's CoreML EP, DirectML on Windows. It takes
    demosaiced LINEAR Rec2020 RGB {1,3,H,W} in [0,1] plus an ISO conditioning scalar {1,1} (=ISO/
    6400) and returns the denoised RGB. Winnow's WorkingImage is scene-linear sRGB primaries, so
    the engine converts sRGB<->Rec2020 around the model, runs it tiled with a feathered blend, and
    applies the denoise as a highlight-preserving, amount-scaled correction (greens/luma by
    denoiseLuma, chroma by max(luma,chroma)).

    NOTE (long-term): a PRE-demosaic (Bayer-domain) denoiser is the better end goal; this
    post-demosaic path was adopted because it has proven public weights and works on every image
    (X-Trans and the macOS Apple decode engine included). See the plan and
    notes/Documentation.txt "Denoise raw".
*/
namespace RawDenoise {

/* Load-once shared session (rawdenoise.onnx next to the executable, warn-if-absent). Thread-safe;
   ORT sessions are safe to Run() from multiple decoder threads. */
InferenceSession *SharedSession();

/* Denoise img in place using the shared session; iso conditions the model (0/unknown -> 100).
   Returns true iff modified. No-op (false) when both amounts are 0 or the model is not loaded.
   Safe to call unconditionally from the decode pipeline (any format, any decode engine). */
bool Apply(WorkingImage &img, const EditParams &edit, int iso);

/* Core routine with an injected session, for the standalone oracle/validation build. */
bool DenoiseRgb(WorkingImage &img, const EditParams &edit, int iso, InferenceSession &session);

} // namespace RawDenoise

#endif // RAWDENOISE_H
