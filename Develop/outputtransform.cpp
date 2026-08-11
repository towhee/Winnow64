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

/* Adobe RGB (1998) transfer: a pure power curve, gamma 563/256 = 2.19921875 exactly (the
   spec's odd number, not 2.2 -- the difference is visible in a gradient). */
constexpr float kAdobeGammaInv = 256.0f / 563.0f;

/*
    Linear working space (sRGB / Rec.709 primaries, D65) -> linear output primaries, row
    major. Both are the product of sRGB->XYZ and XYZ->target with matching D65 white, so
    no chromatic adaptation is involved and each row sums to 1 (white -> white).

    Adobe RGB shares sRGB's red and blue primaries and differs only in green, which is why
    its matrix is mostly identity -- red borrows from green, and blue borrows a little
    green back.
*/
struct Encoding {
    float m[9];         // primaries matrix; unused when identity
    bool  identity;     // sRGB in, sRGB out: skip the matrix entirely
    float gammaInv;     // 0 => sRGB piecewise transfer, else a pure power curve
};

Encoding EncodingFor(OutputTransform::Space space)
{
    switch (space) {
    case OutputTransform::Space::DisplayP3:
        return { { 0.8224620f, 0.1775380f, 0.0000000f,
                   0.0331942f, 0.9668058f, 0.0000000f,
                   0.0170826f, 0.0723974f, 0.9105199f }, false, 0.0f };
    case OutputTransform::Space::AdobeRGB:
        return { { 0.7151256f, 0.2848744f, 0.0000000f,
                   0.0000000f, 1.0000000f, 0.0000000f,
                   0.0000000f, 0.0411619f, 0.9588381f }, false, kAdobeGammaInv };
    case OutputTransform::Space::sRGB:
        break;
    }
    return { { 1,0,0, 0,1,0, 0,0,1 }, true, 0.0f };
}

/* Primaries conversion, in LINEAR -- before the transfer function, after the tone curve.
   Values above 1 are left alone here; the transfer clamps. */
inline void ToPrimaries(const float m[9], float &r, float &g, float &b)
{
    const float R = m[0]*r + m[1]*g + m[2]*b;
    const float G = m[3]*r + m[4]*g + m[5]*b;
    const float B = m[6]*r + m[7]*g + m[8]*b;
    r = R; g = G; b = B;
}

/* Linear -> the output space's transfer function. */
inline float Transfer(float v, float gammaInv)
{
    return gammaInv > 0.0f ? std::pow(Clamp01(v), gammaInv) : SrgbGamma(v);
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

/*
    Highlight rolloff: applied to every render in LINEAR, in the OUTPUT primaries (after
    the matrix, before the transfer) -- that is the space the clip actually happens in.

    Without it the only thing standing between an over-range value and the file is the
    transfer function's Clamp01 -- a hard per-channel clip. That is what makes a
    brightened radial mask look wrong: the core of the mask clips flat, so the smooth
    smootherstep falloff is invisible there and re-appears with a derivative kink at the
    radius where the channel drops back under 1.0 (a Mach-band ring), and because the
    channels clip one at a time the hue swings on the way out.

    Instead the energy the peak channel loses to the clip is bled into the other two, so
    an over-range colour desaturates toward white the way Lightroom's does: pushing a
    saturated green past white lifts red and blue instead of pinning green at 255 and
    freezing the pixel. Measured on the green test frame that puts the mask core at
    ~(154,255,151) against Lightroom's ~(172,255,164), and it removes the flat plateau --
    brightness keeps rising smoothly through the region where green alone is clipped.

    Everything at or below white is left BIT-EXACT: nothing but the over-range part of a
    render can change, so an unedited image (and every export of one) is untouched. That
    rules out a compressive shoulder here -- any curve with a soft knee below 1.0 would
    have to darken existing whites to make room.
*/
inline void HighlightRolloff(float &r, float &g, float &b)
{
    const float mx = qMax(r, qMax(g, b));
    if (mx <= 1.0f) return;                     // in range: the common case, untouched
    const float bleed = mx - 1.0f;              // the part that would have been clipped
    r = qMin(1.0f, r + bleed);
    g = qMin(1.0f, g + bleed);
    b = qMin(1.0f, b + bleed);
}

/*
    Run processRows(y0, y1) over the image's rows, parallelised over disjoint row chunks
    (QtConcurrent + the global pool) exactly as Develop::applyPointOps is. Rows are
    disjoint, so the threads write into the output buffer without contention. Shared by
    the 8-bit and 16-bit transforms -- only the per-pixel packing differs between them.
*/
template <typename RowFn>
void RunRows(int H, RowFn processRows)
{
    const int maxThreads = qMax(1, QThreadPool::globalInstance()->maxThreadCount());
    const int kMinRowsPerChunk = 64;
    if (maxThreads == 1 || H < kMinRowsPerChunk * 2) {
        processRows(0, H);
        return;
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
}

} // namespace

QColorSpace OutputTransform::ColorSpaceOf(Space space)
{
    switch (space) {
    case Space::DisplayP3: return QColorSpace(QColorSpace::DisplayP3);
    case Space::AdobeRGB:  return QColorSpace(QColorSpace::AdobeRgb);
    case Space::sRGB:      break;
    }
    return QColorSpace(QColorSpace::SRgb);
}

bool OutputTransform::ToImage(const WorkingImage &img, QImage &out, Space space)
{
    if (!img.isValid()) return false;

    const int W = img.width;
    const int H = img.height;
    const float scale = img.white > 0.0f ? 1.0f / img.white : 1.0f;

    out = QImage(W, H, QImage::Format_RGB888);
    if (out.isNull()) return false;

    const bool tone = img.sceneReferred;    // baseline tone curve for RAW only
    const Encoding enc = EncodingFor(space);
    const float *rgb = img.rgb.data();
    uchar *bits = out.bits();
    const qsizetype bpl = out.bytesPerLine();

    /* Per-pixel float -> 8-bit: the (optional) baseline tone curve, then the primaries
       matrix in linear, then the transfer function. This is the dominant slider-drag cost
       (per-pixel pow), so it runs over parallel row chunks (RunRows). The sRGB path skips
       the matrix outright rather than multiplying by an identity. */
    auto processRows = [=](int y0, int y1) {
        for (int y = y0; y < y1; ++y) {
            uchar *line = bits + static_cast<qsizetype>(y) * bpl;
            const size_t base = static_cast<size_t>(y) * W * 3;
            for (int x = 0; x < W; ++x) {
                const size_t o = base + static_cast<size_t>(x) * 3;
                float v[3];
                for (int c = 0; c < 3; ++c) {
                    v[c] = rgb[o + c] * scale;
                    if (tone) v[c] = BaselineTone(v[c]);
                }
                if (!enc.identity) ToPrimaries(enc.m, v[0], v[1], v[2]);
                HighlightRolloff(v[0], v[1], v[2]);
                for (int c = 0; c < 3; ++c)
                    line[x * 3 + c] = static_cast<uchar>(
                        std::lround(Transfer(v[c], enc.gammaInv) * 255.0f));
            }
        }
    };

    RunRows(H, processRows);
    return true;
}

bool OutputTransform::ToImage16(const WorkingImage &img, QImage &out, Space space)
{
/*
    16-bit twin of ToImage, for the export path. Identical maths -- the same baseline tone
    curve, primaries and transfer -- quantised to 16 bits per channel into
    Format_RGBX64 (four quint16 per pixel: R, G, B, unused-opaque). The develop pipeline
    is float
    throughout, so this is the only stage that loses precision; at 16 bits the loss is
    below what any downstream edit can reveal.
*/
    if (!img.isValid()) return false;

    const int W = img.width;
    const int H = img.height;
    const float scale = img.white > 0.0f ? 1.0f / img.white : 1.0f;

    out = QImage(W, H, QImage::Format_RGBX64);
    if (out.isNull()) return false;

    const bool tone = img.sceneReferred;    // baseline tone curve for RAW only
    const Encoding enc = EncodingFor(space);
    const float *rgb = img.rgb.data();
    uchar *bits = out.bits();
    const qsizetype bpl = out.bytesPerLine();

    auto processRows = [=](int y0, int y1) {
        for (int y = y0; y < y1; ++y) {
            quint16 *line = reinterpret_cast<quint16*>(bits + static_cast<qsizetype>(y) * bpl);
            const size_t base = static_cast<size_t>(y) * W * 3;
            for (int x = 0; x < W; ++x) {
                const size_t o = base + static_cast<size_t>(x) * 3;
                float v[3];
                for (int c = 0; c < 3; ++c) {
                    v[c] = rgb[o + c] * scale;
                    if (tone) v[c] = BaselineTone(v[c]);
                }
                if (!enc.identity) ToPrimaries(enc.m, v[0], v[1], v[2]);
                HighlightRolloff(v[0], v[1], v[2]);
                for (int c = 0; c < 3; ++c)
                    line[x * 4 + c] = static_cast<quint16>(
                        std::lround(Transfer(v[c], enc.gammaInv) * 65535.0f));
                line[x * 4 + 3] = 0xFFFF;   // RGBX64: the fourth channel is unused/opaque
            }
        }
    };

    RunRows(H, processRows);
    return true;
}
