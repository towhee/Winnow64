#include "Develop/develop.h"
#include <QtConcurrent>
#include <QThreadPool>
#include <QFuture>
#include <QVector>
#include <QtGlobal>
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
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

/* Tone-region controls (highlights/shadows/whites/blacks). Each is a smooth, region-weighted
   shift of the PERCEPTUAL (gamma) value, applied after contrast (pipeline order #5). The
   weight is a Gaussian centred on the region's perceptual position; the four overlap gently so
   the curve stays smooth. Sliders are integer -100..100 (normalised to +/-1 by kToneFullScale);
   positive always brightens (lifts) the region, negative darkens (recovers), like Lightroom.
   kToneShift is the max perceptual lift at full slider. The table is built over up to ~3 stops
   above white (kToneLutMaxN) so highlight headroom is shaped, not clipped. */
constexpr float kToneFullScale    = 100.0f;
constexpr float kToneShift        = 0.20f;
constexpr float kToneSigma        = 0.18f;
constexpr float kBlacksCenter     = 0.00f;
constexpr float kShadowsCenter    = 0.25f;
constexpr float kHighlightsCenter = 0.75f;
constexpr float kWhitesCenter     = 1.00f;
constexpr float kToneLutMaxN      = 8.0f;   // build the curve over linear n in [0, 8] (~3 stops)

inline float toneWeight(float s, float center)
{
    const float d = s - center;
    return std::exp(-(d * d) / (2.0f * kToneSigma * kToneSigma));
}

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
}

bool Develop::Apply(WorkingImage &img, const EditParams &p)
{
    if (!img.isValid()) return false;
    if (p.isIdentity()) return true;    // nothing to do; serve image as-is

    /* Fixed pipeline order: spatial ops own a pass, point ops share the fused pass.
       See notes/Documentation.txt "DEVELOP / IMAGE EDIT". Denoise (#1) -> fused point pass
       (#2-5: WB, exposure, contrast, tone regions) -> Texture (#6) -> Dehaze (#7). */
    Denoise(img, p);

    PointCoeffs c = buildPointCoeffs(p, img);
    if (c.active) applyPointOps(img, c);

    Texture(img, p);
    Dehaze(img, p);
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
    for (size_t i = 0; i < n; ++i) {
        const float r = rgb[i * 3 + 0], g = rgb[i * 3 + 1], b = rgb[i * 3 + 2];
        const float Y = 0.2126f * r + 0.7152f * g + 0.0722f * b;
        Ylin[i] = Y;
        float nrm = Y * invWhite;
        if (nrm < 0.0f) nrm = 0.0f;
        yp[i] = std::pow(nrm, kInvGamma);
    }

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
    for (size_t i = 0; i < n; ++i) {
        const float Y = Ylin[i];
        if (Y <= kEps) continue;                          // black pixel: nothing to scale
        const float Yd = std::pow(ypd[i], kGamma) * white;   // perceptual -> linear
        const float factor = Yd / Y;
        rgb[i * 3 + 0] *= factor;
        rgb[i * 3 + 1] *= factor;
        rgb[i * 3 + 2] *= factor;
    }
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
    for (size_t i = 0; i < n; ++i) {
        const float r = rgb[i * 3 + 0], g = rgb[i * 3 + 1], b = rgb[i * 3 + 2];
        const float Y = 0.2126f * r + 0.7152f * g + 0.0722f * b;
        Ylin[i] = Y;
        float nrm = Y * invWhite;
        if (nrm < 0.0f) nrm = 0.0f;
        yp[i] = std::pow(nrm, kInvGamma);
    }

    /* Mid-frequency base; sigma scales with the longer edge so proxy and full-res match. */
    const double sigma = qMax(1.0, static_cast<double>(kTextureSigmaFrac) * qMax(w, h));
    cv::Mat base;
    cv::GaussianBlur(Yp, base, cv::Size(0, 0), sigma);

    /* base + factor*(detail): factor>1 amplifies the band, factor in [0,1) smooths toward base.
       Positive uses the stronger kTextureGain; negative scales the detail down to zero at -1. */
    const float factor = qMax(0.0f, 1.0f + amt * (amt >= 0.0f ? kTextureGain : 1.0f));
    const float *bp = base.ptr<float>();
    constexpr float kEps = 1e-6f;
    for (size_t i = 0; i < n; ++i) {
        const float Y = Ylin[i];
        if (Y <= kEps) continue;
        float s = bp[i] + factor * (yp[i] - bp[i]);
        if (s < 0.0f) s = 0.0f;
        const float Yd = std::pow(s, kGamma) * white;
        const float r = Yd / Y;
        rgb[i * 3 + 0] *= r;
        rgb[i * 3 + 1] *= r;
        rgb[i * 3 + 2] *= r;
    }
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
    for (size_t i = 0; i < n; ++i) {
        const float r = rgb[i * 3 + 0], g = rgb[i * 3 + 1], b = rgb[i * 3 + 2];
        const float Y = 0.2126f * r + 0.7152f * g + 0.0722f * b;
        Ylin[i] = Y;
        float nrm = Y * invWhite;
        if (nrm < 0.0f) nrm = 0.0f;
        yp[i] = std::pow(nrm, kInvGamma);
    }

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
    for (size_t i = 0; i < n; ++i) {
        const float Y = Ylin[i];
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

    /* Stage 2: saturation about per-pixel luminance (haze desaturates; dehaze restores). */
    const float sat = 1.0f + amt * kDehazeSat;
    for (size_t i = 0; i < n; ++i) {
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
}

Develop::PointCoeffs Develop::buildPointCoeffs(const EditParams &p, const WorkingImage &img)
{
    PointCoeffs c;

    /* Exposure: a scene-linear gain of 2^EV (a +1 EV slider doubles the linear value). */
    const float exposureGain = (p.exposure != 0.0f) ? std::exp2(p.exposure) : 1.0f;

    /* White balance: per-channel scene-linear gains (see kTempGain/kTintGain). Folded with the
       uniform exposure gain into a single per-channel factor -- both are linear multiplies, so
       they commute and the fused pass applies one multiply per channel. */
    const float t  = p.temp / kWbFullScale;     // -1..1
    const float tn = p.tint / kWbFullScale;     // -1..1
    c.channelGain[0] = (1.0f + kTempGain * t)  * exposureGain;   // R: up when warm
    c.channelGain[1] = (1.0f - kTintGain * tn) * exposureGain;   // G: down toward magenta
    c.channelGain[2] = (1.0f - kTempGain * t)  * exposureGain;   // B: down when warm

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
        for (int j = 0; j < PointCoeffs::kLutSize; ++j) {
            /* s = the perceptual input value this table entry represents. */
            float s = (static_cast<float>(j) / (PointCoeffs::kLutSize - 1)) * sMax;
            s = pivot + (s - pivot) * slope;                 // contrast slope about mid-grey
            const float lift = hi * toneWeight(s, kHighlightsCenter)
                             + sh * toneWeight(s, kShadowsCenter)
                             + wh * toneWeight(s, kWhitesCenter)
                             + bk * toneWeight(s, kBlacksCenter);
            s += kToneShift * lift;                          // tone-region shifts
            if (s < 0.0f) s = 0.0f;                          // crush blacks gracefully
            c.toneLut[j] = std::pow(s, kGamma);              // decode -> white-normalised linear
        }
    }

    c.active = (c.channelGain[0] != 1.0f) || (c.channelGain[1] != 1.0f) ||
               (c.channelGain[2] != 1.0f) || c.toneActive;
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
