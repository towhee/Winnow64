#ifndef DEVELOP_H
#define DEVELOP_H

#include "Develop/editparams.h"
#include "Develop/workingimage.h"
#include <QtGlobal>

/*
    Applies parametric develop adjustments to a WorkingImage in place. Stateless and
    reentrant, constructed per decode (same discipline as Demosaic / RawFormat) so it
    carries no cross-thread state.

    The operation order is fixed and hard-coded (Lightroom-like); order matters and is not a
    caller concern. Ops are split by cost (see notes/Documentation.txt "Scope & masking
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

    /* Per-stage wall-clock timings for one Apply(), filled when a non-null pointer is passed.
       A latency probe only (Develop preview [DevTime] logging); pass nullptr in normal use. */
    struct StageTimings { qint64 denoiseMs = 0; qint64 pointMs = 0; qint64 textureMs = 0;
                          qint64 clarityMs = 0; qint64 dehazeMs = 0;
                          qint64 vignetteMs = 0; qint64 sharpenMs = 0;
                          qint64 grainMs = 0; };

    /* Apply p to img in place. Returns true on success (and trivially when p is identity,
       leaving img untouched). Fills *t when non-null. */
    bool Apply(WorkingImage &img, const EditParams &p, StageTimings *t = nullptr);

    /*
        STAGE 0 -- camera-native primaries -> the working space, in place.

        A raw decode stops at the sensor's own primaries (RawColor::ToCameraNative), so
        that changing a camera profile is a re-render off the WorkingImageCache rather
        than a re-decode. This is the step that finishes the job: it applies the as-shot
        multipliers and cam.camToWorking, then re-tags img.space.

        MOST RENDERS NEVER CALL IT. buildPointCoeffs folds the same transform into
        PointCoeffs::preMat, where it costs nothing because the fused pass already
        touches every pixel. This standalone pass exists for the two cases that cannot
        wait for the point pass: an identity edit (which has no point pass at all), and
        an active Denoise (op #1, which runs BEFORE the point pass and must see
        working-space pixels so its results do not depend on the sensor's primaries).

        A no-op unless img.space is CameraNative, so calling it twice is safe.
    */
    static void ToWorkingSpace(WorkingImage &img);

    /* Blend a full-strength raw-denoised image toward the clean one, per the Global "Denoise raw"
       amounts (the interactive slider blend for the PMRID pre-demosaic denoiser -- see
       MW::ensureRawDenoise). The (denoised - clean) correction is split, in scene-linear RGB, into
       a luma term (scaled by lum) and a per-channel chroma term (scaled by max(lum,chr)), so
       highlights (where the correction is ~0) pass through. Writes into out (set to clean when the
       dimensions mismatch or both amounts are 0). Static: carries no develop state. */
    static void BlendRawDenoise(const WorkingImage &clean, const WorkingImage &denoised,
                                float lum, float chr, WorkingImage &out);

    /* The PARAMETRIC part of the tone curve -- contrast plus the four tone-region lifts,
       shaped by the tone splits -- sampled as a perceptual s -> s' mapping over [0,1]
       into
       out[0..n-1]. The Curves panel's Parametric view draws this, and it deliberately
       lives here rather than in the widget so the drawn curve can never drift from the
       one buildPointCoeffs actually bakes (they share this file's tuning constants). The
       Curves panel's own point curves are NOT included: the view layers them separately.
    */
    static void ParametricCurve(const EditParams &p, float *out, int n);

private:
    /* Spatial op: local (maskable) NR, owns a full-image pass, run BEFORE the fused point pass
       (fixed pipeline order). Two independent strengths: EditParams::localDenoiseLuma = luminance
       NR (ratio-preserving, chroma untouched); EditParams::localDenoiseChroma = colour/chroma NR
       (downscaled opponent-chroma blur, luminance kept exact). Luma runs first, then chroma on the
       result. This is the GLOBAL (no-mask) case; the planned scope compositor calls it per scope,
       bounded to the mask bbox and blended by the scope's alpha (see notes/Documentation.txt
       "Scope & masking model"). No-op when both strengths are 0. */
    void Denoise(WorkingImage &img, const EditParams &p);

    /* Spatial op (pipeline #6): mid-frequency local contrast on LUMINANCE only (ratio-preserving,
       like Denoise), in the perceptual domain. A Gaussian base isolates a mid-frequency detail
       band; positive texture amplifies it, negative attenuates it. The base radius scales with
       image size so the proxy preview matches the full-res result. Runs AFTER the point pass.
       No-op when EditParams::texture is 0. */
    void Texture(WorkingImage &img, const EditParams &p);

    /* Spatial op (pipeline #6.5): mid-radius luminance local contrast, between Texture
       (~13px) and Dehaze (~170px) on an 8640px edge, so the three occupy separate bands
       and stack. Positive adds midtone punch, negative gives a soft glow. What makes it
       more than "Texture with a bigger radius": the band is weighted toward the MIDTONES
       (it will not crush blacks or flatten highlights) and rolled off at strong edges
       (where a band this wide would ring). Ratio-preserving on luminance like the rest;
       shares gaussianBase with Texture. Math in Develop/localcontrast.h. No-op at 0. */
    void Clarity(WorkingImage &img, const EditParams &p);

    /* Spatial op (pipeline #7): an APPROXIMATE dehaze (not dark-channel-prior) -- large-radius
       luminance local contrast + a contrast pull about a low pivot (deepens shadows / extends
       range) + a saturation boost, since haze flattens contrast and desaturates. Positive
       removes haze, negative adds it. No-op when EditParams::dehaze is 0. */
    void Dehaze(WorkingImage &img, const EditParams &p);

    /* Spatial op (pipeline #8, runs LAST): a radial exposure vignette about the image
       centre. vignetteExposure is the EV applied at the corners (negative darkens = the
       classic vignette, positive brightens), ramping smoothly to 0 at the centre;
       vignetteFeather (0..1) shapes the falloff (high = gradual/reaches inward, low =
       concentrated in the corners). Custom / off-centre vignettes are done with radial
       masks, so this global op is just the two sliders. No-op when the EV is 0. */
    void Vignette(WorkingImage &img, const EditParams &p);

    /* Spatial op (pipeline #8.5, between Vignette and Grain): capture sharpening, an
       unsharp mask on perceptual luminance, ratio-preserving like Texture/Denoise. Placed
       after Vignette so darkened corners are not amplitude-boosted, and before Grain so
       film grain is never itself sharpened.

       THE ONE SCALE-DEPENDENT OP. Every other spatial op scales its radius to max(w,h) so
       the proxy matches full res; acutance belongs to the pixel grid, so sharpenRadius is
       an ABSOLUTE pixel radius and this op reads WorkingImage::renderScale to shrink it
       for a proxy. The proxy preview is therefore scale-honest but only true at 1:1.
       Distinct from Texture, a resolution-proportional mid-frequency band -- the two
       occupy disjoint bands and stack. Math in Develop/sharpen.h. No-op at amount 0. */
    void Sharpen(WorkingImage &img, const EditParams &p);

    /* Spatial op (pipeline #9, runs LAST -- after Sharpen): monochromatic film grain
       added to luminance (ratio-preserving, like Texture/Denoise). Deterministic
       (fixed-seed noise) so a re-render does not make the grain shimmer, and the particle
       size scales with the image so the proxy preview matches full res. grainAmount is
       the strength, grainSize the particle size, grainRoughness the amplitude
       irregularity. No-op when grainAmount is 0. */
    void Grain(WorkingImage &img, const EditParams &p);

    /* Precomputed once per Apply(); the fused point pass reads only these. active == false
       means no implemented point op would change a pixel, so the pass is skipped entirely. */
    struct PointCoeffs {
        bool  active        = false;
        /* Per-channel scene-linear gain = white balance (temp/tint) folded with exposure (2^EV)
           AND the Colour RGB sliders (red/green/blue). All are pure linear multiplies, so they
           commute and combine into one per-channel factor applied before the perceptual tone
           curve. {1,1,1} = identity. */
        float channelGain[3] = {1.0f, 1.0f, 1.0f};
        float white         = 1.0f;   // linear value that maps to display white

        /* Perceptual-domain tone curve: contrast, the four tone-region controls
           (highlights/shadows/whites/blacks) and the Curves panel's tone curve are all
           pure 1-D functions of a channel's white-normalised value, so they are baked
           once into a lookup table. The table is indexed by the perceptual value
           s = (v/white)^(1/gamma) in [0, toneLutSMax] and returns the white-normalised
           LINEAR output (i.e. it folds the gamma decode back in), so the hot loop does
           one pow (encode) + one interpolated lookup instead of several pow/exp per
           pixel.
           toneActive == false => identity tone curve (skip it).

           THREE tables, one per channel, because the Curves panel carries separate Red /
           Green / Blue curves on top of its RGB composite. Everything shared -- contrast,
           the region lifts, the composite curve -- is computed once and written into all
           three, so the extra cost is the table build, not the hot loop (which does the
           same single lookup either way, just into toneLut[ch]). */
        static constexpr int kLutSize = 1024;
        bool  toneActive = false;
        float toneLutSMax = 1.0f;          // perceptual s domain the table spans is [0, this]
        /* s -> white-normalised linear output, one table per channel. */
        float toneLut[3][kLutSize] = {};

        /* Calibration (Calibrate panel) -- a 3x3 matrix (row-major) re-pointing the R/G/B
           primaries, applied in LINEAR light right after channelGain and BEFORE the tone
           curve. Its columns are the rotated/chroma-scaled primaries, built so neutrals
           are fixed. calActive == false => identity (skip the block). */
        bool  calActive = false;
        float calMat[9] = {1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f};

        /*
            THE FUSED FRONT MATRIX -- the input profile, the per-channel gains and the
            calibration matrix collapsed into ONE 3x3 (row-major).

            A raw arrives in CAMERA-NATIVE primaries (RawColor no longer converts it, so
            that a profile change is a re-render off the cache rather than a re-decode).
            The front of the pipeline is then a run of linear stages:

                calMat . diag(channelGain) . camToWorking . diag(asShotMul)

            Every one of those is a matrix or a diagonal, so the product is a single 3x3
            computed once here. The hot loop does 9 multiplies instead of a matrix, three
            gains and a second matrix -- fewer than TODAY's per-pixel cost, even though it
            now does the colour conversion the decoder used to do.

            When this is active, channelGain and calMat have been folded in and must NOT
            be applied again. It is inactive for input that is already in the working
            space (a JPEG), which keeps that path on the cheap diagonal-only gain.
        */
        bool  preMatActive = false;
        float preMat[9] = {1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f};

        /* Set when the CALIBRATE matrix was one of the stages folded in. The standalone
           calibrate block clamps its output to >= 0, so the fold has to clamp too or the
           two routes disagree -- and which route runs depends on whether Denoise is on,
           which would make the denoise slider appear to shift colour. Only set when
           calibrate actually folded: with no calibration there is no clamp in the
           sequence either, and inventing one would throw away the out-of-gamut sensor
           values the decode deliberately stopped clamping. */
        bool  preMatClamps = false;

        /* HSL (hue/saturation/luminance) -- a cross-channel point op applied AFTER the
           tone curve in the same fused pass (it mixes the three channels, so unlike the
           tone curve it cannot be a per-channel LUT). hueMat is a 3x3 rotation about the
           neutral axis (row-major, used only when hue != 0); satFactor scales chroma
           about luma; vibAmount is a per-pixel saturation boost weighted by how muted the
           pixel already is (0 = off); lumGain is a uniform gain. hslActive == false =>
           identity (skip the block). */
        bool  hslActive  = false;
        float hueMat[9]  = {1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f};
        float satFactor  = 1.0f;
        float vibAmount  = 0.0f;
        float lumGain    = 1.0f;

        /* Colour grading (Color Grade panel) -- tonal-range tinting applied after HSL in
           the same fused pass. For each of the four ranges [0]=shadows, [1]=midtones,
           [2]=highlights, [3]=GLOBAL: gradeTint is a zero-luma RGB chroma push (hue+sat
           pre-scaled) ADDED to the pixel, gradeLum a per-range luminance gain delta.
           Ranges 0-2 are weighted per pixel by smooth tonal windows of the pixel's luma;
           range 3 is NOT tone-selective and applies at weight 1 everywhere, so it needs
           no window (see applyPointOps). gradeShadowEnd / gradeHighStart are those
           windows' split points, derived once from the panel's Blending + Balance
           sliders. gradeActive == false => identity (skip the block). */
        bool  gradeActive = false;
        float gradeTint[4][3] = {};   // [range][rgb], zero-luma chroma offset
        float gradeLum[4]     = {};   // [range] luminance gain delta (0 = none)
        float gradeShadowEnd  = 0.5f; // perceptual L where the shadow window closes
        float gradeHighStart  = 0.5f; // perceptual L where the highlight window opens
    };
    static PointCoeffs buildPointCoeffs(const EditParams &p, const WorkingImage &img);

    /* The fused per-pixel pass, parallelised over row ranges. */
    void applyPointOps(WorkingImage &img, const PointCoeffs &c);
};

#endif // DEVELOP_H
