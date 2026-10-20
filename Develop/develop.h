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
    struct StageTimings { qint64 profileMs = 0; qint64 denoiseMs = 0;
                          qint64 pointMs = 0; qint64 textureMs = 0;
                          qint64 clarityMs = 0; qint64 dehazeMs = 0;
                          qint64 vignetteMs = 0; qint64 sharpenMs = 0;
                          qint64 grainMs = 0; qint64 blurMs = 0; };

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

        THE PARAMS ARE OPTIONAL, and are needed for exactly one thing: a CAMERA PROFILE
        carries the white balance inside its matrix, so the matrix depends on the chosen
        temperature. Passing nullptr (or params naming no profile) uses the built-in
        cam.camToWorking . diag(asShotMul), which is what every caller did before profiles
        existed. img.cam.profile is deliberately LEFT SET afterwards -- it is how
        buildPointCoeffs knows the white balance has already been applied and must not be
        applied a second time as a per-channel gain.
    */
    static void ToWorkingSpace(WorkingImage &img, const EditParams *p = nullptr);

    /*
        The camera-native -> working matrix this render should use, and whether it already
        carries the white balance. ONE definition, shared by ToWorkingSpace and by
        buildPointCoeffs's preMat fold, because the two must never disagree about which
        matrix an image went through. False when there is nothing to convert. Includes
        the camera's BaselineExposure as a uniform scale (Develop/baselineexposure.h).
    */
    static bool InputMatrix(const WorkingImage &img, const EditParams *p,
                            float m[3][3], bool &wbIncluded);
    /* InputMatrix without the camera's BaselineExposure -- the colour transform alone. */
    static bool InputMatrixUnexposed(const WorkingImage &img, const EditParams *p,
                                     float m[3][3], bool &wbIncluded);

    /*
        STAGE 0.5 -- the camera profile's HueSatMap, the part of a profile a 3x3 cannot
        carry. Runs immediately after stage 0 and BEFORE everything else, because it
        finishes the input profile: it is a correction to the camera characterisation,
        not an adjustment made on top of one. In particular it precedes Exposure, whose
        uniform scale does not commute with a table indexed by value.

        A no-op unless the image has a profile carrying a HueSatMap, which is why an
        active one forces the early conversion (a table cannot be folded into a matrix).

        It also ATTACHES the profile's look -- LookTable, BaselineExposureOffset,
        ProfileToneCurve -- to img.look for OutputTransform, which applies it after every
        edit, in the DNG SDK's order (see Develop/outputlook.h).
    */
    static void ApplyProfileTables(WorkingImage &img, const EditParams &p);

    /* Whether the profile has a HueSatMap, i.e. per-pixel work at stage 0.5 -- asked
       before stage 0, to choose the route. */
    static bool ProfileTablesActive(const WorkingImage &img, const EditParams &p);

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

    /* The whole-frame statistics sample of img (see StatSampleSlot in workingimage.h):
       ~128 px on the long edge, filled once into the slot img shares with its copies,
       proxies and crops. Call it on the FULL image before cutting a crop, or the crop
       fills the shared slot from itself. Null for an invalid image or a sample. */
    static std::shared_ptr<const WorkingImage> StatSample(const WorkingImage &img);

    /* The frame's LOCAL sample: ~1024 px on the long edge, filled once into the same slot,
       for whole-frame spatial statistics too fine for the StatSample (Highlights -'s
       local Laplacian). Never filled from a crop: call it on the FULL image before
       cutting one. Null for an invalid image, a sample, or a crop of an unfilled slot. */
    static std::shared_ptr<const WorkingImage> LocalSample(const WorkingImage &img);

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

    /* Spatial op (after Sharpen, before Grain): Effects > Blur, the artistic blurs
       (Gaussian / Motion / Spin / Zoom / Tilt-shift / Glow -- Develop/artblur.h). The
       centre and angle must already be in the SOURCE frame (MW::developCompositeStack
       converts the stored, oriented-frame values); distances are fractions of the
       frame's long edge, and positions frame pixels via img.frameX/Y/W/H, so a proxy,
       the settle render and the Detail 1:1 patch agree. No-op when blurAmount is 0. */
    void Blur(WorkingImage &img, const EditParams &p);

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
        /* Calibration's Shadows Tint (Calibrate::shadowTintGains), applied per pixel
           right after the primaries -- it depends on the pixel's luma, so it cannot fold
           into calMat / preMat. 0 = off. */
        float calShadowTint = 0.0f;
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

        /* Colour grading (Color Grade panel), applied after HSL in the same fused pass,
           on the DISPLAY axis the way Lightroom does it (Develop/colorgrade.h has the
           model and its measurement). Per pixel: each channel goes scene-linear ->
           display-linear through the active view transform (gradeFwd), the pixel's L*
           picks the summed push of all four ranges (gradePush: hue + Luminance, with
           Blending / Balance baked in), the push is added in gamma-2.2 ProPhoto (gradeEnc / gradeDec), and the
           CHANGE comes back to scene-linear through gradeInv. Applied as a delta
           (n + inv(v') - inv(v)) so a channel the view has already clipped still moves
           rather than snapping to the clip point. gradeActive == false => identity. */
        static constexpr int kGradeTfN = 2048;   // transfer tables
        static constexpr int kGradeWN  = 1024;   // per-L* push table
        bool  gradeActive = false;
        float gradeNMax   = 8.0f;               // scene n gradeFwd spans (sqrt-spaced)
        float gradeEncMax = 1.5f;               // ProPhoto value gradeEnc spans (sqrt)
        float gradeDecMax = 1.5f;               // encoded value gradeDec spans (linear)
        float gradeFwd[kGradeTfN] = {};         // n -> display-linear v
        float gradeInv[kGradeTfN] = {};         // v (sqrt-spaced on 0..1) -> n
        float gradeEnc[kGradeTfN] = {};         // P -> P^(1/2.2)
        float gradeDec[kGradeTfN] = {};         // e -> e^2.2
        float gradePush[kGradeWN][3] = {};      // by sqrt(Y): summed encoded push

        /* Point Color (Develop/pointcolor.h), applied after HSL and BEFORE Color Grade
           on the same display axis: it reuses gradeFwd / gradeInv, which are built when
           either is active. Per pixel: display-linear -> OKLab (pcToLms folds the
           working -> sRGB gamut change into OKLab's M1), each point's weighted change
           summed, back through pcFromLms, and the CHANGE carried to scene-linear.
           pcVisualize >= 0 greys every pixel by how little that point selects it. */
        bool  pcActive = false;
        int   pcN = 0;
        int   pcVisualize = -1;
        PointColor::Coef pc[PointColor::kMaxPts];
        float pcToLms[9] = {};
        float pcFromLms[9] = {};

        /* Highlights - is LOCAL (see the note by kHighsLocalSigma in develop.cpp).
           hiLocal says the tables below are built. Given a region-correction field
           (applyPointOps' hiField) the fused pass does not use toneLut: it takes each
           channel to the DISPLAY axis without the band (hiDispLut, by s), adds the
           band's own shift (hiShiftLut, by s) plus the field's correction for where the
           pixel sits, and comes back through hiBackLut[ch] (display -> white-normalised
           linear, Curves included). The field is built and read on the band's own axis:
           hiPosLut is d0 by s, hiShiftByPos the band's shift by d0 over [0, 1]. With no
           field toneLut, which always holds the full per-pixel curve, is used. */
        static constexpr int kBackSize = 4096;
        bool  hiLocal = false;
        float hiDispLut[kLutSize] = {};
        float hiShiftLut[kLutSize] = {};
        float hiPosLut[kLutSize] = {};
        float hiBackLut[3][kBackSize] = {};
        float hiShiftByPos[kBackSize] = {};
        /* The frame-relative term (see the note by kHighsAnchorK in develop.cpp): a gain
           in display-linear light by the pixel's LUMINANCE s (hiGainByS, 1 = none; the
           per-pixel fallback when there is no field), with the two tables that take a
           channel's display value to display-linear light and back (hiLinOfD by d;
           hiDOfRootLin by sqrt of linear, for precision near black). */
        float hiGainByS[kLutSize] = {};
        float hiLinOfD[kBackSize] = {};
        float hiDOfRootLin[kBackSize] = {};
        /* The same term on its own axis, for the region field: L = log2 of the band-free
           display value in display-linear light, by s (hiLogLumLut), and the stops it
           loses, by L over [kHiLMin, 0] (hiDropByL). */
        static constexpr float kHiLMin = -16.0f;
        float hiLogLumLut[kLutSize] = {};
        float hiDropByL[kBackSize] = {};
    };

    /* The local Highlights - correction on a whole-frame grid: correction = a * d0 + b
       (the guided upsample of a local Laplacian run on the frame's LocalSample), read
       bilinearly per pixel through WorkingImage::frameX.. so a crop reads it in place. */
    struct HiField {
        int w = 0, h = 0;
        std::vector<float> a, b;      // the band's region correction, by d0
        std::vector<float> ga, gb;    // the frame-relative term's log2 gain, by L
    };
    static std::shared_ptr<const HiField> buildHiField(const WorkingImage &local,
                                                       const EditParams &p,
                                                       const PointCoeffs &c);
    /* hiWeight: in/out. The mean Highlights band weight over the WHOLE FRAME, which sets
       the band's strength. Pass the value the frame's StatSample measured, or < 0 to
       measure it on img itself (then written back). */
    static std::shared_ptr<const PointCoeffs> buildPointCoeffs(const EditParams &p,
                                                               const WorkingImage &img,
                                                               float *hiWeight = nullptr);

    /* What the last Apply measured into hiWeight (< 0: not needed). Read by the Apply
       that developed this frame's sample, for the image itself. */
    float lastHiWeight = -1.0f;

    /* The point coefficients the last Apply of a StatSample built. They depend on the
       params, the metadata and hiWeight -- never on the pixels otherwise -- and the
       sample shares all three with its image, so the image's Apply reuses them instead
       of building the same tone table a second time. */
    std::shared_ptr<const PointCoeffs> lastCoeffs;

    /* The fused per-pixel pass, parallelised over row ranges. hiField: the local
       Highlights - correction (null = per pixel, through toneLut). */
    void applyPointOps(WorkingImage &img, const PointCoeffs &c,
                       const HiField *hiField = nullptr);
};

#endif // DEVELOP_H
