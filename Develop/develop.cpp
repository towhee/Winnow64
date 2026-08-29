#include "Develop/develop.h"
#include "Develop/colorspace.h"
#include "Develop/whitebalance.h"
#include "Develop/calibrate.h"
#include "Develop/colorgrade.h"
#include "Develop/tonecurve.h"
#include "Develop/sharpen.h"
#include "Develop/localcontrast.h"
#include <QtConcurrent>
#include <QThreadPool>
#include <QElapsedTimer>
#include <QFuture>
#include <QVector>
#include <QtGlobal>
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <algorithm>
#include <cmath>
#include <vector>

/* Luma weights for the WORKING space, folded at compile time from
   ColorSpaceMath::kWorking. These used to be the literals 0.2126 / 0.7152 / 0.0722
   written out at every site; they now follow the working space automatically, so
   widening it does not silently leave the ops weighting the wrong primaries. */
using ColorSpaceMath::kLumR;
using ColorSpaceMath::kLumG;
using ColorSpaceMath::kLumB;

namespace {
/* Contrast is applied in a perceptual (~sRGB gamma) domain so the slope steepens the tone
   curve evenly instead of exploding scene-linear highlights. kGamma/kInvGamma encode/decode;
   kContrastSlopeRange keeps the control gentle (full slider = 1 +/- 0.4 slope). */
constexpr float kGamma             = 2.2f;
constexpr float kInvGamma          = 1.0f / kGamma;
constexpr float kContrastSlopeRange = 0.4f;
/* The contrast slider is an integer -100..100 control (see DevelopProperties::addBasic),
   so normalise to the -1..1 "amount" the slope math is designed around. */
constexpr float kContrastFullScale  = 100.0f;

/* White balance is a per-channel gain in SCENE-LINEAR (the physically correct domain: it
   rescales the light before any tone curve). The gains are ABSOLUTE: p.temp is a Kelvin
   temperature and p.tint a green/magenta offset, resolved against the image's own colour
   characterisation (WorkingImage::cam) by WhiteBalance::relativeGains -- so "5500 K"
   means the same light on every camera body. See Develop/whitebalance.h. */

/* Colour -- RGB sliders (red/green/blue). Per-channel scene-linear gain, normalised -1..1 by
   kRgbFullScale and folded into the same per-channel factor as WB + exposure (free; no extra
   pass). Gentle creative range: full slider = +/-50% on that channel. */
constexpr float kRgbFullScale = 100.0f;
constexpr float kRgbGain      = 0.5f;

/* Colour -- HSL sliders (hue/saturation/luminance), normalised -1..1 by kHslFullScale. Hue is a
   rotation about the neutral (1,1,1) axis: full slider = +/-kHueMaxRad. Saturation scales chroma
   about Rec.709 luma: -1 -> grayscale, +1 -> 2x. Luminance is a uniform gain: -1 -> black,
   +1 -> 2x. Applied as a cross-channel block after the tone curve (see applyPointOps). */
constexpr float kHslFullScale = 100.0f;
constexpr float kHueMaxRad    = 1.04719755f;   // 60 degrees at full slider

/* Colour grading (Color Grade panel) -- tonal-range tinting. Each range's hue+sat becomes
   a zero-luma RGB chroma push scaled by kGradeTintStrength (added at full sat), and
   its -100..100 luminance a gain delta scaled by kGradeLumStrength (+/-100 -> +/-50% in
   that range). Weighted per pixel by smooth tonal windows split at kGradeShadowEnd /
   kGradeHighStart in the perceptual (gamma) domain. See buildPointCoeffs' grade block. */
constexpr float kGradeTintStrength = 0.5f;
constexpr float kGradeLumStrength  = 0.5f;
constexpr float kGradeLumFullScale = 100.0f;
constexpr float kGradeShadowEnd    = 0.5f;    // perceptual L: shadow weight reaches 0
constexpr float kGradeHighStart    = 0.5f;    // perceptual L: highlight weight starts

/* How far the Blending and Balance sliders may move those two split points. Blending
   pushes them apart / together about the base; Balance slides both the same way. The
   DEFAULTS (blending 50, balance 0) must land exactly on the two constants above, or
   every already-graded image would shift the first time it is reopened. */
constexpr float kGradeBlendRange   = 0.30f;
constexpr float kGradeBalanceRange = 0.20f;
constexpr float kGradeSplitMin     = 0.05f;   // keep both strictly inside (0,1): the
constexpr float kGradeSplitMax     = 0.95f;   // weights divide by them

/* Tone-region controls (highlights/shadows/whites/blacks). Each is a smooth, region-weighted
   shift of the PERCEPTUAL (gamma) value, applied after contrast (pipeline order #5). The
   weight is a Gaussian centred on the region's perceptual position; the four overlap gently so
   the curve stays smooth. Blacks/whites are pinned at 0/1; the shadows and highlights centres
   (and each one's reach, via sigma) come from the per-image tone-split params set by the
   histogram region slider (see buildToneLut). Sliders are integer -100..100 (normalised to
   +/-1 by kToneFullScale); positive always brightens (lifts) the region, negative darkens
   (recovers), like Lightroom. kToneShift is the max perceptual lift at full slider. The table
   is built over up to ~3 stops above white (kToneLutMaxN) so highlight headroom is shaped. */
constexpr float kToneFullScale    = 100.0f;
constexpr float kToneShift        = 0.20f;
constexpr float kToneSigma        = 0.18f;
/* Blacks and whites stay pinned at the ends; shadows/highlights centres are per-image now
   (EditParams tone splits, set by the histogram region slider). */
constexpr float kBlacksCenter     = 0.00f;
constexpr float kWhitesCenter     = 1.00f;
constexpr float kToneLutMaxN      = 8.0f;   // build the curve over linear n in [0, 8] (~3 stops)

inline float toneWeight(float s, float center, float sigma)
{
    const float d = s - center;
    return std::exp(-(d * d) / (2.0f * sigma * sigma));
}

/* The region slider's crossover sets each movable region's reach: half-width = gap to the
   crossover, scaled so the default gap (0.25) reproduces kToneSigma (0.18). kToneSigmaMin keeps
   a very narrow region from spiking. */
constexpr float kToneSigmaSpan = kToneSigma / 0.25f;   // 0.72
constexpr float kToneSigmaMin  = 0.05f;

/* The PARAMETRIC tone shape -- the contrast slope plus the four Gaussian region lifts --
   resolved from the params once. Shared by Develop::buildPointCoeffs (which bakes it into
   the tone LUT) and Develop::ParametricCurve (which the Curves panel's Parametric view
   draws), so the curve the user sees and the curve that actually renders cannot diverge.
   Everything here is in the PERCEPTUAL (gamma) domain. */
struct ToneShape {
    float pivot = 0.0f, slope = 1.0f;
    float hi = 0.0f, sh = 0.0f, wh = 0.0f, bk = 0.0f;
    float shC = 0.25f, hiC = 0.75f;
    float shSig = kToneSigma, hiSig = kToneSigma;
    bool  active = false;
};

inline ToneShape buildToneShape(const EditParams &p)
{
    ToneShape t;
    t.pivot = std::pow(0.18f, kInvGamma);
    t.slope = (p.contrast != 0.0f)
                  ? 1.0f + kContrastSlopeRange * (p.contrast / kContrastFullScale)
                  : 1.0f;
    t.hi = p.highlights / kToneFullScale;     // -1..1, + brightens
    t.sh = p.shadows    / kToneFullScale;
    t.wh = p.whites     / kToneFullScale;
    t.bk = p.blacks     / kToneFullScale;
    /* Region slider: shadows/highlights centres move with the handles; blacks (0) and
       whites (1) stay pinned. The crossover sets each movable region's reach (sigma).
       Defaults 0.25/0.50/0.75 give the old fixed centres and kToneSigma, so this is a
       no-op until moved. Clamped defensively so a hand-edited / corrupt sidecar can never
       invert the order (qBound requires min <= max): shC <= 0.94 leaves room for hiC,
       which sits at least 0.04 above it, and the crossover stays strictly between. */
    t.shC = qBound(0.02f, p.toneShadowCenter, 0.94f);
    t.hiC = qBound(t.shC + 0.04f, p.toneHighlightCenter, 0.98f);
    const float crX = qBound(t.shC + 0.01f, p.toneCrossover, t.hiC - 0.01f);
    t.shSig = std::max(kToneSigmaMin, (crX - t.shC) * kToneSigmaSpan);
    t.hiSig = std::max(kToneSigmaMin, (t.hiC - crX) * kToneSigmaSpan);
    t.active = (t.slope != 1.0f) || (t.hi != 0.0f) || (t.sh != 0.0f) ||
               (t.wh != 0.0f) || (t.bk != 0.0f);
    return t;
}

/* Contrast slope about mid-grey, then the four region shifts. */
inline float applyToneShape(const ToneShape &t, float s)
{
    s = t.pivot + (s - t.pivot) * t.slope;
    const float lift = t.hi * toneWeight(s, t.hiC, t.hiSig)
                     + t.sh * toneWeight(s, t.shC, t.shSig)
                     + t.wh * toneWeight(s, kWhitesCenter, kToneSigma)
                     + t.bk * toneWeight(s, kBlacksCenter, kToneSigma);
    return s + kToneShift * lift;
}

/* Texture (spatial op): mid-frequency luminance local contrast. The base-blur radius is a
   fraction of the image's longer edge so the proxy and full-res renders shape the same band;
   kTextureGain sets how hard a positive slider amplifies the detail (negative smooths it). */
constexpr float kTextureFullScale = 100.0f;
constexpr float kTextureSigmaFrac = 0.0015f;   // ~13px on a 8640px edge
constexpr float kTextureGain      = 1.5f;      // amount=+1 -> detail *2.5

/* Clarity (spatial op #6.5): mid-radius luminance local contrast, between Texture's fine
   detail band and Dehaze's atmospheric one. Two things keep it from being "Texture with a
   bigger number": the effect is weighted toward the MIDTONES (so it cannot crush blacks
   or flatten highlights the way a flat wide band does) and rolls off at strong edges,
   where a band this wide rings. Gentler gain than Texture because a wider band reads
   much stronger per unit. See Develop/localcontrast.h. */
constexpr float kClarityFullScale = 100.0f;
constexpr float kClaritySigmaFrac = 0.007f;    // ~60px on a 8640px edge
constexpr float kClarityGain      = 0.8f;      // vs kTextureGain 1.5, at 5x the radius
constexpr float kClarityHaloKnee  = 0.15f;     // excursion counting as a hard edge

/* Dehaze (spatial op, APPROXIMATION): a large-radius luminance local-contrast boost, a contrast
   pull about a low pivot (deepens shadows, the hallmark of clearing haze), and a saturation
   lift. Radius is a larger fraction of the image; the box blur (cv::blur) is used because its
   running-sum cost is ~independent of radius, keeping the big base blur cheap at full res. */
constexpr float kDehazeFullScale  = 100.0f;
constexpr float kDehazeSigmaFrac  = 0.02f;     // ~170px on a 8640px edge
constexpr float kDehazeLocalGain  = 1.0f;      // local-contrast strength
constexpr float kDehazeContrast   = 0.3f;      // contrast about the low pivot
constexpr float kDehazePivot      = 0.3f;      // perceptual pivot (below mid-grey)
constexpr float kDehazeSat        = 0.3f;      // saturation lift at full slider

/* Sharpen (#8.5) has NO tuning constants here. Its amount is stored pre-scaled (0..1.5,
   like grainAmount / localDenoise*, the panel applies the divisor) and, uniquely, its
   radius is ABSOLUTE pixels rather than a fraction of max(w,h) -- so the sigma and the
   detail/mask knees all come from Develop/sharpen.h, scaled by WorkingImage::render-
   Scale instead of by image size. See the Sharpen() comment. */

/* Vignette (spatial op #8): a radial exposure falloff about the image centre. The mask is
   pow(rn, k) where rn is the elliptical radius normalised to 1 at the corners, so it is 0
   at the centre and 1 at the corners; feather maps to the exponent k (high feather -> low
   k -> gradual/reaches inward; low feather -> high k -> effect hugs the corners). The
   corner gain is 2^vignetteExposure, scaled by the mask so the centre is untouched. */
constexpr float kVignetteFeatherKMin = 0.5f;   // exponent at feather = 1 (gradual)
constexpr float kVignetteFeatherKMax = 5.0f;   // exponent at feather = 0 (corner-hugging)

/* Grain (spatial op #9): monochromatic film grain added to perceptual luminance, most
   visible in the midtones. The noise is generated at a REDUCED resolution (a grain-cell
   grid) and bilinearly upsampled, so the particle size is set by the cell size and the
   cost stays low; the cell scales with the long edge (kGrainCellUnit) so the proxy and
   full-res render show proportionally the same grain. grainSize maps the cell from fine
   to coarse (kGrainCellMin..Max units); grainRoughness modulates the grain amplitude with
   a second, lower-frequency field (kGrainRoughFreqDiv) so it is patchy, not uniform.
   kGrainStrength is the max perceptual std at full amount. A fixed RNG seed keeps the
   pattern stable across re-renders (no shimmer). */
constexpr float  kGrainStrength     = 0.12f;   // perceptual std of grain at amount = 1
constexpr double kGrainCellUnit     = 1500.0;  // long-edge px per grain "unit"
constexpr double kGrainCellMin      = 0.75;    // grain cell (units) at size = 0 (fine)
constexpr double kGrainCellMax      = 3.75;    // grain cell (units) at size = 1 (coarse)
constexpr int    kGrainRoughFreqDiv = 4;       // roughness field: lower frequency factor
constexpr uint64_t kGrainSeed = 0x9E3779B97F4A7C15ULL;   // fixed: grain must not shimmer

/* Run fn over the pixel-index range [0, n) split into chunks across the global pool (the same
   data-parallel idiom as Develop::applyPointOps / OutputTransform::ToImage). fn(i0, i1) must
   process a disjoint half-open slice; chunks are pixel-aligned so callers that index rgb[i*3+c],
   yp[i], etc. stay race-free. Falls back to a single in-line call when the pool is one thread or
   the work is small. */
template <class F>
inline void parallelFor(size_t n, F fn)
{
    const int maxThreads = qMax(1, QThreadPool::globalInstance()->maxThreadCount());
    const size_t kMinPerChunk = 1u << 16;   // ~65k pixels: keep chunks coarse vs. dispatch cost
    if (maxThreads == 1 || n < kMinPerChunk * 2) { fn(static_cast<size_t>(0), n); return; }

    const int chunks = static_cast<int>(qMin<size_t>(maxThreads, (n + kMinPerChunk - 1) / kMinPerChunk));
    const size_t per = (n + chunks - 1) / chunks;
    QVector<QFuture<void>> futures;
    futures.reserve(chunks);
    for (int k = 0; k < chunks; ++k) {
        const size_t i0 = static_cast<size_t>(k) * per;
        const size_t i1 = qMin(n, i0 + per);
        if (i0 >= i1) break;
        futures.append(QtConcurrent::run(QThreadPool::globalInstance(),
                                         [fn, i0, i1]() { fn(i0, i1); }));
    }
    for (QFuture<void> &f : futures) f.waitForFinished();
}

/* ------------------------------------------------------------------------------------
   Shared perceptual-luminance band pass

   Texture (#6), Clarity (#6.5), Dehaze (#7) and Sharpen (#8.5) all do the same three
   things: lift a perceptual luminance off the RGB, reshape it against a blurred base,
   and fold the result back as a RATIO on RGB so hue and chroma are untouched. Only the
   middle step differs. These two helpers own the first and third, so each op is just its
   own blur plus its own shape function -- see Develop/localcontrast.h for the shared
   scalar math and tests/unit/tst_localcontrast.cpp for the characterization gate that
   pins what Texture and Dehaze rendered before this was extracted.
   ------------------------------------------------------------------------------------ */

/* Perceptual, white-normalised luminance into a blurrable cv::Mat, plus the scene-linear
   luma kept alongside for the ratio fold-back. */
inline void buildPerceptualLuma(const WorkingImage &img, cv::Mat &Yp,
                                std::vector<float> &Ylin)
{
    const int w = img.width, h = img.height;
    const size_t n = static_cast<size_t>(w) * static_cast<size_t>(h);
    const float white = (img.white > 0.0f) ? img.white : 1.0f;
    const float invWhite = 1.0f / white;
    const float *rgb = img.rgb.data();

    Yp.create(h, w, CV_32FC1);
    Ylin.resize(n);
    float *yp = Yp.ptr<float>();
    float *ylin = Ylin.data();
    parallelFor(n, [=](size_t i0, size_t i1) {
        for (size_t i = i0; i < i1; ++i) {
            const float r = rgb[i * 3 + 0], g = rgb[i * 3 + 1], b = rgb[i * 3 + 2];
            const float Y = kLumR * r + kLumG * g + kLumB * b;
            ylin[i] = Y;
            float nrm = Y * invWhite;
            if (nrm < 0.0f) nrm = 0.0f;
            yp[i] = std::pow(nrm, kInvGamma);
        }
    });
}

/* Gaussian base for a band op, with the large-sigma downscale trick.

   sigmaFrac is a fraction of the image's LONGER EDGE, so the proxy and the full-res
   settle render shape the same relative band (the invariant every spatial op but Sharpen
   holds). The base is a smooth low-frequency signal, so for a large sigma the costly
   Gaussian runs on a DOWNSCALED luminance image and is upsampled -- visually identical to
   a full-res blur, but the cost drops ~kDown^2, and GaussianBlur is the dominant cost of
   these ops at full resolution. The downscale targets a well-sampled small-image working
   sigma (~3px) and is capped so the base stays smooth; a sigma <= ~3 (e.g. on the proxy)
   blurs at full size. Shared by Texture (#6) and Clarity (#6.5); Dehaze uses a box blur
   instead because at its much larger radius a running-sum blur is cheaper still. */
inline cv::Mat gaussianBase(const cv::Mat &Yp, float sigmaFrac, int w, int h)
{
    const double sigma = qMax(1.0, static_cast<double>(sigmaFrac) * qMax(w, h));
    const int kDown = qBound(1, static_cast<int>(sigma / 3.0), 4);
    cv::Mat base;
    if (kDown > 1 && w >= kDown * 2 && h >= kDown * 2) {
        cv::Mat small;
        cv::resize(Yp, small, cv::Size(w / kDown, h / kDown), 0, 0, cv::INTER_AREA);
        cv::GaussianBlur(small, small, cv::Size(0, 0), sigma / kDown);
        cv::resize(small, base, cv::Size(w, h), 0, 0, cv::INTER_LINEAR);
    } else {
        cv::GaussianBlur(Yp, base, cv::Size(0, 0), sigma);
    }
    return base;
}

/* Reshape the perceptual luma and fold it back onto RGB as a ratio.

   shape(i, yp[i]) returns the new perceptual luma for pixel i; the op's own lambda closes
   over its blurred base. TEMPLATED ON PURPOSE: the shape inlines into this loop, so the
   op still costs ONE parallel pass and no extra buffer, exactly as the hand-written
   versions did. Pixels at or below black are skipped so true black stays black (a ratio
   is meaningless there). */
template <class Shape>
inline void shapeAndFoldLuma(WorkingImage &img, const float *yp, const float *ylin,
                             Shape shape)
{
    const size_t n = static_cast<size_t>(img.width) * static_cast<size_t>(img.height);
    const float white = (img.white > 0.0f) ? img.white : 1.0f;
    float *rgb = img.rgb.data();
    constexpr float kEps = 1e-6f;
    parallelFor(n, [=](size_t i0, size_t i1) {
        for (size_t i = i0; i < i1; ++i) {
            const float Y = ylin[i];
            if (Y <= kEps) continue;
            float s = shape(i, yp[i]);
            if (s < 0.0f) s = 0.0f;
            const float Yd = std::pow(s, kGamma) * white;
            const float r = Yd / Y;
            rgb[i * 3 + 0] *= r;
            rgb[i * 3 + 1] *= r;
            rgb[i * 3 + 2] *= r;
        }
    });
}
}

void Develop::ToWorkingSpace(WorkingImage &img)
{
    if (img.space != ColorSpaceMath::ColorSpace::CameraNative) return;
    if (!img.isValid()) return;

    /* camToWorking . diag(asShotMul), the same product buildPointCoeffs folds into
       preMat. An invalid characterisation leaves the pixels alone and just re-tags:
       an unknown camera renders approximately rather than not at all. */
    float m[3][3] = {{1,0,0}, {0,1,0}, {0,0,1}};
    if (img.cam.valid) {
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 3; ++j)
                m[i][j] = img.cam.camToWorking[i][j] * img.cam.asShotMul[j];

        float *rgb = img.rgb.data();
        const size_t n = static_cast<size_t>(img.width) * static_cast<size_t>(img.height);
        parallelFor(n, [=](size_t i0, size_t i1) {
            for (size_t i = i0; i < i1; ++i) {
                float *px = rgb + i * 3;
                const float r = px[0], g = px[1], b = px[2];
                px[0] = m[0][0] * r + m[0][1] * g + m[0][2] * b;
                px[1] = m[1][0] * r + m[1][1] * g + m[1][2] * b;
                px[2] = m[2][0] * r + m[2][1] * g + m[2][2] * b;
            }
        });
    }
    img.space = ColorSpaceMath::kWorking;
}

bool Develop::Apply(WorkingImage &img, const EditParams &p, StageTimings *t)
{
    if (!img.isValid()) return false;

    /* The input profile is NOT an edit -- a raw arrives in camera-native primaries and
       has to be converted whatever the params say, so this precedes the identity
       early-out. Denoise (#1) runs before the fused point pass, so when it is active the
       conversion has to happen here as its own pass; otherwise it is left to
       buildPointCoeffs, which folds it into preMat for free. */
    const bool denoiseActive = (p.localDenoiseLuma > 0.0f || p.localDenoiseChroma > 0.0f);
    if (p.isIdentity() || denoiseActive) ToWorkingSpace(img);

    if (p.isIdentity()) return true;    // nothing to do; serve image as-is

    QElapsedTimer probe;
    if (t) probe.start();

    /* Fixed pipeline order: spatial ops own a pass, point ops share the fused pass.
       See notes/Documentation.txt "DEVELOP / IMAGE EDIT". Denoise (#1) -> fused point
       pass (#2-5: WB, exposure, contrast, tone regions) -> Texture (#6) ->
       Clarity (#6.5, mid-radius local contrast) -> Dehaze (#7) ->
       Vignette (#8, a radial exposure falloff) -> Sharpen (#8.5, capture USM) ->
       Grain (#9, film grain, last). */
    Denoise(img, p);
    if (t) t->denoiseMs = probe.restart();

    PointCoeffs c = buildPointCoeffs(p, img);
    if (c.active) applyPointOps(img, c);
    /* preMat carried the camera-native -> working conversion, so the pixels are in the
       working space now and the tag has to say so: every op after this point, and
       OutputTransform, read img.space to pick their luma weights and matrices. */
    if (c.preMatActive) img.space = ColorSpaceMath::kWorking;
    if (t) t->pointMs = probe.restart();

    Texture(img, p);
    if (t) t->textureMs = probe.restart();

    Clarity(img, p);
    if (t) t->clarityMs = probe.restart();

    Dehaze(img, p);
    if (t) t->dehazeMs = probe.restart();

    Vignette(img, p);
    if (t) t->vignetteMs = probe.restart();

    Sharpen(img, p);
    if (t) t->sharpenMs = probe.restart();

    Grain(img, p);
    if (t) t->grainMs = probe.restart();
    return true;
}

/* Local luminance noise reduction (spatial op). Ratio-preserving: edge-preserving smoothing of
   luminance only, applied as a per-pixel scale of RGB so hue/chroma ratios are untouched. The
   smoothing runs in the perceptual (gamma) domain -- noise is more uniform there, and it matches
   the contrast op's domain -- while the final ratio is formed in scene-linear. Portable (OpenCV,
   mac + Windows), independent of which engine decoded the image.

   GLOBAL (no-mask) case: smooths the whole frame. When the scope compositor lands, it calls this
   per scope bounded to the mask bbox and blends the result by the scope's alpha (see
   notes/Documentation.txt "Scope & masking model"); the cached output then only re-blends, not
   re-computes, on a point-slider drag. */
void Develop::Denoise(WorkingImage &img, const EditParams &p)
{
    const float lumAmt = p.localDenoiseLuma;
    const float chrAmt = p.localDenoiseChroma;
    if (lumAmt <= 0.0f && chrAmt <= 0.0f) return;

    const int w = img.width;
    const int h = img.height;
    const size_t n = static_cast<size_t>(w) * static_cast<size_t>(h);
    float *rgb = img.rgb.data();
    const float white = (img.white > 0.0f) ? img.white : 1.0f;
    const float invWhite = 1.0f / white;

    /* LUMINANCE NR (localDenoiseLuma): edge-preserving smoothing on luminance only, ratio-
       preserving so chroma is untouched. */
    if (lumAmt > 0.0f) {
        /* Perceptual, white-normalised luminance (Rec.709 linear-luma weights). Ylin keeps the
           scene-linear luma so the post-smoothing ratio can be formed in linear. */
        cv::Mat Yp(h, w, CV_32FC1);
        std::vector<float> Ylin(n);
        float *yp = Yp.ptr<float>();
        float *ylin = Ylin.data();
        parallelFor(n, [=](size_t i0, size_t i1) {
            for (size_t i = i0; i < i1; ++i) {
                const float r = rgb[i * 3 + 0], g = rgb[i * 3 + 1], b = rgb[i * 3 + 2];
                const float Y = kLumR * r + kLumG * g + kLumB * b;
                ylin[i] = Y;
                float nrm = Y * invWhite;
                if (nrm < 0.0f) nrm = 0.0f;
                yp[i] = std::pow(nrm, kInvGamma);
            }
        });

        /* Edge-preserving smoothing on luminance only; strength scales the range/space sigmas.
           d = 0 derives the kernel diameter from sigmaSpace. The bilateral is the expensive part
           and (unlike the O(n) passes) grows with pixel count, so run it at a BOUNDED resolution:
           downscale to ~2 MP, filter with a proportionally smaller spatial sigma (same effective
           radius), then upscale. This keeps the interactive proxy render off the GUI thread's
           critical path (see the Develop perf note in Documentation.txt); small images (<=2 MP,
           e.g. the proxy on a modest display) skip the downscale and filter at full resolution. */
        const double sigmaColor = 0.03 + 0.09 * static_cast<double>(lumAmt);
        const double sigmaSpace = 2.0 + 4.0 * static_cast<double>(lumAmt);
        const double mp = static_cast<double>(n) / 1e6;
        const int scale = (mp > 2.0) ? std::max(1, int(std::lround(std::sqrt(mp / 2.0)))) : 1;
        cv::Mat Ypd;
        if (scale > 1) {
            const int sw = std::max(1, w / scale), sh = std::max(1, h / scale);
            cv::Mat lo, loD;
            cv::resize(Yp, lo, cv::Size(sw, sh), 0, 0, cv::INTER_AREA);
            cv::bilateralFilter(lo, loD, 0, sigmaColor, sigmaSpace / scale);
            cv::resize(loD, Ypd, cv::Size(w, h), 0, 0, cv::INTER_LINEAR);
        }
        else {
            cv::bilateralFilter(Yp, Ypd, 0, sigmaColor, sigmaSpace);
        }

        /* Scale RGB by the linear luminance ratio, preserving chroma. */
        const float *ypd = Ypd.ptr<float>();
        constexpr float kEps = 1e-6f;
        parallelFor(n, [=](size_t i0, size_t i1) {
            for (size_t i = i0; i < i1; ++i) {
                const float Y = ylin[i];
                if (Y <= kEps) continue;                          // black pixel: nothing to scale
                const float Yd = std::pow(ypd[i], kGamma) * white;   // perceptual -> linear
                const float factor = Yd / Y;
                rgb[i * 3 + 0] *= factor;
                rgb[i * 3 + 1] *= factor;
                rgb[i * 3 + 2] *= factor;
            }
        });
    }

    /* COLOUR/CHROMA NR (localDenoiseChroma): smooth the opponent chroma (R-Y, B-Y) while keeping
       luminance EXACT, so colour blotches wash out without touching luma detail. Chroma noise is
       low-frequency, so we blur a downscaled copy (fast, O(1)-ish -- see the Develop perf note in
       Documentation.txt) and upsample; mild colour bleed at edges is acceptable for chroma. Runs
       after the luma pass, on the luma-denoised RGB. */
    if (chrAmt > 0.0f) {
        cv::Mat chroma(h, w, CV_32FC3);          // (Cr, Cb, 0), white-normalised linear
        std::vector<float> Yn(n);
        float *cp = chroma.ptr<float>();
        float *yn = Yn.data();
        parallelFor(n, [=](size_t i0, size_t i1) {
            for (size_t i = i0; i < i1; ++i) {
                const float r = rgb[i * 3 + 0] * invWhite;
                const float g = rgb[i * 3 + 1] * invWhite;
                const float b = rgb[i * 3 + 2] * invWhite;
                const float Y = kLumR * r + kLumG * g + kLumB * b;
                yn[i] = Y;
                cp[i * 3 + 0] = r - Y;            // Cr
                cp[i * 3 + 1] = b - Y;            // Cb
                cp[i * 3 + 2] = 0.0f;
            }
        });

        /* Downscaled edge-preserving blur of the chroma; strength scales the sigmas. */
        const int scale = 4;
        const int sw = std::max(1, w / scale), sh = std::max(1, h / scale);
        cv::Mat lo, loD;
        cv::resize(chroma, lo, cv::Size(sw, sh), 0, 0, cv::INTER_AREA);
        const double sigmaColorC = 0.02 + 0.10 * static_cast<double>(chrAmt);
        const double sigmaSpaceC = 2.0 + 6.0 * static_cast<double>(chrAmt);
        cv::bilateralFilter(lo, loD, 0, sigmaColorC, sigmaSpaceC);
        cv::Mat chromaD;
        cv::resize(loD, chromaD, cv::Size(w, h), 0, 0, cv::INTER_LINEAR);

        /* Recombine: keep Y exact, take the smoothed chroma; derive G so Y is preserved. */
        const float *cpd = chromaD.ptr<float>();
        parallelFor(n, [=](size_t i0, size_t i1) {
            for (size_t i = i0; i < i1; ++i) {
                const float Y = yn[i];
                const float R = Y + cpd[i * 3 + 0];
                const float B = Y + cpd[i * 3 + 1];
                const float G = (Y - kLumR * R - kLumB * B) / kLumG;
                rgb[i * 3 + 0] = std::max(0.0f, R * white);
                rgb[i * 3 + 1] = std::max(0.0f, G * white);
                rgb[i * 3 + 2] = std::max(0.0f, B * white);
            }
        });
    }
}

void Develop::BlendRawDenoise(const WorkingImage &clean, const WorkingImage &den,
                              float lum, float chr, WorkingImage &out)
{
/*
    Interactive "Denoise raw" blend: out = clean + amount-scaled (den - clean). PMRID runs at full
    strength once (den); the two Global amounts scale a luma/chroma split of the correction here, so
    dragging the sliders only re-blends (cheap) instead of re-running the model. Mirrors the
    highlight-preserving split used by the old post-demosaic path: highlights (correction ~0) pass
    through untouched.
*/
    out = clean;                            // dims/white/space/sceneReferred + fallback
    if (clean.width != den.width || clean.height != den.height) return;

    /* Luma weights for the space the buffers are ACTUALLY in. Since RawColor stops at
       camera-native this blend now runs on SENSOR primaries, where the working space's
       weights are the wrong numbers -- a camera's green channel does not carry 0.72 of
       its luminance. lumaWeightsFor (workingimage.h) returns the right ones. */
    const ColorSpaceMath::Luma LW = lumaWeightsFor(clean);
    const float wR = LW.r, wG = LW.g, wB = LW.b;
    lum = std::clamp(lum, 0.0f, 1.0f);
    chr = std::clamp(chr, 0.0f, 1.0f);
    const float cAmt = std::max(lum, chr);
    if (cAmt <= 0.0f) return;                        // nothing to add -> clean

    const size_t n = static_cast<size_t>(clean.width) * static_cast<size_t>(clean.height);
    const float *c = clean.rgb.data();
    const float *d = den.rgb.data();
    float *o = out.rgb.data();
    parallelFor(n, [=](size_t i0, size_t i1) {
        for (size_t i = i0; i < i1; ++i) {
            const float dr = d[i * 3 + 0] - c[i * 3 + 0];
            const float dg = d[i * 3 + 1] - c[i * 3 + 1];
            const float db = d[i * 3 + 2] - c[i * 3 + 2];
            const float dY = wR * dr + wG * dg + wB * db;   // luma of the correction
            o[i * 3 + 0] = c[i * 3 + 0] + lum * dY + cAmt * (dr - dY);
            o[i * 3 + 1] = c[i * 3 + 1] + lum * dY + cAmt * (dg - dY);
            o[i * 3 + 2] = c[i * 3 + 2] + lum * dY + cAmt * (db - dY);
        }
    });
}

/* Texture (spatial op #6). Ratio-preserving luminance local contrast in the perceptual domain:
   a Gaussian base isolates a mid-frequency detail band, which the slider amplifies (crisper) or
   attenuates (smoother). Runs after the point pass. See the class doc and the constants above. */
void Develop::Texture(WorkingImage &img, const EditParams &p)
{
    const float amt = p.texture / kTextureFullScale;     // -1..1
    if (amt == 0.0f) return;

    cv::Mat Yp;
    std::vector<float> Ylin;
    buildPerceptualLuma(img, Yp, Ylin);

    /* Mid-frequency base; sigma scales with the long edge so proxy and full-res match. */
    const cv::Mat base = gaussianBase(Yp, kTextureSigmaFrac, img.width, img.height);

    /* Positive uses the stronger kTextureGain; negative scales the detail down to zero
       at -1 (bandFactor clamps, so it lands on the base rather than inverting). */
    const float factor =
        LocalContrast::bandFactor(amt, (amt >= 0.0f) ? kTextureGain : 1.0f);
    const float *bp = base.ptr<float>();
    shapeAndFoldLuma(img, Yp.ptr<float>(), Ylin.data(), [=](size_t i, float yp) {
        return LocalContrast::applyBand(yp, bp[i], factor);
    });
}

/* Clarity (spatial op #6.5). Ratio-preserving luminance local contrast at a MID radius --
   between Texture (#6, ~13px) and Dehaze (#7, ~170px) on an 8640px edge -- so the three
   stack rather than compete. Positive adds midtone punch, negative gives a soft glow.

   Unlike Texture the band is WEIGHTED per pixel before it is scaled: midtoneWeight fades
   the effect toward black and white, and haloGuard rolls it off where the high-pass
   excursion is large, i.e. at the strong edges where a band this wide would otherwise
   ring. Weighting the AMOUNT (rather than the result) means the two guards simply pull
   the band factor back toward 1, so they can never invert or overshoot the band.

   Shares gaussianBase with Texture (the downscale trick keeps the wider blur cheap) and
   the perceptual-luma prep / ratio fold-back with every other band op. Math in
   Develop/localcontrast.h; no-op at 0. */
void Develop::Clarity(WorkingImage &img, const EditParams &p)
{
    const float amt = p.clarity / kClarityFullScale;     // -1..1
    if (amt == 0.0f) return;

    cv::Mat Yp;
    std::vector<float> Ylin;
    buildPerceptualLuma(img, Yp, Ylin);

    const cv::Mat base = gaussianBase(Yp, kClaritySigmaFrac, img.width, img.height);
    const float *bp = base.ptr<float>();

    shapeAndFoldLuma(img, Yp.ptr<float>(), Ylin.data(), [=](size_t i, float yp) {
        const float b = bp[i];
        const float w = LocalContrast::midtoneWeight(yp) *
                        LocalContrast::haloGuard(yp - b, kClarityHaloKnee);
        const float factor = LocalContrast::bandFactor(amt * w, kClarityGain);
        return LocalContrast::applyBand(yp, b, factor);
    });
}

/* Dehaze (spatial op #7), APPROXIMATION. Stage 1: ratio-preserving luminance large-radius local
   contrast plus a contrast pull about a low pivot (deepens shadows / extends range, the look of
   clearing haze). Stage 2: a saturation lift (haze desaturates). Positive removes haze, negative
   adds it. Not a physical dark-channel-prior dehaze -- see notes/Documentation.txt. */
void Develop::Dehaze(WorkingImage &img, const EditParams &p)
{
    const float amt = p.dehaze / kDehazeFullScale;       // -1..1
    if (amt == 0.0f) return;

    const int w = img.width;
    const int h = img.height;
    const size_t n = static_cast<size_t>(w) * static_cast<size_t>(h);
    float *rgb = img.rgb.data();

    cv::Mat Yp;
    std::vector<float> Ylin;
    buildPerceptualLuma(img, Yp, Ylin);

    /* Large-radius base via a box blur: its running-sum cost is ~independent of kernel
       size, so the wide blur stays cheap even at full resolution -- which is why this op
       does NOT use gaussianBase like Texture and Clarity. */
    const int rad = qMax(1, static_cast<int>(std::lround(static_cast<double>(kDehazeSigmaFrac) * qMax(w, h))));
    const int ksz = rad * 2 + 1;
    cv::Mat base;
    cv::blur(Yp, base, cv::Size(ksz, ksz));

    /* Stage 1: luminance local contrast + low-pivot contrast (ratio-preserving). The
       local
       contrast is the shared band expression: this op historically wrote it as
       yp + k*(yp-base), which is the same thing as base + (1+k)*(yp-base) in exact
       arithmetic and differs only in float rounding (~1e-7, far below one 8-bit code
       value). tst_localcontrast pins the pre-extraction render to prove it stayed put. */
    const float factor = LocalContrast::bandFactor(amt, kDehazeLocalGain);
    const float cont  = amt * kDehazeContrast;
    const float *bp = base.ptr<float>();
    shapeAndFoldLuma(img, Yp.ptr<float>(), Ylin.data(), [=](size_t i, float yp) {
        const float s = LocalContrast::applyBand(yp, bp[i], factor);
        return kDehazePivot + (s - kDehazePivot) * (1.0f + cont);   // deepen shadows
    });

    /* Stage 2: saturation about per-pixel luminance (haze desaturates; dehaze restores). */
    const float sat = 1.0f + amt * kDehazeSat;
    parallelFor(n, [=](size_t i0, size_t i1) {
        for (size_t i = i0; i < i1; ++i) {
            const float r = rgb[i * 3 + 0], g = rgb[i * 3 + 1], b = rgb[i * 3 + 2];
            const float Y2 = kLumR * r + kLumG * g + kLumB * b;
            float nr = Y2 + sat * (r - Y2);
            float ng = Y2 + sat * (g - Y2);
            float nb = Y2 + sat * (b - Y2);
            if (nr < 0.0f) nr = 0.0f;
            if (ng < 0.0f) ng = 0.0f;
            if (nb < 0.0f) nb = 0.0f;
            rgb[i * 3 + 0] = nr;
            rgb[i * 3 + 1] = ng;
            rgb[i * 3 + 2] = nb;
        }
    });
}

/* Vignette (spatial op #8, runs last). Per-pixel radial exposure gain about the centre;
   see the constants block above for the mask/feather model. A pure per-pixel op (no
   neighbourhood), but kept out of the fused point pass since it needs pixel coordinates,
   not just the channel value. Custom / off-centre vignettes are handled by radial masks,
   so this global op is centre-only. */
void Develop::Vignette(WorkingImage &img, const EditParams &p)
{
    const float ev = p.vignetteExposure;                 // EV applied at the corners
    if (ev == 0.0f) return;

    const int w = img.width;
    const int h = img.height;
    if (w < 1 || h < 1) return;
    const size_t n = static_cast<size_t>(w) * static_cast<size_t>(h);
    float *rgb = img.rgb.data();

    /* Elliptical normalisation: dx,dy in [-1,1] across the frame, so the radius reaches 1
       at the mid-edges and sqrt(2) at the corners; dividing by sqrt(2) puts full effect
       at the corners. */
    const float cx = (w - 1) * 0.5f;
    const float cy = (h - 1) * 0.5f;
    const float invHalfW = (w > 1) ? 2.0f / (w - 1) : 0.0f;
    const float invHalfH = (h > 1) ? 2.0f / (h - 1) : 0.0f;
    const float invCorner = 0.70710678f;                 // 1 / sqrt(2)

    /* Feather -> falloff exponent. High feather spreads the effect inward (low k); low
       feather concentrates it in the corners (high k). */
    const float feather = qBound(0.0f, p.vignetteFeather, 1.0f);
    const float k = kVignetteFeatherKMin +
                    (1.0f - feather) * (kVignetteFeatherKMax - kVignetteFeatherKMin);

    parallelFor(n, [=](size_t i0, size_t i1) {
        for (size_t i = i0; i < i1; ++i) {
            const int x = static_cast<int>(i % static_cast<size_t>(w));
            const int y = static_cast<int>(i / static_cast<size_t>(w));
            const float dx = (x - cx) * invHalfW;         // -1..1
            const float dy = (y - cy) * invHalfH;         // -1..1
            float rn = std::sqrt(dx * dx + dy * dy) * invCorner;   // 0 centre -> 1 corner
            if (rn > 1.0f) rn = 1.0f;
            const float mask = std::pow(rn, k);           // 0 centre -> 1 corner
            const float gain = std::exp2(ev * mask);      // ev stops at corner
            rgb[i * 3 + 0] *= gain;
            rgb[i * 3 + 1] *= gain;
            rgb[i * 3 + 2] *= gain;
        }
    });
}

/* Sharpen (spatial op #8.5, between Vignette and Grain). Capture sharpening: an unsharp
   mask on perceptual luminance, folded back as a ratio-preserving RGB scale so hue and
   chroma are untouched -- the same house idiom as Texture / Denoise / Grain.

   THE ONE OP THAT IS NOT SCALE-INVARIANT. Texture and Dehaze derive their sigma from
   max(w,h) so the proxy and the full-res settle render shape the same RELATIVE band.
   Acutance is a property of the pixel grid, so sharpenRadius is an ABSOLUTE pixel radius
   and the effective sigma is scaled by img.renderScale instead: the proxy shows a
   scale-honest preview, but the true result is the full-res one, judged at 1:1.

   Detail soft-thresholds the smallest amplitudes away; Masking gates the whole thing by
   local edge strength so flat sky / skin is left alone. Both live in Develop/sharpen.h.
   The Gaussian here is small (sigma <= 3), so unlike Texture there is no downscale
   trick -- a separable blur at this radius is already cheap. No-op at amount 0. */
void Develop::Sharpen(WorkingImage &img, const EditParams &p)
{
    const float amount = qMax(0.0f, p.sharpenAmount);
    if (amount == 0.0f) return;

    const int w = img.width;
    const int h = img.height;
    if (w < 3 || h < 3) return;                      // no neighbourhood to work with

    cv::Mat Yp;
    std::vector<float> Ylin;
    buildPerceptualLuma(img, Yp, Ylin);

    const float scale = (img.renderScale > 0.0f) ? img.renderScale : 1.0f;
    const float sigma = Sharpen::effectiveSigma(p.sharpenRadius, scale);

    cv::Mat base;
    cv::GaussianBlur(Yp, base, cv::Size(0, 0), sigma);

    /* Edge strength for the mask gate. Only computed when Masking is actually engaged --
       at 0 the gate is 1 everywhere and the Sobel pair would be pure waste. */
    const float masking = qBound(0.0f, p.sharpenMasking, 1.0f);
    cv::Mat grad;
    if (masking > 0.0f) {
        /* Gradient of the BLURRED luma, so noise does not read as an edge. The 3x3 Sobel
           kernel has a gain of 8 (a ramp of slope s answers 8s), so scale by 1/8 to get
           the TRUE per-pixel slope -- Sharpen::maskKnee is expressed in perceptual-luma
           units per pixel, and unscaled Sobel would open the gate ~8x too easily. */
        constexpr double kSobelNorm = 1.0 / 8.0;
        cv::Mat gx, gy;
        cv::Sobel(base, gx, CV_32F, 1, 0, 3, kSobelNorm);
        cv::Sobel(base, gy, CV_32F, 0, 1, 3, kSobelNorm);
        cv::magnitude(gx, gy, grad);
    }

    const float detail = qBound(0.0f, p.sharpenDetail, 1.0f);
    const float *bp = base.ptr<float>();
    const float *gp = grad.empty() ? nullptr : grad.ptr<float>();
    shapeAndFoldLuma(img, Yp.ptr<float>(), Ylin.data(), [=](size_t i, float yp) {
        const float gm = gp ? gp[i] : 0.0f;
        return Sharpen::applyPixel(yp, bp[i], gm, amount, detail, masking, scale);
    });
}

/* Grain (spatial op #9, runs LAST). Monochromatic film grain: a deterministic noise field
   is generated on a coarse grain-cell grid (size sets the cell), bilinearly upsampled,
   its amplitude modulated by a lower-frequency roughness field. It is added to PERCEPTUAL
   luminance and folded back as a ratio-preserving RGB scale (like Texture) so only the
   luminance is perturbed. A midtone weight fades the grain toward pure black / white. See
   the grain constants block for the size / roughness / proxy-match model. No-op at 0. */
void Develop::Grain(WorkingImage &img, const EditParams &p)
{
    const float amount = qBound(0.0f, p.grainAmount, 1.0f);
    if (amount == 0.0f) return;

    const int w = img.width;
    const int h = img.height;
    if (w < 1 || h < 1) return;
    const size_t n = static_cast<size_t>(w) * static_cast<size_t>(h);
    float *rgb = img.rgb.data();
    const float white = (img.white > 0.0f) ? img.white : 1.0f;
    const float invWhite = 1.0f / white;

    /* Grain cell size in pixels: scales with the long edge (so the proxy and full-res
       render show the same grain proportionally) and grows with grainSize (fine to
       coarse). The noise grid divides the image into cells: bigger cell = bigger grain. */
    const float size01 = qBound(0.0f, p.grainSize, 1.0f);
    const double unit = qMax(1.0, static_cast<double>(qMax(w, h)) / kGrainCellUnit);
    const double cellPx = unit * (kGrainCellMin + size01 * (kGrainCellMax - kGrainCellMin));
    const int nw = qMax(1, static_cast<int>(std::lround(w / cellPx)));
    const int nh = qMax(1, static_cast<int>(std::lround(h / cellPx)));

    /* Fixed-seed noise -> stable across re-renders (no shimmer). Grain N(0,1) on the cell
       grid, upsampled to full res; the roughness field is a coarser [0,1] grid that patchily
       modulates the grain amplitude (0 roughness = uniform, 1 = strongly varying). */
    cv::RNG rng(kGrainSeed);
    cv::Mat grainSmall(nh, nw, CV_32FC1);
    rng.fill(grainSmall, cv::RNG::NORMAL, 0.0, 1.0);
    cv::Mat grain;
    cv::resize(grainSmall, grain, cv::Size(w, h), 0, 0, cv::INTER_LINEAR);

    const float rough01 = qBound(0.0f, p.grainRoughness, 1.0f);
    cv::Mat roughUp;
    if (rough01 > 0.0f) {
        const int rw = qMax(1, nw / kGrainRoughFreqDiv);
        const int rh = qMax(1, nh / kGrainRoughFreqDiv);
        cv::Mat roughSmall(rh, rw, CV_32FC1);
        rng.fill(roughSmall, cv::RNG::UNIFORM, 0.0, 1.0);
        cv::resize(roughSmall, roughUp, cv::Size(w, h), 0, 0, cv::INTER_LINEAR);
    }

    const float strength = amount * kGrainStrength;
    const float *gp = grain.ptr<float>();
    const float *rp = roughUp.empty() ? nullptr : roughUp.ptr<float>();
    constexpr float kEps = 1e-6f;
    parallelFor(n, [=](size_t i0, size_t i1) {
        for (size_t i = i0; i < i1; ++i) {
            const float r = rgb[i * 3 + 0], g = rgb[i * 3 + 1], b = rgb[i * 3 + 2];
            const float Y = kLumR * r + kLumG * g + kLumB * b;
            if (Y <= kEps) continue;
            float nrm = Y * invWhite;
            if (nrm < 0.0f) nrm = 0.0f;
            const float s = std::pow(nrm, kInvGamma);
            /* Midtone weight: peaks at mid-grey, fades to 0 at black / white; sqrt broadens
               it so shadows and highlights still carry some grain. Shared with Clarity --
               the expression is unchanged, so grain renders exactly as before. */
            const float lw = LocalContrast::midtoneWeight(s);
            /* Roughness: mean-1 amplitude that varies more as roughness rises (u in [0,1]). */
            const float amp = rp ? (1.0f - rough01 + rough01 * 2.0f * rp[i]) : 1.0f;
            float s2 = s + strength * lw * amp * gp[i];
            if (s2 < 0.0f) s2 = 0.0f;
            const float Yd = std::pow(s2, kGamma) * white;
            const float ratio = Yd / Y;
            rgb[i * 3 + 0] = r * ratio;
            rgb[i * 3 + 1] = g * ratio;
            rgb[i * 3 + 2] = b * ratio;
        }
    });
}

Develop::PointCoeffs Develop::buildPointCoeffs(const EditParams &p, const WorkingImage &img)
{
    PointCoeffs c;

    /* Exposure: a scene-linear gain of 2^EV (a +1 EV slider doubles the linear value). */
    const float exposureGain = (p.exposure != 0.0f) ? std::exp2(p.exposure) : 1.0f;

    /* White balance: per-channel scene-linear gains for the absolute (Kelvin, tint),
       taken RELATIVE to the as-shot rendering so an untouched image (p.temp == 0) is
       an exact no-op. Folded with the uniform exposure gain AND the Colour RGB
       sliders (per-channel gain, kRgbGain) into a single per-channel factor -- all
       are linear multiplies, so they commute and the fused pass applies one multiply
       per channel. */
    float wbGain[3] = {1.0f, 1.0f, 1.0f};
    if (p.temp > 0.0f || p.tint != 0.0f) {
        float kelvin, tint;
        WhiteBalance::resolve(img.cam, p.temp, p.tint, kelvin, tint);
        WhiteBalance::relativeGains(img.cam, kelvin, tint, wbGain);
    }
    const float rGain = 1.0f + kRgbGain * (p.red   / kRgbFullScale);
    const float gGain = 1.0f + kRgbGain * (p.green / kRgbFullScale);
    const float bGain = 1.0f + kRgbGain * (p.blue  / kRgbFullScale);
    c.channelGain[0] = wbGain[0] * exposureGain * rGain;
    c.channelGain[1] = wbGain[1] * exposureGain * gGain;
    c.channelGain[2] = wbGain[2] * exposureGain * bGain;

    c.white = (img.white > 0.0f ? img.white : 1.0f);

    /* Perceptual tone curve = contrast + the four tone-region controls + the Curves
       panel's point curves, baked into a per-channel 1-D LUT (see PointCoeffs). Everything here is in the PERCEPTUAL (gamma) domain, NOT scene-linear:
       a contrast/tone curve in linear pivots around 0.18 and sends highlights (several stops
       above the pivot) far too high. Encoding to ~gamma first puts mid-grey near 0.46 so the
       curve shapes tones evenly, like a normal tone control.

       Contrast is a gentle slope about mid-grey: the -100..100 slider normalises to amount
       -1..1, mapping to 1 + 0.4*amount -> [0.6, 1.4] (kContrastSlopeRange); >1 steepens.
       The tone regions add a Gaussian-weighted lift on top (see kToneShift / toneWeight). */
    const ToneShape shape = buildToneShape(p);

    /* The Curves panel's tone curve (Develop/tonecurve.h). Channel 0 is the RGB
       composite; 1/2/3 are the per-channel Red / Green / Blue curves layered on top of
       it. Built once here (n <= 16 control points each), then evaluated per LUT entry --
       never per pixel. */
    ToneCurve::Spline curve[ToneCurve::kChannels];
    for (int ch = 0; ch < ToneCurve::kChannels; ++ch)
        curve[ch].build(p.curveX[ch], p.curveY[ch], p.curveN[ch]);
    const bool curveActive = !p.curvesAreIdentity();

    c.toneActive = shape.active || curveActive;
    if (c.toneActive) {
        const float sMax = std::pow(kToneLutMaxN, kInvGamma);
        c.toneLutSMax = sMax;
        for (int j = 0; j < PointCoeffs::kLutSize; ++j) {
            /* s = the perceptual input value this table entry represents. */
            float s = (static_cast<float>(j) / (PointCoeffs::kLutSize - 1)) * sMax;
            s = applyToneShape(shape, s);                    // contrast + tone regions
            /* The tone curve runs AFTER the parametric controls above, matching
               Lightroom's order: the RGB composite first, then that channel's own curve
               on the composite's output. Both extrapolate past x = 1 with their end
               slope, so the ~3 stops of highlight headroom in this table stay smooth. */
            if (curveActive) s = curve[0].eval(s);
            for (int ch = 0; ch < 3; ++ch) {
                float sc = curveActive ? curve[ch + 1].eval(s) : s;
                if (sc < 0.0f) sc = 0.0f;                    // crush blacks gracefully
                /* decode -> white-normalised linear */
                c.toneLut[ch][j] = std::pow(sc, kGamma);
            }
        }
    }

    /* HSL (hue/saturation/luminance): a cross-channel block applied after the tone curve. Hue
       rotates about the neutral (1,1,1) axis (Rodrigues rotation of RGB space, which fixes the
       gray axis so neutrals stay neutral); saturation scales chroma about luma; luminance is a
       uniform gain. */
    const float hue = p.hue        / kHslFullScale;     // -1..1
    const float sat = p.saturation / kHslFullScale;
    const float vib = p.vibrance   / kHslFullScale;
    const float lum = p.luminance  / kHslFullScale;
    c.satFactor = 1.0f + sat;                          // -1 -> grayscale, +1 -> 2x chroma
    c.vibAmount = vib;                                 // per-pixel weighted chroma boost
    c.lumGain   = 1.0f + lum;                          // -1 -> black, +1 -> 2x
    if (hue != 0.0f) {
        const float a = hue * kHueMaxRad;
        const float cs = std::cos(a);
        const float sn = std::sin(a);
        const float k  = (1.0f - cs) / 3.0f;            // axis (1,1,1)/sqrt3 -> k = (1-cos)*nx*ny
        const float w  = sn * 0.57735027f;              // sin * 1/sqrt(3)
        c.hueMat[0] = cs + k;       c.hueMat[1] = k - w;       c.hueMat[2] = k + w;
        c.hueMat[3] = k + w;        c.hueMat[4] = cs + k;      c.hueMat[5] = k - w;
        c.hueMat[6] = k - w;        c.hueMat[7] = k + w;       c.hueMat[8] = cs + k;
    }
    c.hslActive = (c.satFactor != 1.0f) || (c.vibAmount != 0.0f) ||
                  (c.lumGain != 1.0f) || (hue != 0.0f);

    /* Calibration: re-point the R/G/B primaries into one 3x3 matrix. Runs in linear light
       before the tone curve, so it is built here but applied earlier in the fused loop
       than HSL. All six sliders at 0 leaves the identity matrix, which we skip. */
    c.calActive = !Calibrate::isIdentity(p.calRedHue, p.calRedSat, p.calGreenHue,
                                         p.calGreenSat, p.calBlueHue, p.calBlueSat);
    if (c.calActive)
        Calibrate::buildMatrix(p.calRedHue, p.calRedSat, p.calGreenHue, p.calGreenSat,
                               p.calBlueHue, p.calBlueSat, c.calMat);

    /* Colour grading: turn each range's (hue, sat, lum) into a pre-scaled zero-luma RGB
       tint vector + a luminance gain delta. HSV(hue, 1, 1) gives a fully-saturated hue
       colour; subtracting its Rec.709 luma leaves a chroma-only push that shifts colour
       without brightness. sat scales it, so a sat-0 range contributes nothing. */
    const float gHue[4] = {p.gradeShadowHue, p.gradeMidHue, p.gradeHighHue,
                           p.gradeGlobalHue};
    const float gSat[4] = {p.gradeShadowSat, p.gradeMidSat, p.gradeHighSat,
                           p.gradeGlobalSat};
    const float gLum[4] = {p.gradeShadowLum, p.gradeMidLum, p.gradeHighLum,
                           p.gradeGlobalLum};
    for (int r = 0; r < 4; ++r) {
        ColorGrade::gradeTintVector(gHue[r], gSat[r], kGradeTintStrength, c.gradeTint[r]);
        c.gradeLum[r] = (gLum[r] / kGradeLumFullScale) * kGradeLumStrength;
    }
    c.gradeActive = false;
    for (int r = 0; r < 4 && !c.gradeActive; ++r)
        c.gradeActive = (c.gradeTint[r][0] != 0.0f || c.gradeTint[r][1] != 0.0f ||
                         c.gradeTint[r][2] != 0.0f || c.gradeLum[r] != 0.0f);

    /* Window shape from the panel-wide Blending / Balance sliders. At their defaults this
       returns the two base constants unchanged, so old grades render as before. */
    ColorGrade::gradeSplitPoints(p.gradeBlending, p.gradeBalance,
                                 kGradeShadowEnd, kGradeHighStart,
                                 kGradeBlendRange, kGradeBalanceRange,
                                 kGradeSplitMin, kGradeSplitMax,
                                 c.gradeShadowEnd, c.gradeHighStart);

    /*
        FOLD THE FRONT OF THE PIPELINE INTO ONE MATRIX.

        For camera-native input (a raw that RawColor deliberately left unconverted) the
        first three stages are all linear:

            camToWorking . diag(asShotMul)   the input profile -- what the decoder used
                                             to bake in, moved here so a profile change
                                             is a re-render, not a re-decode
            diag(channelGain)                white balance x exposure x RGB sliders
            calMat                           the Calibrate primaries rotation

        so they multiply out to a single 3x3. channelGain and calMat are then cleared:
        they are IN preMat, and applying them twice is the obvious bug here.
    */
    if (img.space == ColorSpaceMath::ColorSpace::CameraNative && img.cam.valid) {
        /* camToWorking . diag(asShotMul) -- scaling a matrix's COLUMN j by asShotMul[j]
           is what right-multiplying by a diagonal does. */
        float m[3][3];
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 3; ++j)
                m[i][j] = img.cam.camToWorking[i][j] * img.cam.asShotMul[j];

        /* diag(channelGain) . m -- a diagonal on the LEFT scales ROW i. */
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 3; ++j) m[i][j] *= c.channelGain[i];

        /* calMat . m, when the Calibrate panel is doing anything. */
        if (c.calActive) {
            float o[3][3];
            for (int i = 0; i < 3; ++i)
                for (int j = 0; j < 3; ++j) {
                    float s = 0.0f;
                    for (int k = 0; k < 3; ++k) s += c.calMat[i * 3 + k] * m[k][j];
                    o[i][j] = s;
                }
            for (int i = 0; i < 3; ++i)
                for (int j = 0; j < 3; ++j) m[i][j] = o[i][j];
        }

        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 3; ++j) c.preMat[i * 3 + j] = m[i][j];
        c.preMatActive = true;
        c.preMatClamps = c.calActive;    // the calibrate block clamps; the fold must too
        c.calActive    = false;                                  // folded into preMat
        c.channelGain[0] = c.channelGain[1] = c.channelGain[2] = 1.0f;   // ditto
    }

    c.active = (c.channelGain[0] != 1.0f) || (c.channelGain[1] != 1.0f) ||
               (c.channelGain[2] != 1.0f) || c.toneActive || c.calActive ||
               c.hslActive || c.gradeActive || c.preMatActive;
    return c;
}

/* Sample the parametric tone shape over the DISPLAY range [0,1] for the Curves panel.
   Clamped to [0,1] because it is drawn inside a unit plot; the LUT itself is not clamped
   (it carries ~3 stops of headroom above white), so this is a view of the visible part of
   the same curve, not a different curve. */
void Develop::ParametricCurve(const EditParams &p, float *out, int n)
{
    if (!out || n < 2) return;
    const ToneShape shape = buildToneShape(p);
    for (int i = 0; i < n; ++i) {
        const float s = static_cast<float>(i) / (n - 1);
        out[i] = qBound(0.0f, applyToneShape(shape, s), 1.0f);
    }
}

void Develop::applyPointOps(WorkingImage &img, const PointCoeffs &c)
{
    const int w = img.width;
    const int h = img.height;
    float *rgb = img.rgb.data();

    const float g0 = c.channelGain[0];
    const float g1 = c.channelGain[1];
    const float g2 = c.channelGain[2];
    const float white = c.white;
    const float invWhite = 1.0f / white;
    const bool doGain = (g0 != 1.0f) || (g1 != 1.0f) || (g2 != 1.0f);
    const bool doTone = c.toneActive;
    const bool doCal  = c.calActive;
    const bool doPre  = c.preMatActive;
    const bool doPreClamp = c.preMatClamps;
    const bool doHsl  = c.hslActive;
    const bool doGrade = c.gradeActive;

    /* Fused front matrix (input profile + gains + calibrate), local like the rest. */
    const float pm0 = c.preMat[0], pm1 = c.preMat[1], pm2 = c.preMat[2];
    const float pm3 = c.preMat[3], pm4 = c.preMat[4], pm5 = c.preMat[5];
    const float pm6 = c.preMat[6], pm7 = c.preMat[7], pm8 = c.preMat[8];

    /* Calibration matrix, local so the block folds into the inner loop like the rest. */
    const float cm0 = c.calMat[0], cm1 = c.calMat[1], cm2 = c.calMat[2];
    const float cm3 = c.calMat[3], cm4 = c.calMat[4], cm5 = c.calMat[5];
    const float cm6 = c.calMat[6], cm7 = c.calMat[7], cm8 = c.calMat[8];

    /* HSL coeffs (cross-channel, applied after the tone curve). Locals so they fold into the
       inner loop the same way the gain/tone constants do. */
    const float hm0 = c.hueMat[0], hm1 = c.hueMat[1], hm2 = c.hueMat[2];
    const float hm3 = c.hueMat[3], hm4 = c.hueMat[4], hm5 = c.hueMat[5];
    const float hm6 = c.hueMat[6], hm7 = c.hueMat[7], hm8 = c.hueMat[8];
    const float satF = c.satFactor;
    const float vibA = c.vibAmount;
    const bool  doVib = (vibA != 0.0f);
    const float lumG = c.lumGain;

    /* Colour-grading coeffs (per-range zero-luma tint + luminance delta). Locals so the
       grade block folds into the inner loop like the HSL constants. Tonal weights split the
       pixel's PERCEPTUAL lightness (gamma-encoded luma), matching the slider feel. */
    const float gt00 = c.gradeTint[0][0], gt01 = c.gradeTint[0][1], gt02 = c.gradeTint[0][2];
    const float gt10 = c.gradeTint[1][0], gt11 = c.gradeTint[1][1], gt12 = c.gradeTint[1][2];
    const float gt20 = c.gradeTint[2][0], gt21 = c.gradeTint[2][1], gt22 = c.gradeTint[2][2];
    /* Range 3 is Global: no tonal window, so it is added at weight 1. */
    const float gt30 = c.gradeTint[3][0], gt31 = c.gradeTint[3][1], gt32 = c.gradeTint[3][2];
    const float gl0 = c.gradeLum[0], gl1 = c.gradeLum[1], gl2 = c.gradeLum[2];
    const float gl3 = c.gradeLum[3];
    const float gShadowEnd = c.gradeShadowEnd, gHighStart = c.gradeHighStart;

    /* Perceptual tone curve via the precomputed LUT (contrast + tone regions, see
       buildPointCoeffs). The table is indexed by the perceptual value s = (v/white)^(1/gamma)
       and already returns the white-normalised linear output, so the only transcendental in the
       hot loop is the single encode pow. Constants are local so they fold into the inner loop. */
    const float *lutR = c.toneLut[0];
    const float *lutG = c.toneLut[1];
    const float *lutB = c.toneLut[2];
    const int lutLast = PointCoeffs::kLutSize - 1;
    const float lutScale = lutLast / c.toneLutSMax;
    /* `lut` is the channel's own table -- the three differ only when a per-channel
       Curves R/G/B curve is in play, and are byte-identical otherwise. */
    auto toneCh = [=](const float *lut, float v) -> float {
        float n = v * invWhite;
        if (n < 0.0f) n = 0.0f;
        const float s = std::pow(n, kInvGamma);
        float fi = s * lutScale;
        if (fi >= lutLast) return lut[lutLast] * white;      // clamp above the table's domain
        const int i = static_cast<int>(fi);
        const float frac = fi - i;
        return (lut[i] + (lut[i + 1] - lut[i]) * frac) * white;
    };

    /* One fused per-pixel kernel over a band of rows. Point ops apply PER CHANNEL (R,G,B
       interleaved), so the inner loop walks a pixel at a time: per-channel linear gain (white
       balance + exposure) then the perceptual tone curve, matching the fixed pipeline order
       (WB/exposure in scene-linear, contrast + tone regions in the gamma domain). */
    auto processRows = [=](int y0, int y1) {
        const int span = w * 3;
        for (int y = y0; y < y1; ++y) {
            float *row = rgb + static_cast<size_t>(y) * span;
            for (int x = 0; x < w; ++x) {
                float *px = row + static_cast<size_t>(x) * 3;
                if (doPre) {
                    /* Camera-native -> working space, with the white balance, exposure,
                       RGB sliders and Calibrate all folded into this one matrix (see
                       PointCoeffs::preMat). Replaces the gain and calibrate blocks
                       below -- buildPointCoeffs cleared them so they cannot double up.

                       The clamp fires only when CALIBRATE was folded in, because that is
                       the only stage in the sequence that clamps. Without it, negatives
                       are KEPT: an out-of-gamut sensor colour is real data, and the
                       output stage is what decides how to bring it back, not a clamp
                       buried mid-pipeline. */
                    const float r = px[0], g = px[1], b = px[2];
                    float pr = pm0 * r + pm1 * g + pm2 * b;
                    float pg = pm3 * r + pm4 * g + pm5 * b;
                    float pb = pm6 * r + pm7 * g + pm8 * b;
                    if (doPreClamp) {
                        if (pr < 0.0f) pr = 0.0f;
                        if (pg < 0.0f) pg = 0.0f;
                        if (pb < 0.0f) pb = 0.0f;
                    }
                    px[0] = pr; px[1] = pg; px[2] = pb;
                }
                if (doGain) { px[0] *= g0; px[1] *= g1; px[2] *= g2; }
                if (doCal) {
                    /* Re-point the primaries in LINEAR light, before any tone shaping.
                       The matrix fixes the neutral axis, so greys pass through. */
                    const float r = px[0], g = px[1], b = px[2];
                    float cr = cm0 * r + cm1 * g + cm2 * b;
                    float cg = cm3 * r + cm4 * g + cm5 * b;
                    float cb = cm6 * r + cm7 * g + cm8 * b;
                    px[0] = (cr < 0.0f) ? 0.0f : cr;
                    px[1] = (cg < 0.0f) ? 0.0f : cg;
                    px[2] = (cb < 0.0f) ? 0.0f : cb;
                }
                if (doTone) {
                    px[0] = toneCh(lutR, px[0]);
                    px[1] = toneCh(lutG, px[1]);
                    px[2] = toneCh(lutB, px[2]);
                }
                if (doHsl) {
                    float r = px[0], g = px[1], b = px[2];
                    /* Hue: rotate about the neutral axis (identity matrix when hue == 0). */
                    const float hr = hm0 * r + hm1 * g + hm2 * b;
                    const float hg = hm3 * r + hm4 * g + hm5 * b;
                    const float hb = hm6 * r + hm7 * g + hm8 * b;
                    /* Saturation: scale chroma about Rec.709 luma. Vibrance adds a
                       per-pixel boost weighted by (1 - HSV saturation), so muted pixels
                       move more than already-saturated ones; the two factors combine
                       multiplicatively. */
                    const float Y = kLumR * hr + kLumG * hg + kLumB * hb;
                    float sF = satF;
                    if (doVib) {
                        const float mx = (hr > hg ? (hr > hb ? hr : hb) : (hg > hb ? hg : hb));
                        const float mn = (hr < hg ? (hr < hb ? hr : hb) : (hg < hb ? hg : hb));
                        const float pxSat = (mx > 1e-6f) ? (mx - mn) / mx : 0.0f;  // 0..1
                        sF *= 1.0f + vibA * (1.0f - pxSat);
                        if (sF < 0.0f) sF = 0.0f;        // never invert chroma
                    }
                    r = (Y + sF * (hr - Y)) * lumG;       // luminance: uniform gain
                    g = (Y + sF * (hg - Y)) * lumG;
                    b = (Y + sF * (hb - Y)) * lumG;
                    px[0] = (r < 0.0f) ? 0.0f : r;
                    px[1] = (g < 0.0f) ? 0.0f : g;
                    px[2] = (b < 0.0f) ? 0.0f : b;
                }
                if (doGrade) {
                    /* Split the pixel's perceptual lightness into three smooth tonal
                       windows (shadows/mid/highlights, weights sum to 1), then apply each
                       range's luminance gain + zero-luma chroma tint, weighted by its
                       window -- plus the Global range, which is not tone-selective and so
                       rides in at weight 1. Tint is in 0..1 RGB units, scaled to the
                       working range by white. The perceptual encode pow is the only
                       transcendental. */
                    float Yg = kLumR * px[0] + kLumG * px[1] + kLumB * px[2];
                    float n = Yg * invWhite;
                    if (n < 0.0f) n = 0.0f; else if (n > 1.0f) n = 1.0f;
                    const float L = std::pow(n, kInvGamma);          // perceptual 0..1
                    float wS, wM, wH;
                    ColorGrade::gradeTonalWeights(L, gShadowEnd, gHighStart, wS, wM, wH);
                    const float lumMul = 1.0f + wS * gl0 + wM * gl1 + wH * gl2 + gl3;
                    const float tR = (wS * gt00 + wM * gt10 + wH * gt20 + gt30) * white;
                    const float tG = (wS * gt01 + wM * gt11 + wH * gt21 + gt31) * white;
                    const float tB = (wS * gt02 + wM * gt12 + wH * gt22 + gt32) * white;
                    float r = px[0] * lumMul + tR;
                    float g = px[1] * lumMul + tG;
                    float b = px[2] * lumMul + tB;
                    px[0] = (r < 0.0f) ? 0.0f : r;
                    px[1] = (g < 0.0f) ? 0.0f : g;
                    px[2] = (b < 0.0f) ? 0.0f : b;
                }
            }
        }
    };

    /* Data-parallel over row ranges within this single render (QtConcurrent + the global
       pool, the same idiom as FocusStack). The interactive edit is one image, so splitting
       its rows is what uses all cores; a background decoder that also calls render() shares
       the fixed-size pool, which bounds oversubscription. */
    const int maxThreads = qMax(1, QThreadPool::globalInstance()->maxThreadCount());
    const int kMinRowsPerChunk = 64;
    if (maxThreads == 1 || h < kMinRowsPerChunk * 2) {
        processRows(0, h);
        return;
    }

    const int chunks = qMin(maxThreads, (h + kMinRowsPerChunk - 1) / kMinRowsPerChunk);
    const int rowsPerChunk = (h + chunks - 1) / chunks;
    QVector<QFuture<void>> futures;
    futures.reserve(chunks);
    for (int k = 0; k < chunks; ++k) {
        const int y0 = k * rowsPerChunk;
        const int y1 = qMin(h, y0 + rowsPerChunk);
        if (y0 >= y1) break;
        futures.append(QtConcurrent::run(QThreadPool::globalInstance(),
                                         [processRows, y0, y1]() { processRows(y0, y1); }));
    }
    for (QFuture<void> &f : futures) f.waitForFinished();
}
