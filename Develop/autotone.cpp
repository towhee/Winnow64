#include "Develop/autotone.h"
#include "Develop/develop.h"
#include "Develop/workingimagecache.h"

#include <algorithm>
#include <cmath>
#include <vector>
#include <opencv2/imgproc.hpp>

namespace AutoTone {

namespace {

/* ---- Tuning (first guesses; calibrate against Lightroom's Auto) -------------------- */

constexpr int   kProxyLong     = 360;     // proxy long edge: ~86k px, a few ms a render

/* Exposure */
constexpr float kKeyTarget     = 0.18f;   // middle grey, display-linear
constexpr float kKeyStrength   = 0.65f;   // fraction of the way there, in stops
constexpr float kRealBright    = 0.80f;   // p98 luma above this = a real bright end
constexpr float kRealDark      = 0.06f;   // p2 below this = a real dark end
constexpr float kKeyRestraint  = 0.5f;    // strength multiplier when either applies
constexpr float kExposureMin   = -3.0f;
constexpr float kExposureMax   =  3.0f;
constexpr int   kExposureIters = 10;      // 6 EV / 2^10 = 0.006 EV

/* Contrast */
constexpr float kSdTarget      = 0.22f;   // display-luma std. dev. of a "full" picture
constexpr float kSdGain        = 150.0f;  // slider units per unit of sd shortfall
constexpr float kContrastMin   = -25.0f;
constexpr float kContrastMax   =  30.0f;

/* Highlights / Shadows */
constexpr float kHiThresh      = 0.80f;   // display luma
constexpr float kHiGain        = 300.0f;  // slider units per unit of frame fraction
constexpr float kHiMax         = 70.0f;
constexpr float kLoThresh      = 0.15f;
constexpr float kLoGain        = 220.0f;
constexpr float kLoMax         = 60.0f;

/* Whites / Blacks (clip points) */
constexpr double kWhitePct     = 0.995;
constexpr float  kWhiteTarget  = 0.97f;   // ~247 of 255
constexpr float  kWhitesMin    = -40.0f;  // a blown sun must not drag the whole top down
constexpr float  kWhitesMax    =  60.0f;
constexpr double kBlackPct     = 0.002;
constexpr float  kBlackTarget  = 0.015f;  // ~4 of 255
constexpr float  kBlacksMin    = -60.0f;
constexpr float  kBlacksMax    =  30.0f;
constexpr int    kClipIters    = 8;       // 100 units / 2^8 = 0.4 of a slider step

/* Vibrance / Saturation */
constexpr float kChromaTarget  = 26.0f;   // mean CIELAB C* of the mid-tones
constexpr float kMonoChroma    = 3.0f;    // mean C* at or below this: monochrome, no push
constexpr float kColourChroma  = 8.0f;    // ... fading in to the full push by this
constexpr float kGamutChroma   = 75.0f;   // p98 C* above this is near the gamut edge
constexpr float kGamutFade     = 20.0f;   // ... and the positive push is gone by +this
constexpr float kVibGain       = 1.5f;
constexpr float kVibMin        = -10.0f;
constexpr float kVibMax        =  25.0f;
constexpr float kSatGain       = 0.4f;
constexpr float kSatMin        = -5.0f;
constexpr float kSatMax        =  10.0f;

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

/* Increasing f: the x in [lo,hi] where f(x) crosses target. Ends at whichever bound the
   target lies beyond when it is not bracketed, which is the clamp we want. */
template <class F>
float bisect(float lo, float hi, float target, int iters, F f)
{
    for (int i = 0; i < iters; ++i) {
        const float mid = 0.5f * (lo + hi);
        if (f(mid) < target) lo = mid;
        else                 hi = mid;
    }
    return 0.5f * (lo + hi);
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

Stats Measure(const QImage &src, bool withChroma)
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

    if (withChroma) {
        cv::Mat rgb8(H, W, CV_8UC3, const_cast<uchar *>(img.constBits()),
                     size_t(img.bytesPerLine()));
        cv::Mat rgbF, lab;
        rgb8.convertTo(rgbF, CV_32FC3, 1.0 / 255.0);
        cv::cvtColor(rgbF, lab, cv::COLOR_RGB2Lab);     // float: L 0..100, a/b signed
        std::vector<float> c;
        c.reserve(size_t(W) * H);
        for (int y = 0; y < H; ++y) {
            const float *p = lab.ptr<float>(y);
            for (int x = 0; x < W; ++x) {
                const float L = p[x*3+0];
                /* Mid-tones only: chroma collapses toward black and white whatever the
                   saturation, so those pixels would read as "muted" and over-push. */
                if (L > 10.0f && L < 95.0f)
                    c.push_back(std::hypot(p[x*3+1], p[x*3+2]));
            }
        }
        if (!c.empty()) {
            double cs = 0.0;
            for (float v : c) cs += v;
            s.meanChroma = float(cs / c.size());
            const size_t k = std::min(c.size() - 1, size_t(0.98 * c.size()));
            std::nth_element(c.begin(), c.begin() + k, c.end());
            s.highChroma = c[k];
        }
    }
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
    WorkingImage proxy = WorkingImageCache::downscaled(work, kProxyLong);
    auto slot = std::make_shared<StatSampleSlot>();
    slot->sample = Develop::StatSample(work);
    slot->local  = Develop::LocalSample(work);
    proxy.statSlot = slot;

    WorkingImage scratch;
    bool ok = true;
    auto measure = [&](const EditParams &p, bool chroma) -> Stats {
        QImage img;
        ++out.renders;
        if (!WorkingImageCache::render(proxy, p, img, nullptr,
                                       WorkingImageCache::OutDepth::Eight,
                                       WorkingImageCache::Space::sRGB, &scratch)) {
            ok = false;
            return Stats();
        }
        return Measure(img, chroma);
    };

    EditParams p = base;
    p.exposure = p.contrast = p.highlights = p.shadows = p.whites = p.blacks = 0.0f;
    p.vibrance = p.saturation = 0.0f;

    /* 1. Exposure -- move the key part of the way to middle grey. */
    const Stats s0 = measure(p, false);
    if (!ok || s0.key <= 0.0f) return false;
    float strength = kKeyStrength;
    const bool brighten = s0.key < kKeyTarget;
    if (brighten && s0.percentile(0.98) > kRealBright) strength *= kKeyRestraint;
    if (!brighten && s0.percentile(0.02) < kRealDark)  strength *= kKeyRestraint;
    const float keyGoal = float(std::exp(std::log(s0.key) +
                                         strength * (std::log(kKeyTarget) - std::log(s0.key))));
    p.exposure = roundTo(bisect(kExposureMin, kExposureMax, keyGoal, kExposureIters,
                                [&](float e) { p.exposure = e; return measure(p, false).key; }),
                         0.01f);
    if (!ok) return false;

    /* 2. Contrast -- from the spread at the new exposure. */
    const Stats s1 = measure(p, false);
    p.contrast = std::round(std::clamp(kSdGain * (kSdTarget - s1.sd),
                                       kContrastMin, kContrastMax));

    /* 3. Highlights / Shadows -- from how much of the frame sits at each end. */
    const Stats s2 = measure(p, false);
    p.highlights = -std::round(std::clamp(kHiGain * s2.fractionAbove(kHiThresh),
                                          0.0f, kHiMax));
    p.shadows = std::round(std::clamp(kLoGain * s2.fractionBelow(kLoThresh),
                                      0.0f, kLoMax));
    if (!ok) return false;

    /* 4. Whites then Blacks -- set the clip points last. */
    p.whites = std::round(bisect(kWhitesMin, kWhitesMax, kWhiteTarget, kClipIters,
                                 [&](float v) { p.whites = v;
                                                return measure(p, false).percentile(kWhitePct); }));
    p.blacks = std::round(bisect(kBlacksMin, kBlacksMax, kBlackTarget, kClipIters,
                                 [&](float v) { p.blacks = v;
                                                return measure(p, false).percentile(kBlackPct); }));
    if (!ok) return false;

    /* 5. Vibrance / Saturation -- from the finished tone. */
    const Stats s3 = measure(p, true);
    if (!ok) return false;
    {
        const float gap = kChromaTarget - s3.meanChroma;
        float vib = std::clamp(kVibGain * gap, kVibMin, kVibMax);
        float sat = std::clamp(kSatGain * gap, kSatMin, kSatMax);
        /* A positive push fades out toward the gamut edge, and the whole push fades in
           from monochrome -- a ramp, not a cutoff, so a near-grey frame does not jump
           from nothing to the full push across one threshold. */
        const float room = std::clamp(1.0f - (s3.highChroma - kGamutChroma) / kGamutFade,
                                      0.0f, 1.0f);
        if (vib > 0.0f) vib *= room;
        if (sat > 0.0f) sat *= room;
        const float colour = std::clamp((s3.meanChroma - kMonoChroma) /
                                        (kColourChroma - kMonoChroma), 0.0f, 1.0f);
        p.vibrance = std::round(vib * colour);
        p.saturation = std::round(sat * colour);
    }

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

} // namespace AutoTone
