#include "Develop/outputtransform.h"
#include <QtConcurrent>
#include <QThreadPool>
#include <QFuture>
#include <QVector>
#include <QtGlobal>
#include <cmath>

namespace {

inline float Clamp01(float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }

/* Linear -> sRGB transfer (IEC 61966-2-1). */
inline float SrgbGamma(float v)
{
    v = Clamp01(v);
    return v <= 0.0031308f ? 12.92f * v
                           : 1.055f * std::pow(v, 1.0f / 2.4f) - 0.055f;
}

/*
    Baseline tone curve for scene-referred (RAW) data: a fixed exposure lift followed by the
    ACES (Narkowicz) filmic shoulder, applied in linear before the sRGB transfer. A raw render
    is scene-linear and otherwise lands dark and flat next to the camera's JPEG; this lifts the
    midtones (~+0.7 EV) and rolls highlights off smoothly instead of hard-clipping. It is a
    fixed default look (not per-image auto-exposure) -- tune kBaselineExposure to taste, or let
    a future Develop "exposure" override it. Validated against the A9 II embedded preview.
    Display-referred input skips this (it already carries the camera tone curve).
*/
constexpr float kBaselineExposure = 1.6f;       // ~ +0.68 EV

inline float BaselineTone(float v)
{
    v *= kBaselineExposure;
    if (v < 0.0f) v = 0.0f;
    const float a = 2.51f, b = 0.03f, c = 2.43f, d = 0.59f, e = 0.14f;
    return Clamp01((v * (a * v + b)) / (v * (c * v + d) + e));
}

} // namespace

bool OutputTransform::ToImage(const WorkingImage &img, QImage &out)
{
    if (!img.isValid()) return false;

    const int W = img.width;
    const int H = img.height;
    const float scale = img.white > 0.0f ? 1.0f / img.white : 1.0f;

    out = QImage(W, H, QImage::Format_RGB888);
    if (out.isNull()) return false;

    const bool tone = img.sceneReferred;    // baseline tone curve for RAW only
    const float *rgb = img.rgb.data();
    uchar *bits = out.bits();
    const qsizetype bpl = out.bytesPerLine();

    /* Per-pixel float -> 8-bit with the (optional) baseline tone curve and the sRGB transfer.
       This is the dominant slider-drag cost (per-pixel pow), so it is parallelised over row
       ranges the same way Develop::applyPointOps is (QtConcurrent + the global pool). Rows are
       disjoint, so the threads write into out's buffer without contention. */
    auto processRows = [=](int y0, int y1) {
        for (int y = y0; y < y1; ++y) {
            uchar *line = bits + static_cast<qsizetype>(y) * bpl;
            const size_t base = static_cast<size_t>(y) * W * 3;
            for (int x = 0; x < W; ++x) {
                const size_t o = base + static_cast<size_t>(x) * 3;
                for (int c = 0; c < 3; ++c) {
                    float v = rgb[o + c] * scale;
                    if (tone) v = BaselineTone(v);
                    line[x * 3 + c] = static_cast<uchar>(std::lround(SrgbGamma(v) * 255.0f));
                }
            }
        }
    };

    const int maxThreads = qMax(1, QThreadPool::globalInstance()->maxThreadCount());
    const int kMinRowsPerChunk = 64;
    if (maxThreads == 1 || H < kMinRowsPerChunk * 2) {
        processRows(0, H);
        return true;
    }

    const int chunks = qMin(maxThreads, (H + kMinRowsPerChunk - 1) / kMinRowsPerChunk);
    const int rowsPerChunk = (H + chunks - 1) / chunks;
    QVector<QFuture<void>> futures;
    futures.reserve(chunks);
    for (int k = 0; k < chunks; ++k) {
        const int y0 = k * rowsPerChunk;
        const int y1 = qMin(H, y0 + rowsPerChunk);
        if (y0 >= y1) break;
        futures.append(QtConcurrent::run(QThreadPool::globalInstance(),
                                         [processRows, y0, y1]() { processRows(y0, y1); }));
    }
    for (QFuture<void> &f : futures) f.waitForFinished();
    return true;
}
