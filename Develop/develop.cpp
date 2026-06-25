#include "Develop/develop.h"
#include <QtConcurrent>
#include <QThreadPool>
#include <QFuture>
#include <QVector>
#include <QtGlobal>
#include <cmath>

namespace {
/* Contrast is applied in a perceptual (~sRGB gamma) domain so the slope steepens the tone
   curve evenly instead of exploding scene-linear highlights. kGamma/kInvGamma encode/decode;
   kContrastSlopeRange keeps the control gentle (full slider = 1 +/- 0.4 slope). */
constexpr float kGamma             = 2.2f;
constexpr float kInvGamma          = 1.0f / kGamma;
constexpr float kContrastSlopeRange = 0.4f;
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

/* SCAFFOLD: spatial op, deferred. Real implementation operates on a neighbourhood and lands
   in a later phase (for RAW, ideally on the CFA mosaic inside the raw pipeline). */
void Develop::Denoise(WorkingImage &img, const EditParams &p) { Q_UNUSED(img) Q_UNUSED(p) }

Develop::PointCoeffs Develop::buildPointCoeffs(const EditParams &p, const WorkingImage &img)
{
    PointCoeffs c;

    /* Exposure: a scene-linear gain of 2^EV (a +1 EV slider doubles the linear value). */
    if (p.exposure != 0.0f) c.exposureGain = std::exp2(p.exposure);

    /* Contrast: a slope about mid-grey applied in a PERCEPTUAL (gamma) domain, NOT in
       scene-linear -- a power curve in linear pivots around 0.18 and sends highlights, which
       sit several stops above the pivot, far too high (it "explodes" the highlights). Encoding
       to ~gamma first puts mid-grey near 0.46 and the slope steepens/flattens the tone curve
       evenly, like a normal contrast control. The slope is deliberately gentle: amount -1..1
       maps to 1 + 0.4*amount -> [0.6, 1.4] (kContrastSlopeRange). >1 steepens, <1 flattens. */
    if (p.contrast != 0.0f) c.contrastSlope = 1.0f + kContrastSlopeRange * p.contrast;

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
