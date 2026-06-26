#ifndef DEVELOP_H
#define DEVELOP_H

#include "Develop/editparams.h"
#include "Develop/workingimage.h"

/*
    Applies parametric develop adjustments to a WorkingImage in place. Stateless and
    reentrant, constructed per decode (same discipline as Demosaic / RawFormat) so it
    carries no cross-thread state.

    The operation order is fixed and hard-coded (Lightroom-like); order matters and is not a
    caller concern. Ops are split by cost (see notes/Documentation.txt "Layer & masking
    model"):

        SPATIAL ops (denoise, texture, dehaze) need a neighbourhood, so each owns a full-image
        pass. Denoise runs first; texture and dehaze run after the point pass. Each is a no-op
        when its slider is 0.

        POINT ops (white balance, exposure, contrast, tone regions) are pure per-pixel functions,
        so they are FUSED into a single parallel pass: coefficients are precomputed once from
        EditParams, then applied per pixel in one loop over the image. This is the slider-drag
        hot path. All Basic sliders are now implemented; the proxy/coalesce preview pipeline
        (MW::renderDevelopPreview) keeps spatial-op cost off the interactive drag.
*/
class Develop
{
public:
    Develop() = default;

    /* Apply p to img in place. Returns true on success (and trivially when p is identity,
       leaving img untouched). */
    bool Apply(WorkingImage &img, const EditParams &p);

private:
    /* Spatial op: local (maskable) luminance NR, owns a full-image pass. Ratio-preserving --
       smooths luminance only (chroma untouched), driven by EditParams::localDenoiseLuma. Runs
       BEFORE the fused point pass (fixed pipeline order). This is the GLOBAL (no-mask) case; the
       planned layer compositor calls it per layer, bounded to the mask bbox and blended by the
       layer's alpha (see notes/Documentation.txt "Layer & masking model"). No-op when the
       strength is 0. */
    void Denoise(WorkingImage &img, const EditParams &p);

    /* Spatial op (pipeline #6): mid-frequency local contrast on LUMINANCE only (ratio-preserving,
       like Denoise), in the perceptual domain. A Gaussian base isolates a mid-frequency detail
       band; positive texture amplifies it, negative attenuates it. The base radius scales with
       image size so the proxy preview matches the full-res result. Runs AFTER the point pass.
       No-op when EditParams::texture is 0. */
    void Texture(WorkingImage &img, const EditParams &p);

    /* Spatial op (pipeline #7): an APPROXIMATE dehaze (not dark-channel-prior) -- large-radius
       luminance local contrast + a contrast pull about a low pivot (deepens shadows / extends
       range) + a saturation boost, since haze flattens contrast and desaturates. Positive
       removes haze, negative adds it. Runs last. No-op when EditParams::dehaze is 0. */
    void Dehaze(WorkingImage &img, const EditParams &p);

    /* Precomputed once per Apply(); the fused point pass reads only these. active == false
       means no implemented point op would change a pixel, so the pass is skipped entirely. */
    struct PointCoeffs {
        bool  active        = false;
        /* Per-channel scene-linear gain = white balance (temp/tint) folded with exposure (2^EV).
           Both are pure linear multiplies, so they commute and combine into one per-channel
           factor applied before the perceptual tone curve. {1,1,1} = identity. */
        float channelGain[3] = {1.0f, 1.0f, 1.0f};
        float white         = 1.0f;   // linear value that maps to display white

        /* Perceptual-domain tone curve: contrast + the four tone-region controls
           (highlights/shadows/whites/blacks) are all pure 1-D functions of a channel's
           white-normalised value, so they are baked once into a lookup table. The table is
           indexed by the perceptual value s = (v/white)^(1/gamma) in [0, toneLutSMax] and
           returns the white-normalised LINEAR output (i.e. it folds the gamma decode back in),
           so the hot loop does one pow (encode) + one interpolated lookup instead of several
           pow/exp per pixel. toneActive == false => identity tone curve (skip it). */
        static constexpr int kLutSize = 1024;
        bool  toneActive = false;
        float toneLutSMax = 1.0f;          // perceptual s domain the table spans is [0, this]
        float toneLut[kLutSize] = {};      // s -> white-normalised linear output
    };
    static PointCoeffs buildPointCoeffs(const EditParams &p, const WorkingImage &img);

    /* The fused per-pixel pass, parallelised over row ranges. */
    void applyPointOps(WorkingImage &img, const PointCoeffs &c);
};

#endif // DEVELOP_H
