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

    Runs via the unified inference layer (Utilities/inference/) -- CoreML ML Program on
    the Apple GPU on macOS, DirectML on Windows, CPU fallback -- as a STATIC 4x512x512
    graph (see SharedSession in pmrid.cpp for why and the measurements).

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

/* Where the most recent Apply spent its time -- the probe for making "Denoise raw"
   faster (is it the model, and on which backend, or the CPU work around it?). Same
   locked-snapshot contract as LastResolution. All times in ms. inferMs is the sum of
   the per-tile InferenceSession::Run calls; the first Run of a session can include
   CoreML's on-device compile, so it is reported apart (inferFirstMs) and min/max show
   the steady state. sessionLoadMs is the one-time model load, paid by whichever call
   came first this session (not necessarily this Apply). Two Applies can run at once
   (a Develop denoise and an ImageCache saved-edit decode of the same image), so a
   caller that wants ITS run's numbers passes Apply's outTiming instead. */
struct Timing {
    bool valid = false;
    QString backend;            // InferenceSession::BackendName
    int width = 0, height = 0;  // sensor mosaic
    int tiles = 0;
    int tileW = 0, tileH = 0;   // padded model input of the FIRST tile (packed grid)
    bool firstRunOfSession = false;
    double sessionLoadMs = 0.0;
    double warmUpMs = 0.0;      // WarmUpAsync's first Run, if it has finished (0 = not)
    double kbMs      = 0.0;     // resolveKB (blind estimation can be the costly tier)
    double packMs    = 0.0;     // cfa -> planar RGGB
    double tileInMs  = 0.0;     // sum: build padded KSigma input tensors
    double inferMs   = 0.0;     // sum: model Run
    double inferFirstMs = 0.0, inferMinMs = 0.0, inferMaxMs = 0.0;
    double tileOutMs = 0.0;     // sum: inverse KSigma + feathered accumulate
    double unpackMs  = 0.0;     // packed -> mosaic
    double totalMs   = 0.0;     // whole Apply
    QString Summary() const;    // one line, for the log and the Develop diagnostics
};
Timing LastTiming();

/* Denoise raw.cfa in place at full strength. Returns true iff the mosaic was modified.
   No-op (false) when the model is not loaded or the pattern is non-Bayer. Call AFTER
   SubtractBlack (expects black-subtracted cfa in [0, raw.white]). iso + model select the
   per-camera noise calibration (model is ImageMetadata::model, e.g. "Sony ILCE-7RM5").
   progress, when set, is called after each tile as progress(tilesDone, tilesTotal) -- used to
   drive the status-bar progress bar during the interactive denoise (MW::ensureRawDenoise). It may
   be called from a worker thread, so the callback must marshal to the GUI thread itself.
   outTiming, when set, receives THIS call's stage timing (see Timing). */
using ProgressFn = std::function<void(int done, int total)>;
bool Apply(RawImage &raw, int iso, const QString &model, const ProgressFn &progress = {},
           Timing *outTiming = nullptr);

/* True if the model is loaded and can denoise -- false when Winnow was built without ONNX
   Runtime (CMake WINNOW_ENABLE_ORT=OFF -> the OrtBackend stub) or pmrid.onnx is not beside
   the binary. Lets a caller tell "denoise is unavailable in this build" (permanent, session
   wide) from "this SENSOR is not supported" (per image), which Apply's single false cannot.
   Loads the session on first call, like Apply. */
bool IsAvailable();

/* Load the session and run one dummy tile on a pool thread, once per process (later
   calls return at once). The first Run of a fresh session costs ~1.8 s on the CoreML GPU
   path even with a warm compile cache -- CoreML prepares the graph on first execution --
   and without this the user's first denoise of the launch paid it (measured 2026-10-13:
   first tile 1843 ms, then ~7 ms). MW calls it on entering Develop with the Winnow engine
   and on switching to that engine in Develop. No-op when IsSupportedBuild() is false.
   Safe to overlap with a real Apply: the session is shared and ORT Run is thread-safe. */
void WarmUpAsync();

/* Could PMRID EVER run in this build -- an inference backend is compiled in and pmrid.onnx
   is beside the binary? Cheap: no session load, no model parse, so it is safe to call from
   the GUI thread on every panel refresh (unlike IsAvailable, whose first call loads and
   compiles the model). Use it to grey the "Denoise raw" controls up front; use IsAvailable
   to explain a denoise that already ran and changed nothing. */
bool IsSupportedBuild();

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
