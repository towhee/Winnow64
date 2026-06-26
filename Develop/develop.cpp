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
}

bool Develop::Apply(WorkingImage &img, const EditParams &p)
{
    if (!img.isValid()) return false;
    if (p.isIdentity()) return true;    // nothing to do; serve image as-is

    /* Fixed pipeline order: spatial ops own a pass, point ops share the fused pass.
       See notes/Documentation.txt "DEVELOP / IMAGE EDIT". */
    Denoise(img, p);

    PointCoeffs c = buildPointCoeffs(p, img);
    if (c.active) applyPointOps(img, c);
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

Develop::PointCoeffs Develop::buildPointCoeffs(const EditParams &p, const WorkingImage &img)
{
    PointCoeffs c;

    /* Exposure: a scene-linear gain of 2^EV (a +1 EV slider doubles the linear value). */
    if (p.exposure != 0.0f) c.exposureGain = std::exp2(p.exposure);

    /* Contrast: a slope about mid-grey applied in a PERCEPTUAL (gamma) domain, NOT in
       scene-linear -- a power curve in linear pivots around 0.18 and sends highlights, which
       sit several stops above the pivot, far too high (it "explodes" the highlights). Encoding
       to ~gamma first puts mid-grey near 0.46 and the slope steepens/flattens the tone curve
       evenly, like a normal contrast control. The slope is deliberately gentle: the slider's
       -100..100 value is normalised to amount -1..1, which maps to 1 + 0.4*amount -> [0.6, 1.4]
       (kContrastSlopeRange). >1 steepens, <1 flattens. */
    if (p.contrast != 0.0f)
        c.contrastSlope = 1.0f + kContrastSlopeRange * (p.contrast / kContrastFullScale);

    c.white = (img.white > 0.0f ? img.white : 1.0f);

    c.active = (c.exposureGain != 1.0f) || (c.contrastSlope != 1.0f);
    return c;
}

void Develop::applyPointOps(WorkingImage &img, const PointCoeffs &c)
{
    const int w = img.width;
    const int h = img.height;
    float *rgb = img.rgb.data();

    const float gain  = c.exposureGain;
    const float slope = c.contrastSlope;
    const float white = c.white;
    const float invWhite = 1.0f / white;
    const bool doExposure = (gain != 1.0f);
    const bool doContrast = (slope != 1.0f);

    /* Contrast works in a perceptual (gamma) domain: encode the white-normalised value with
       kInvGamma, steepen/flatten about mid-grey (which lands at pivotGamma there), decode with
       kGamma. Constants are local so they fold into the inner loop. */
    const float pivotGamma = std::pow(0.18f, kInvGamma);

    /* One fused per-pixel kernel over a band of rows. Point ops apply per channel (R,G,B
       interleaved), so the inner loop walks w*3 floats. Exposure then contrast, matching the
       fixed pipeline order. */
    auto processRows = [=](int y0, int y1) {
        const int span = w * 3;
        for (int y = y0; y < y1; ++y) {
            float *row = rgb + static_cast<size_t>(y) * span;
            for (int i = 0; i < span; ++i) {
                float v = row[i];
                if (doExposure) v *= gain;
                if (doContrast) {
                    /* Perceptual-domain slope about mid-grey; clamps keep pow() positive and
                       crush blacks gracefully rather than producing negatives. */
                    float n = v * invWhite;
                    if (n < 0.0f) n = 0.0f;
                    float s = std::pow(n, kInvGamma);
                    s = pivotGamma + (s - pivotGamma) * slope;
                    if (s < 0.0f) s = 0.0f;
                    v = std::pow(s, kGamma) * white;
                }
                row[i] = v;
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
