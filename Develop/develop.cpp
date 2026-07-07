#include "Develop/develop.h"
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
   rescales the light before any tone curve). The temp/tint sliders are integer -100..100
   controls, normalised to amount -1..1 by kWbFullScale. Temp warms by lifting red / lowering
   blue (green fixed); tint shifts the green<->magenta axis on the green channel. The ranges
   are deliberately gentle creative defaults (full slider = +/-50% on a channel). */
constexpr float kWbFullScale = 100.0f;
constexpr float kTempGain    = 0.5f;    // full warm: R *1.5, B *0.5
constexpr float kTintGain    = 0.5f;    // full magenta: G *0.5

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

/* Texture (spatial op): mid-frequency luminance local contrast. The base-blur radius is a
   fraction of the image's longer edge so the proxy and full-res renders shape the same band;
   kTextureGain sets how hard a positive slider amplifies the detail (negative smooths it). */
constexpr float kTextureFullScale = 100.0f;
constexpr float kTextureSigmaFrac = 0.0015f;   // ~13px on a 8640px edge
constexpr float kTextureGain      = 1.5f;      // amount=+1 -> detail *2.5

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
}

bool Develop::Apply(WorkingImage &img, const EditParams &p, StageTimings *t)
{
    if (!img.isValid()) return false;
    if (p.isIdentity()) return true;    // nothing to do; serve image as-is

    QElapsedTimer probe;
    if (t) probe.start();

    /* Fixed pipeline order: spatial ops own a pass, point ops share the fused pass.
       See notes/Documentation.txt "DEVELOP / IMAGE EDIT". Denoise (#1) -> fused point pass
       (#2-5: WB, exposure, contrast, tone regions) -> Texture (#6) -> Dehaze (#7). */
    Denoise(img, p);
    if (t) t->denoiseMs = probe.restart();

    PointCoeffs c = buildPointCoeffs(p, img);
    if (c.active) applyPointOps(img, c);
    if (t) t->pointMs = probe.restart();

    Texture(img, p);
    if (t) t->textureMs = probe.restart();

    Dehaze(img, p);
    if (t) t->dehazeMs = probe.restart();
    return true;
}

/* Local luminance noise reduction (spatial op). Ratio-preserving: edge-preserving smoothing of
   luminance only, applied as a per-pixel scale of RGB so hue/chroma ratios are untouched. The
   smoothing runs in the perceptual (gamma) domain -- noise is more uniform there, and it matches
   the contrast op's domain -- while the final ratio is formed in scene-linear. Portable (OpenCV,
   mac + Windows), independent of which engine decoded the image.

   GLOBAL (no-mask) case: smooths the whole frame. When the layer compositor lands, it calls this
   per layer bounded to the mask bbox and blends the result by the layer's alpha (see
   notes/Documentation.txt "Layer & masking model"); the cached output then only re-blends, not
   re-computes, on a point-slider drag. */
void Develop::Denoise(WorkingImage &img, const EditParams &p)
{
    const float amt = p.localDenoiseLuma;
    if (amt <= 0.0f) return;

    const int w = img.width;
    const int h = img.height;
    const size_t n = static_cast<size_t>(w) * static_cast<size_t>(h);
    float *rgb = img.rgb.data();
    const float white = (img.white > 0.0f) ? img.white : 1.0f;
    const float invWhite = 1.0f / white;

    /* Perceptual, white-normalised luminance (Rec.709 linear-luma weights). Ylin keeps the
       scene-linear luma so the post-smoothing ratio can be formed in linear. */
    cv::Mat Yp(h, w, CV_32FC1);
    std::vector<float> Ylin(n);
    float *yp = Yp.ptr<float>();
    float *ylin = Ylin.data();
    parallelFor(n, [=](size_t i0, size_t i1) {
        for (size_t i = i0; i < i1; ++i) {
            const float r = rgb[i * 3 + 0], g = rgb[i * 3 + 1], b = rgb[i * 3 + 2];
            const float Y = 0.2126f * r + 0.7152f * g + 0.0722f * b;
            ylin[i] = Y;
            float nrm = Y * invWhite;
            if (nrm < 0.0f) nrm = 0.0f;
            yp[i] = std::pow(nrm, kInvGamma);
        }
    });

    /* Edge-preserving smoothing on luminance only; strength scales the range/space sigmas.
       bilateralFilter parallelises internally on OpenCV's own pool (see the threading note in
       Documentation.txt). d = 0 derives the kernel diameter from sigmaSpace. */
    const double sigmaColor = 0.03 + 0.09 * static_cast<double>(amt);
    const double sigmaSpace = 2.0 + 4.0 * static_cast<double>(amt);
    cv::Mat Ypd;
    cv::bilateralFilter(Yp, Ypd, 0, sigmaColor, sigmaSpace);

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

void Develop::BlendRawDenoise(const WorkingImage &clean, const WorkingImage &den,
                              float lum, float chr, WorkingImage &out)
{
/*
    Interactive "Denoise raw" blend: out = clean + amount-scaled (den - clean). PMRID runs at full
    strength once (den); the two Base amounts scale a luma/chroma split of the correction here, so
    dragging the sliders only re-blends (cheap) instead of re-running the model. Mirrors the
    highlight-preserving split used by the old post-demosaic path: highlights (correction ~0) pass
    through untouched.
*/
    out = clean;                                    // dims/white/sceneReferred + fallback
    if (clean.width != den.width || clean.height != den.height) return;
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
            const float dY = 0.2126f * dr + 0.7152f * dg + 0.0722f * db;   // luma of the correction
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

    const int w = img.width;
    const int h = img.height;
    const size_t n = static_cast<size_t>(w) * static_cast<size_t>(h);
    float *rgb = img.rgb.data();
    const float white = (img.white > 0.0f) ? img.white : 1.0f;
    const float invWhite = 1.0f / white;

    /* Perceptual, white-normalised luminance; Ylin keeps the scene-linear luma for the ratio. */
    cv::Mat Yp(h, w, CV_32FC1);
    std::vector<float> Ylin(n);
    float *yp = Yp.ptr<float>();
    float *ylin = Ylin.data();
    parallelFor(n, [=](size_t i0, size_t i1) {
        for (size_t i = i0; i < i1; ++i) {
            const float r = rgb[i * 3 + 0], g = rgb[i * 3 + 1], b = rgb[i * 3 + 2];
            const float Y = 0.2126f * r + 0.7152f * g + 0.0722f * b;
            ylin[i] = Y;
            float nrm = Y * invWhite;
            if (nrm < 0.0f) nrm = 0.0f;
            yp[i] = std::pow(nrm, kInvGamma);
        }
    });

    /* Mid-frequency base; sigma scales with the longer edge so proxy and full-res match. The base
       is a smooth low-frequency band, so for a large sigma compute the (costly) Gaussian on a
       DOWNSCALED luminance image and upsample -- visually identical to a full-res blur but the
       blur cost drops ~kDown^2 (GaussianBlur is the dominant texture cost at full res). The
       downscale factor targets a well-sampled small-image working sigma (~3px) and is capped so
       the base stays smooth; sigma <= ~3 (e.g. the proxy) blurs at full size as before. */
    const double sigma = qMax(1.0, static_cast<double>(kTextureSigmaFrac) * qMax(w, h));
    cv::Mat base;
    const int kDown = qBound(1, static_cast<int>(sigma / 3.0), 4);
    if (kDown > 1 && w >= kDown * 2 && h >= kDown * 2) {
        cv::Mat small;
        cv::resize(Yp, small, cv::Size(w / kDown, h / kDown), 0, 0, cv::INTER_AREA);
        cv::GaussianBlur(small, small, cv::Size(0, 0), sigma / kDown);
        cv::resize(small, base, cv::Size(w, h), 0, 0, cv::INTER_LINEAR);
    } else {
        cv::GaussianBlur(Yp, base, cv::Size(0, 0), sigma);
    }

    /* base + factor*(detail): factor>1 amplifies the band, factor in [0,1) smooths toward base.
       Positive uses the stronger kTextureGain; negative scales the detail down to zero at -1. */
    const float factor = qMax(0.0f, 1.0f + amt * (amt >= 0.0f ? kTextureGain : 1.0f));
    const float *bp = base.ptr<float>();
    constexpr float kEps = 1e-6f;
    parallelFor(n, [=](size_t i0, size_t i1) {
        for (size_t i = i0; i < i1; ++i) {
            const float Y = ylin[i];
            if (Y <= kEps) continue;
            float s = bp[i] + factor * (yp[i] - bp[i]);
            if (s < 0.0f) s = 0.0f;
            const float Yd = std::pow(s, kGamma) * white;
            const float r = Yd / Y;
            rgb[i * 3 + 0] *= r;
            rgb[i * 3 + 1] *= r;
            rgb[i * 3 + 2] *= r;
        }
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
    const float white = (img.white > 0.0f) ? img.white : 1.0f;
    const float invWhite = 1.0f / white;

    cv::Mat Yp(h, w, CV_32FC1);
    std::vector<float> Ylin(n);
    float *yp = Yp.ptr<float>();
    float *ylin = Ylin.data();
    parallelFor(n, [=](size_t i0, size_t i1) {
        for (size_t i = i0; i < i1; ++i) {
            const float r = rgb[i * 3 + 0], g = rgb[i * 3 + 1], b = rgb[i * 3 + 2];
            const float Y = 0.2126f * r + 0.7152f * g + 0.0722f * b;
            ylin[i] = Y;
            float nrm = Y * invWhite;
            if (nrm < 0.0f) nrm = 0.0f;
            yp[i] = std::pow(nrm, kInvGamma);
        }
    });

    /* Large-radius base via a box blur: its running-sum cost is ~independent of the kernel size,
       so the wide blur stays cheap even at full resolution. */
    const int rad = qMax(1, static_cast<int>(std::lround(static_cast<double>(kDehazeSigmaFrac) * qMax(w, h))));
    const int ksz = rad * 2 + 1;
    cv::Mat base;
    cv::blur(Yp, base, cv::Size(ksz, ksz));

    /* Stage 1: luminance local contrast + low-pivot contrast (ratio-preserving). */
    const float local = amt * kDehazeLocalGain;
    const float cont  = amt * kDehazeContrast;
    const float *bp = base.ptr<float>();
    constexpr float kEps = 1e-6f;
    parallelFor(n, [=](size_t i0, size_t i1) {
        for (size_t i = i0; i < i1; ++i) {
            const float Y = ylin[i];
            if (Y <= kEps) continue;
            float s = yp[i] + local * (yp[i] - bp[i]);              // amplify local detail
            s = kDehazePivot + (s - kDehazePivot) * (1.0f + cont);  // deepen shadows / extend range
            if (s < 0.0f) s = 0.0f;
            const float Yd = std::pow(s, kGamma) * white;
            const float r = Yd / Y;
            rgb[i * 3 + 0] *= r;
            rgb[i * 3 + 1] *= r;
            rgb[i * 3 + 2] *= r;
        }
    });

    /* Stage 2: saturation about per-pixel luminance (haze desaturates; dehaze restores). */
    const float sat = 1.0f + amt * kDehazeSat;
    parallelFor(n, [=](size_t i0, size_t i1) {
        for (size_t i = i0; i < i1; ++i) {
            const float r = rgb[i * 3 + 0], g = rgb[i * 3 + 1], b = rgb[i * 3 + 2];
            const float Y2 = 0.2126f * r + 0.7152f * g + 0.0722f * b;
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

Develop::PointCoeffs Develop::buildPointCoeffs(const EditParams &p, const WorkingImage &img)
{
    PointCoeffs c;

    /* Exposure: a scene-linear gain of 2^EV (a +1 EV slider doubles the linear value). */
    const float exposureGain = (p.exposure != 0.0f) ? std::exp2(p.exposure) : 1.0f;

    /* White balance: per-channel scene-linear gains (see kTempGain/kTintGain). Folded with the
       uniform exposure gain AND the Colour RGB sliders (per-channel gain, kRgbGain) into a single
       per-channel factor -- all are linear multiplies, so they commute and the fused pass applies
       one multiply per channel. */
    const float t  = p.temp / kWbFullScale;     // -1..1
    const float tn = p.tint / kWbFullScale;     // -1..1
    const float rGain = 1.0f + kRgbGain * (p.red   / kRgbFullScale);
    const float gGain = 1.0f + kRgbGain * (p.green / kRgbFullScale);
    const float bGain = 1.0f + kRgbGain * (p.blue  / kRgbFullScale);
    c.channelGain[0] = (1.0f + kTempGain * t)  * exposureGain * rGain;   // R: up when warm
    c.channelGain[1] = (1.0f - kTintGain * tn) * exposureGain * gGain;   // G: down toward magenta
    c.channelGain[2] = (1.0f - kTempGain * t)  * exposureGain * bGain;   // B: down when warm

    c.white = (img.white > 0.0f ? img.white : 1.0f);

    /* Perceptual tone curve = contrast + the four tone-region controls, baked into a 1-D LUT
       (see PointCoeffs). Everything here is in the PERCEPTUAL (gamma) domain, NOT scene-linear:
       a contrast/tone curve in linear pivots around 0.18 and sends highlights (several stops
       above the pivot) far too high. Encoding to ~gamma first puts mid-grey near 0.46 so the
       curve shapes tones evenly, like a normal tone control.

       Contrast is a gentle slope about mid-grey: the -100..100 slider normalises to amount
       -1..1, mapping to 1 + 0.4*amount -> [0.6, 1.4] (kContrastSlopeRange); >1 steepens.
       The tone regions add a Gaussian-weighted lift on top (see kToneShift / toneWeight). */
    const float slope = (p.contrast != 0.0f)
                            ? 1.0f + kContrastSlopeRange * (p.contrast / kContrastFullScale)
                            : 1.0f;
    const float hi = p.highlights / kToneFullScale;     // -1..1, + brightens
    const float sh = p.shadows    / kToneFullScale;
    const float wh = p.whites     / kToneFullScale;
    const float bk = p.blacks     / kToneFullScale;

    c.toneActive = (slope != 1.0f) || (hi != 0.0f) || (sh != 0.0f) || (wh != 0.0f) || (bk != 0.0f);
    if (c.toneActive) {
        const float pivot = std::pow(0.18f, kInvGamma);
        const float sMax  = std::pow(kToneLutMaxN, kInvGamma);
        c.toneLutSMax = sMax;
        /* Region slider: shadows/highlights centres move with the handles; blacks (0) and whites
           (1) stay pinned. The crossover sets each movable region's reach (sigma). Defaults
           0.25/0.50/0.75 give the old fixed centres and kToneSigma, so this is a no-op until moved. */
        /* Clamp defensively so a hand-edited / corrupt sidecar can never invert the order
           (qBound requires min <= max): shC <= 0.94 leaves room for hiC, which sits at least
           0.04 above it, and the crossover stays strictly between them. */
        const float shC = qBound(0.02f, p.toneShadowCenter, 0.94f);
        const float hiC = qBound(shC + 0.04f, p.toneHighlightCenter, 0.98f);
        const float crX = qBound(shC + 0.01f, p.toneCrossover, hiC - 0.01f);
        const float shSig = std::max(kToneSigmaMin, (crX - shC) * kToneSigmaSpan);
        const float hiSig = std::max(kToneSigmaMin, (hiC - crX) * kToneSigmaSpan);
        for (int j = 0; j < PointCoeffs::kLutSize; ++j) {
            /* s = the perceptual input value this table entry represents. */
            float s = (static_cast<float>(j) / (PointCoeffs::kLutSize - 1)) * sMax;
            s = pivot + (s - pivot) * slope;                 // contrast slope about mid-grey
            const float lift = hi * toneWeight(s, hiC, hiSig)
                             + sh * toneWeight(s, shC, shSig)
                             + wh * toneWeight(s, kWhitesCenter, kToneSigma)
                             + bk * toneWeight(s, kBlacksCenter, kToneSigma);
            s += kToneShift * lift;                          // tone-region shifts
            if (s < 0.0f) s = 0.0f;                          // crush blacks gracefully
            c.toneLut[j] = std::pow(s, kGamma);              // decode -> white-normalised linear
        }
    }

    /* HSL (hue/saturation/luminance): a cross-channel block applied after the tone curve. Hue
       rotates about the neutral (1,1,1) axis (Rodrigues rotation of RGB space, which fixes the
       gray axis so neutrals stay neutral); saturation scales chroma about luma; luminance is a
       uniform gain. */
    const float hue = p.hue        / kHslFullScale;     // -1..1
    const float sat = p.saturation / kHslFullScale;
    const float lum = p.luminance  / kHslFullScale;
    c.satFactor = 1.0f + sat;                           // -1 -> grayscale, +1 -> 2x chroma
    c.lumGain   = 1.0f + lum;                           // -1 -> black, +1 -> 2x
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
    c.hslActive = (c.satFactor != 1.0f) || (c.lumGain != 1.0f) || (hue != 0.0f);

    c.active = (c.channelGain[0] != 1.0f) || (c.channelGain[1] != 1.0f) ||
               (c.channelGain[2] != 1.0f) || c.toneActive || c.hslActive;
    return c;
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
    const bool doHsl  = c.hslActive;

    /* HSL coeffs (cross-channel, applied after the tone curve). Locals so they fold into the
       inner loop the same way the gain/tone constants do. */
    const float hm0 = c.hueMat[0], hm1 = c.hueMat[1], hm2 = c.hueMat[2];
    const float hm3 = c.hueMat[3], hm4 = c.hueMat[4], hm5 = c.hueMat[5];
    const float hm6 = c.hueMat[6], hm7 = c.hueMat[7], hm8 = c.hueMat[8];
    const float satF = c.satFactor;
    const float lumG = c.lumGain;

    /* Perceptual tone curve via the precomputed LUT (contrast + tone regions, see
       buildPointCoeffs). The table is indexed by the perceptual value s = (v/white)^(1/gamma)
       and already returns the white-normalised linear output, so the only transcendental in the
       hot loop is the single encode pow. Constants are local so they fold into the inner loop. */
    const float *lut = c.toneLut;
    const int lutLast = PointCoeffs::kLutSize - 1;
    const float lutScale = lutLast / c.toneLutSMax;
    auto toneCh = [=](float v) -> float {
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
                if (doGain) { px[0] *= g0; px[1] *= g1; px[2] *= g2; }
                if (doTone) {
                    px[0] = toneCh(px[0]);
                    px[1] = toneCh(px[1]);
                    px[2] = toneCh(px[2]);
                }
                if (doHsl) {
                    float r = px[0], g = px[1], b = px[2];
                    /* Hue: rotate about the neutral axis (identity matrix when hue == 0). */
                    const float hr = hm0 * r + hm1 * g + hm2 * b;
                    const float hg = hm3 * r + hm4 * g + hm5 * b;
                    const float hb = hm6 * r + hm7 * g + hm8 * b;
                    /* Saturation: scale chroma about Rec.709 luma. */
                    const float Y = 0.2126f * hr + 0.7152f * hg + 0.0722f * hb;
                    r = (Y + satF * (hr - Y)) * lumG;     // luminance: uniform gain
                    g = (Y + satF * (hg - Y)) * lumG;
                    b = (Y + satF * (hb - Y)) * lumG;
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
