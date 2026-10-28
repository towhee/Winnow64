#include "Develop/autotone.h"
#include "Develop/develop.h"
#include "Develop/workingimagecache.h"
#include "Develop/whitebalance.h"

#include <QElapsedTimer>
#include <algorithm>
#include <cmath>
#include <vector>

namespace AutoTone {

namespace {

/* ---- Tuning: fitted to Lightroom's Auto (tests/calibration/autotonecompare.py) ------

   Least squares on 36 Lightroom Auto results (2026-10-28), each feature measured on the
   render the step sees. Lightroom's Auto is close to a FIXED base recipe -- Highlights
   about -60, Shadows +40, Blacks -20, Contrast +6, Vibrance +15 on almost every image --
   adjusted per image through Exposure, Whites and a little extra Shadows lift for a dark
   frame. "Always Lightroom's median" beat every content rule the first version had. */

constexpr int   kProxyLong     = 360;     // proxy long edge: ~86k px, a few ms a render

/* Exposure = kKeyGain * tanh(stops / kKeySoft) + kTopGain * (kTopTarget - p99), where
   stops = log2(middle grey / key). The tanh is why a frame 6 stops dark gets about +1.9,
   not +6; the p99 term is Lightroom watching the top end, which is what lets a hazy
   high-key frame with no highlights be BRIGHTENED although its key is above grey. */
constexpr float kKeyTarget     = 0.18f;   // middle grey, display-linear
constexpr float kKeyGain       = 0.727f;
constexpr float kKeySoft       = 2.5f;    // stops
constexpr float kTopGain       = 2.09f;
constexpr float kTopTarget     = 0.866f;  // display luma
constexpr float kExposureMin   = -3.0f;
constexpr float kExposureMax   =  3.0f;

/* Contrast: Lightroom's Auto is +5..+7 on every image. */
constexpr float kContrast      = 6.0f;

/* Highlights: a fixed pull, smaller for a display-referred file (a JPEG already carries
   its camera's highlight roll-off). */
constexpr float kHighlightsRaw     = -65.0f;
constexpr float kHighlightsDisplay = -39.0f;

/* Shadows / Whites / Blacks: base + gain * the fraction of the frame below kDarkThresh
   (after Exposure), Whites also falling with the frame's p1..p99 range. A frame that is
   still largely dark gets more Shadows lift, less Whites and a lighter Blacks. */
constexpr float kDarkThresh    = 0.15f;   // display luma
constexpr float kShadowsBase   = 36.0f;
constexpr float kShadowsDark   = 44.0f;
constexpr float kWhitesBase    = 48.0f;
constexpr float kWhitesDark    = -51.0f;
constexpr float kWhitesRange   = -33.0f;
constexpr float kBlacksBase    = -24.0f;
constexpr float kBlacksDark    = 27.0f;

/* Vibrance / Saturation: Lightroom's Auto is Vibrance ~+15 (a little less on a JPEG)
   and Saturation ~0 whatever the colour content, monochrome frames included. */
constexpr float kVibranceRaw     = 15.0f;
constexpr float kVibranceDisplay = 13.0f;
constexpr float kSaturation      = 0.0f;

/* ------------------------------------------------------------------------------------ */

/* sRGB transfer, decoded once: display code value -> display-linear. */
const float *srgbToLinear()
{
    static const std::vector<float> lut = [] {
        std::vector<float> t(256);
        for (int i = 0; i < 256; ++i) {
            const double v = i / 255.0;
            t[i] = float(v <= 0.04045 ? v / 12.92 : std::pow((v + 0.055) / 1.055, 2.4));
        }
        return t;
    }();
    return lut.data();
}

float roundTo(float v, float step) { return std::round(v / step) * step; }

} // namespace

float Stats::percentile(double q) const
{
    if (total <= 0.0) return 0.0f;
    const double want = std::clamp(q, 0.0, 1.0) * total;
    double cum = 0.0;
    for (int i = 0; i < 256; ++i) {
        if (cum + hist[i] >= want && hist[i] > 0.0) {
            /* Bin i spans [i-0.5, i+0.5] code values; place the crossing within it. */
            const double f = (want - cum) / hist[i];
            return float(std::clamp((i - 0.5 + f) / 255.0, 0.0, 1.0));
        }
        cum += hist[i];
    }
    return 1.0f;
}

float Stats::fractionAbove(float v) const
{
    if (total <= 0.0) return 0.0f;
    const int b0 = std::clamp(int(std::ceil(v * 255.0f)), 0, 256);
    double n = 0.0;
    for (int i = b0; i < 256; ++i) n += hist[i];
    return float(n / total);
}

float Stats::fractionBelow(float v) const
{
    if (total <= 0.0) return 0.0f;
    const int b1 = std::clamp(int(std::floor(v * 255.0f)), -1, 255);
    double n = 0.0;
    for (int i = 0; i <= b1; ++i) n += hist[i];
    return float(n / total);
}

Stats Measure(const QImage &src)
{
    Stats s;
    const QImage img = src.format() == QImage::Format_RGB888
                       ? src : src.convertToFormat(QImage::Format_RGB888);
    const int W = img.width(), H = img.height();
    if (W < 1 || H < 1) return s;

    const float *lin = srgbToLinear();
    const float cx = 0.5f * (W - 1), cy = 0.5f * (H - 1);
    double logSum = 0.0, wSum = 0.0, sum = 0.0, sum2 = 0.0;
    for (int y = 0; y < H; ++y) {
        const uchar *row = img.constScanLine(y);
        const float dy = cy > 0.0f ? (y - cy) / cy : 0.0f;
        for (int x = 0; x < W; ++x) {
            const int r = row[x*3+0], g = row[x*3+1], b = row[x*3+2];
            /* The histogram scope's luma, exactly (MW::updateDevelopScopes), so a target
               here means the same place on the histogram the user is looking at. */
            const int luma = (r * 54 + g * 183 + b * 19) >> 8;
            s.hist[luma & 0xff] += 1.0;
            const double l = luma / 255.0;
            sum += l;
            sum2 += l * l;

            /* Centre weight 1 -> 0.5 at the corners: the subject is usually nearer the
               middle, but a dark border must still count. */
            const float dx = cx > 0.0f ? (x - cx) / cx : 0.0f;
            const float r2 = std::min(1.0f, 0.5f * (dx * dx + dy * dy));
            const double w = 1.0 - 0.5 * r2;
            const double Y = 0.2126 * lin[r] + 0.7152 * lin[g] + 0.0722 * lin[b];
            logSum += w * std::log(std::max(Y, 1e-3));
            wSum += w;
        }
    }
    s.total = double(W) * H;
    s.mean = float(sum / s.total);
    s.sd = float(std::sqrt(std::max(0.0, sum2 / s.total - double(s.mean) * s.mean)));
    s.key = float(std::exp(logSum / wSum));

    return s;
}

bool Solve(const WorkingImage &work, const EditParams &base, Result &out)
{
    out = Result();
    if (!work.isValid()) return false;

    /*
        The proxy gets its OWN statistics slot, pre-filled with the FULL frame's samples.
        Shared, the proxy's Apply would fill the slot's LocalSample (Highlights -'s local
        Laplacian) from these 360 px and every later render of the image would inherit
        the coarse one; sampled from the proxy, the Highlights band strength would drift
        from what the loupe draws. Taking them from `work` is what the loupe render does
        anyway, so it fills nothing the next render would not have.
    */
    QElapsedTimer stage;
    stage.start();
    WorkingImage proxy = WorkingImageCache::downscaled(work, kProxyLong);
    out.proxyMs = int(stage.restart());
    auto slot = std::make_shared<StatSampleSlot>();
    slot->sample = Develop::StatSample(work);
    out.sampleMs = int(stage.restart());
    slot->local  = Develop::LocalSample(work);
    out.localMs = int(stage.restart());
    proxy.statSlot = slot;

    WorkingImage scratch;
    bool ok = true;
    auto measure = [&](const EditParams &p) -> Stats {
        QImage img;
        ++out.renders;
        if (!WorkingImageCache::render(proxy, p, img, nullptr,
                                       WorkingImageCache::OutDepth::Eight,
                                       WorkingImageCache::Space::sRGB, &scratch)) {
            ok = false;
            return Stats();
        }
        return Measure(img);
    };

    EditParams p = base;
    p.exposure = p.contrast = p.highlights = p.shadows = p.whites = p.blacks = 0.0f;
    p.vibrance = p.saturation = 0.0f;

    /* 1. Exposure -- from the key and the top end of the untouched render. */
    const Stats s0 = measure(p);
    if (!ok || s0.key <= 0.0f) return false;
    const float stops = std::log2(kKeyTarget / s0.key);
    p.exposure = roundTo(std::clamp(kKeyGain * std::tanh(stops / kKeySoft) +
                                        kTopGain * (kTopTarget - s0.percentile(0.99)),
                                    kExposureMin, kExposureMax), 0.01f);

    /* 2. The rest -- Lightroom's base recipe, shaped by how dark the exposed frame
       still is. */
    const Stats s1 = measure(p);
    if (!ok) return false;
    const float dark = s1.fractionBelow(kDarkThresh);
    const float range = s1.percentile(0.99) - s1.percentile(0.01);
    p.contrast   = kContrast;
    p.highlights = work.sceneReferred ? kHighlightsRaw : kHighlightsDisplay;
    p.shadows    = std::round(std::clamp(kShadowsBase + kShadowsDark * dark, 0.0f, 100.0f));
    p.whites     = std::round(std::clamp(kWhitesBase + kWhitesDark * dark +
                                             kWhitesRange * range, -100.0f, 100.0f));
    p.blacks     = std::round(std::clamp(kBlacksBase + kBlacksDark * dark, -100.0f, 0.0f));
    p.vibrance   = work.sceneReferred ? kVibranceRaw : kVibranceDisplay;
    p.saturation = kSaturation;

    out.rendersMs  = int(stage.elapsed());
    out.exposure   = p.exposure;
    out.contrast   = p.contrast;
    out.highlights = p.highlights;
    out.shadows    = p.shadows;
    out.whites     = p.whites;
    out.blacks     = p.blacks;
    out.vibrance   = p.vibrance;
    out.saturation = p.saturation;
    return true;
}

AutosResult ApplyPresetAutos(const WorkingImage &work, bool wb, bool tone, EditParams &p)
{
    AutosResult res;
    if (wb) {
        float k = 0.0f, t = 0.0f;
        res.wbOk = WhiteBalance::autoWhiteBalance(work, k, t);
        if (res.wbOk) {
            p.temp = k;
            p.tint = t;
            p.wbPreset = int(WhiteBalance::Preset::Auto);
        }
    }
    if (tone) {
        Result r;
        res.toneOk = Solve(work, p, r);
        res.renders = r.renders;
        res.tone = r;
        if (res.toneOk) {
            p.exposure   = r.exposure;
            p.contrast   = r.contrast;
            p.highlights = r.highlights;
            p.shadows    = r.shadows;
            p.whites     = r.whites;
            p.blacks     = r.blacks;
            p.vibrance   = r.vibrance;
            p.saturation = r.saturation;
        }
    }
    return res;
}

} // namespace AutoTone
