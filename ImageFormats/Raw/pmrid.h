#ifndef PMRID_H
#define PMRID_H

#include <QString>
#include <functional>

struct RawImage;

/*
    PMRID -- pre-demosaic raw denoiser (MegVii "Practical Mobile Raw Image Denoising",
    ECCV'20, Apache-2.0). Runs on the CFA mosaic (RawImage.cfa) BEFORE demosaic, so it is
    a true sensor-domain denoiser. It supersedes the earlier post-demosaic RawRefinery
    "TreeNet" path (rawdenoise.*, removed 2026-07-12). It is used only on the in-house
    ("Winnow") decode path -- the Apple Core Image engine hands back an already-demosaiced
    image with no CFA to denoise.

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

    The (k,b) noise model is resolved per frame by a priority chain (PMRID::resolveKB):
      1. a DNG NoiseProfile (tag 51041) baked into the file -- exact, no calibration needed;
      2. a measured per-camera-model K(ISO)/B(ISO) table (tools/pmrid_calibration_capture.md
         -> calibrate_pmrid.py -> _pmrid_out/calib_<model>.json), interpolated in ISO;
      3. blind per-image estimation from the CFA (var-vs-mean over flat regions), so an
         uncalibrated body still denoises with its own noise model;
      4. a generic full-frame Sony table, only as a last-ditch guard.
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

/* The most recent (k,b) resolution by Apply (see PMRID::resolveKB) -- surfaced in the
   Develop diagnostics so a mis-denoise can be traced to its tier without the logger.
   Written on a worker thread, read on the GUI thread; LastResolution() is a locked
   snapshot. valid == false until Apply has run once this session. */
struct Resolution {
    bool valid = false;
    QString model;              // ImageMetadata::model seen
    int iso = 0;
    QString source;             // tier: NoiseProfile / model table / blind / generic
    double k = 0.0, b = 0.0;    // resolved coefficients (white-normalised green plane)
    bool hadNoiseProfile = false;
};
Resolution LastResolution();

} // namespace PMRID

#endif // PMRID_H
