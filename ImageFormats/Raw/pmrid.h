#ifndef PMRID_H
#define PMRID_H

#include <QString>
#include <functional>

struct RawImage;

/*
    PMRID -- pre-demosaic raw denoiser (MegVii "Practical Mobile Raw Image Denoising",
    ECCV'20, Apache-2.0). Runs on the CFA mosaic (RawImage.cfa) BEFORE demosaic, so it is
    a true sensor-domain denoiser, unlike the post-demosaic TreeNet path (rawdenoise.*,
    now the Effects local "Denoise"). It is used only on the in-house ("Winnow") decode
    path -- the Apple Core Image engine hands back an already-demosaiced image with no
    CFA to denoise.

    Model contract (validated in tools/oracle_pmrid.py):

      pack cfa -> 4-ch RGGB [1,4,H/2,W/2] normalised to [0,1] by the (black-subtracted)
      white level -> KSigma LINEAR noise-normalization to a fixed anchor ISO (NOT a
      sqrt/Anscombe VST) -> x256 -> net (residual) -> /256 -> inverse KSigma -> unpack
      back into the mosaic.

    Bayer only (RGGB/BGGR/GRBG/GBRG, mapped to RGGB phase); X-Trans and unknown patterns
    are left untouched.

    Runs via the unified inference layer (Utilities/inference/) -- CoreML/ANE on macOS
    (the graph partitions cleanly here, unlike TreeNet), DirectML on Windows, CPU
    fallback.

    Full strength only: the caller decides WHETHER to denoise (engine == Winnow,
    Denoise-raw amount > 0) and applies the user's amount as a blend elsewhere, so this
    stays a clean, parameter-free denoiser (keeps a future "fixed strength" mode a small
    change).

    Noise coefficients are per-camera-model K(ISO)/B(ISO) from the calibration capture
    (tools/pmrid_calibration_capture.md -> calibrate_pmrid.py -> _pmrid_out/calib_<model>.json),
    interpolated in ISO; uncalibrated bodies fall back to a generic full-frame Sony table.
*/
namespace PMRID {

/* Denoise raw.cfa in place at full strength. Returns true iff the mosaic was modified.
   No-op (false) when the model is not loaded or the pattern is non-Bayer. Call AFTER
   SubtractBlack (expects black-subtracted cfa in [0, raw.white]). iso + model select the
   per-camera noise calibration (model is ImageMetadata::model, e.g. "Sony ILCE-7RM5").
   progress, when set, is called after each tile as progress(tilesDone, tilesTotal) -- used to
   drive the status-bar progress bar during the interactive denoise (MW::ensureRawDenoise). It may
   be called from a worker thread, so the callback must marshal to the GUI thread itself. */
using ProgressFn = std::function<void(int done, int total)>;
bool Apply(RawImage &raw, int iso, const QString &model, const ProgressFn &progress = {});

} // namespace PMRID

#endif // PMRID_H
