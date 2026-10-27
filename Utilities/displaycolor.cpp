#include "displaycolor.h"
#include "Main/global.h"
#include "Lcms2/lcms2.h"

#include <opencv2/core/utility.hpp>     // parallel_for_
#include <QFile>
#include <QMutex>
#include <memory>

namespace
{
/* One transform per pixel layout the render paths produce. Built together so ToDisplay
   never creates one on a worker thread. */
struct State
{
    cmsHTRANSFORM rgb = nullptr;        // Format_RGB888
    cmsHTRANSFORM bgra = nullptr;       // Format_RGB32 / ARGB32 (little-endian BGRA)
    cmsHTRANSFORM bgraPremul = nullptr; // Format_ARGB32_Premultiplied
    cmsHTRANSFORM rgba = nullptr;       // Format_RGBX8888 / RGBA8888
    cmsHTRANSFORM rgbaPremul = nullptr; // Format_RGBA8888_Premultiplied

    ~State()
    {
        for (cmsHTRANSFORM t : {rgb, bgra, bgraPremul, rgba, rgbaPremul})
            if (t) cmsDeleteTransform(t);
    }
    bool complete() const { return rgb && bgra && bgraPremul && rgba && rgbaPremul; }
};

QMutex stateMutex;
std::shared_ptr<const State> state;     // null = inactive

std::shared_ptr<const State> current()
{
    QMutexLocker lock(&stateMutex);
    return state;
}

/* True when the display renders sRGB within one level everywhere it was sampled -- the
   conversion would be a slow copy. Samples the primaries, secondaries, a skin tone and
   a grey ramp, which is where a gamut or tone difference shows first. */
bool isIdentity(cmsHTRANSFORM bgra)
{
    static const quint32 samples[] = {
        0xffff0000, 0xff00ff00, 0xff0000ff, 0xffffff00, 0xff00ffff, 0xffff00ff,
        0xffc08070, 0xffe0a890, 0xff804030, 0xff202020, 0xff606060, 0xffa0a0a0,
        0xffe0e0e0, 0xffffffff, 0xff7f3f9f, 0xff3f9f7f};
    const int n = int(sizeof(samples) / sizeof(samples[0]));
    quint32 out[sizeof(samples) / sizeof(samples[0])];
    cmsDoTransform(bgra, samples, out, cmsUInt32Number(n));
    for (int i = 0; i < n; ++i) {
        for (int shift = 0; shift < 24; shift += 8) {
            const int a = int((samples[i] >> shift) & 0xff);
            const int b = int((out[i] >> shift) & 0xff);
            if (qAbs(a - b) > 1) return false;
        }
    }
    return true;
}
} // namespace

namespace DisplayColor
{

void Rebuild()
{
    if (G::isLogger) G::log("DisplayColor::Rebuild", G::winOutProfilePath);

    std::shared_ptr<const State> next;
    if (!G::winOutProfilePath.isEmpty()) {
        cmsHPROFILE in = cmsCreate_sRGBProfile();
        cmsHPROFILE out = cmsOpenProfileFromFile(
            QFile::encodeName(G::winOutProfilePath).constData(), "r");
        if (in && out) {
            /* NOCACHE: the same transform runs on many threads at once, and lcms's
               one-pixel cache is per transform. COPY_ALPHA: input and output are
               different buffers, so alpha is not otherwise carried. */
            const cmsUInt32Number flags = cmsFLAGS_NOCACHE | cmsFLAGS_COPY_ALPHA;
            auto make = [&](cmsUInt32Number type) {
                return cmsCreateTransform(in, type, out, type, INTENT_PERCEPTUAL, flags);
            };
            auto s = std::make_shared<State>();
            s->rgb = make(TYPE_RGB_8);
            s->bgra = make(TYPE_BGRA_8);
            s->bgraPremul = make(TYPE_BGRA_8_PREMUL);
            s->rgba = make(TYPE_RGBA_8);
            s->rgbaPremul = make(TYPE_RGBA_8_PREMUL);
            if (!s->complete())
                G::issue("Warning", "Could not build the display colour transform.",
                         "DisplayColor::Rebuild", -1, G::winOutProfilePath);
            else if (!isIdentity(s->bgra))
                next = s;
        }
        /* A transform keeps what it needs; the profiles can go now. */
        if (in) cmsCloseProfile(in);
        if (out) cmsCloseProfile(out);
    }

    QMutexLocker lock(&stateMutex);
    state = next;
}

bool IsActive()
{
    return current() != nullptr;
}

QImage ToDisplay(const QImage &srgb)
{
    const std::shared_ptr<const State> s = current();
    if (!s || srgb.isNull()) return srgb;

    QImage src = srgb;
    cmsHTRANSFORM t = nullptr;
    switch (src.format()) {
    case QImage::Format_RGB888:                 t = s->rgb;        break;
    case QImage::Format_RGB32:
    case QImage::Format_ARGB32:                 t = s->bgra;       break;
    case QImage::Format_ARGB32_Premultiplied:   t = s->bgraPremul; break;
    case QImage::Format_RGBX8888:
    case QImage::Format_RGBA8888:               t = s->rgba;       break;
    case QImage::Format_RGBA8888_Premultiplied: t = s->rgbaPremul; break;
    default:
        /* 16-bit and indexed frames are not shown by the paths that call this; convert
           rather than show them unmanaged if one ever is. */
        src = srgb.convertToFormat(QImage::Format_ARGB32);
        t = s->bgra;
        break;
    }

    QImage dst(src.size(), src.format());
    if (dst.isNull()) return srgb;
    dst.setDevicePixelRatio(src.devicePixelRatio());

    const int W = src.width();
    const int H = src.height();
    const qsizetype inStride = src.bytesPerLine();
    const qsizetype outStride = dst.bytesPerLine();
    const uchar *in = src.constBits();
    uchar *out = dst.bits();
    auto strip = [&](int y0, int y1) {
        cmsDoTransformLineStride(t, in + y0 * inStride, out + y0 * outStride,
                                 cmsUInt32Number(W), cmsUInt32Number(y1 - y0),
                                 cmsUInt32Number(inStride), cmsUInt32Number(outStride),
                                 0, 0);
    };
    constexpr int kRows = 64;
    const int nStrips = (H + kRows - 1) / kRows;
    if (nStrips <= 1) {
        strip(0, H);
    }
    else {
        cv::parallel_for_(cv::Range(0, nStrips), [&](const cv::Range &r) {
            for (int i = r.start; i < r.end; ++i)
                strip(i * kRows, qMin(H, (i + 1) * kRows));
        });
    }
    return dst;
}

} // namespace DisplayColor
