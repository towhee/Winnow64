/*
    WHEN COPY NEW C CODE FROM LCMS2 AT https://github.com/mm2/Little-CMS

    1. Copy .c .h files from src and include to Lcms2 in Winnow64

    2. In lcms2.h uncomment as per:

        // Uncomment this to remove the "register" storage class
        // #define CMS_NO_REGISTER_KEYWORD 1

*/

#include "icc.h"
#include "Main/global.h"

#include <opencv2/core/utility.hpp>     // parallel_for_
#include <algorithm>
#include <cmath>
#include <memory>

namespace
{
/*
    The fast kernel for a matrix/TRC source and a matrix/TRC display. Decoding a channel
    and multiplying by its matrix column are folded into one table, so a pixel is nine
    integer lookups, six adds, three clamps and three encode lookups:

        out = encode( sum_c  column_c(M) * decode_c(v_c) ),   M = inv(Mdisplay) . Msource

    Fixed point: col[c][v] holds the three products scaled by (kEnc - 1) << 16, so the
    sum indexes the kEnc-entry encode tables directly. Measured 2026-10-26 on a 50 MP
    preview: 130 ms single-threaded against lcms's 296 ms, max one level apart (sRGB ->
    Display P3, a MacBook profile, a BenQ SW320); the profiles' colorants are both D50
    PCS, so no adaptation is needed beyond what the tags already carry.
*/
constexpr int kEnc = 4096;
struct Col { qint32 r, g, b, pad; };

struct Transform
{
    ICC::Path path = ICC::Path::None;
    cmsHTRANSFORM lcms = nullptr;       // always built: the Lcms path and the probes
    Col col[3][256];
    quint8 enc[3][kEnc];

    ~Transform() { if (lcms) cmsDeleteTransform(lcms); }
};

QMutex mutex;                           // guards outProfile and cache
cmsHPROFILE outProfile = nullptr;
QHash<QByteArray, std::shared_ptr<const Transform>> cache;  // key: the source ICC bytes
constexpr int kMaxCached = 64;          // distinct embedded profiles in one session

struct MatrixShaper
{
    double m[3][3];                     // RGB -> XYZ (D50 PCS), columns = colorants
    const cmsToneCurve *trc[3];
};

/* A profile the kernel can stand in for lcms on: RGB, colorants + TRCs, and NO CLUT in
   the direction it is used (lcms prefers a CLUT when one exists, so the matrix would not
   be what lcms does). */
bool readMatrixShaper(cmsHPROFILE p, cmsUInt32Number direction, MatrixShaper &ms)
{
    if (cmsGetColorSpace(p) != cmsSigRgbData || !cmsIsMatrixShaper(p)) return false;
    if (cmsIsCLUT(p, INTENT_PERCEPTUAL, direction)) return false;
    const cmsTagSignature colorant[3] = {cmsSigRedColorantTag, cmsSigGreenColorantTag,
                                         cmsSigBlueColorantTag};
    const cmsTagSignature trc[3] = {cmsSigRedTRCTag, cmsSigGreenTRCTag, cmsSigBlueTRCTag};
    for (int c = 0; c < 3; ++c) {
        const auto *xyz = static_cast<const cmsCIEXYZ *>(cmsReadTag(p, colorant[c]));
        ms.trc[c] = static_cast<const cmsToneCurve *>(cmsReadTag(p, trc[c]));
        if (!xyz || !ms.trc[c]) return false;
        ms.m[0][c] = xyz->X;
        ms.m[1][c] = xyz->Y;
        ms.m[2][c] = xyz->Z;
    }
    return true;
}

bool invert3(const double a[3][3], double r[3][3])
{
    const double d = a[0][0] * (a[1][1] * a[2][2] - a[1][2] * a[2][1])
                   - a[0][1] * (a[1][0] * a[2][2] - a[1][2] * a[2][0])
                   + a[0][2] * (a[1][0] * a[2][1] - a[1][1] * a[2][0]);
    if (std::abs(d) < 1e-12) return false;
    r[0][0] = (a[1][1] * a[2][2] - a[1][2] * a[2][1]) / d;
    r[0][1] = (a[0][2] * a[2][1] - a[0][1] * a[2][2]) / d;
    r[0][2] = (a[0][1] * a[1][2] - a[0][2] * a[1][1]) / d;
    r[1][0] = (a[1][2] * a[2][0] - a[1][0] * a[2][2]) / d;
    r[1][1] = (a[0][0] * a[2][2] - a[0][2] * a[2][0]) / d;
    r[1][2] = (a[0][2] * a[1][0] - a[0][0] * a[1][2]) / d;
    r[2][0] = (a[1][0] * a[2][1] - a[1][1] * a[2][0]) / d;
    r[2][1] = (a[0][1] * a[2][0] - a[0][0] * a[2][1]) / d;
    r[2][2] = (a[0][0] * a[1][1] - a[0][1] * a[1][0]) / d;
    return true;
}

bool buildKernel(const MatrixShaper &src, const MatrixShaper &dst, Transform &t)
{
    double inv[3][3];
    if (!invert3(dst.m, inv)) return false;
    double m[3][3];
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j)
            m[i][j] = inv[i][0] * src.m[0][j] + inv[i][1] * src.m[1][j]
                      + inv[i][2] * src.m[2][j];
    const double scale = double(kEnc - 1) * 65536.0;
    for (int c = 0; c < 3; ++c) {
        for (int v = 0; v < 256; ++v) {
            const double lin = cmsEvalToneCurveFloat(src.trc[c], cmsFloat32Number(v / 255.0));
            t.col[c][v] = {qint32(std::lround(m[0][c] * lin * scale)),
                           qint32(std::lround(m[1][c] * lin * scale)),
                           qint32(std::lround(m[2][c] * lin * scale)), 0};
        }
        cmsToneCurve *rev = cmsReverseToneCurveEx(4096, dst.trc[c]);
        if (!rev) return false;
        for (int v = 0; v < kEnc; ++v) {
            const float e = cmsEvalToneCurveFloat(rev, cmsFloat32Number(v) / (kEnc - 1));
            t.enc[c][v] = quint8(std::lround(std::clamp(e, 0.0f, 1.0f) * 255.0f));
        }
        cmsFreeToneCurve(rev);
    }
    return true;
}

inline quint32 kernelPixel(const Transform &t, quint32 p)
{
    constexpr qint32 hi = (kEnc - 1) << 16;
    const Col &x = t.col[0][(p >> 16) & 255];
    const Col &y = t.col[1][(p >> 8) & 255];
    const Col &z = t.col[2][p & 255];
    const qint32 r = std::min(std::max(x.r + y.r + z.r + 32768, 0), hi) >> 16;
    const qint32 g = std::min(std::max(x.g + y.g + z.g + 32768, 0), hi) >> 16;
    const qint32 b = std::min(std::max(x.b + y.b + z.b + 32768, 0), hi) >> 16;
    return (p & 0xff000000u) | (quint32(t.enc[0][r]) << 16) | (quint32(t.enc[1][g]) << 8)
           | t.enc[2][b];
}

/* Saturated colours, a skin tone and a grey ramp -- where a gamut or tone difference
   shows first. Used both to call a transform an identity and to vet the kernel. */
const quint32 kProbe[] = {
    0xffff0000, 0xff00ff00, 0xff0000ff, 0xffffff00, 0xff00ffff, 0xffff00ff, 0xffc08070,
    0xffe0a890, 0xff804030, 0xff202020, 0xff606060, 0xffa0a0a0, 0xffe0e0e0, 0xffffffff,
    0xff7f3f9f, 0xff3f9f7f, 0xff050505, 0xff000000};
constexpr int kProbeN = int(sizeof(kProbe) / sizeof(kProbe[0]));

int maxChannelDiff(quint32 a, quint32 b)
{
    int worst = 0;
    for (int s = 0; s < 24; s += 8)
        worst = std::max(worst, std::abs(int((a >> s) & 255) - int((b >> s) & 255)));
    return worst;
}

/* Under mutex. Null when there is no display profile (Linux has no lookup) or the
   source profile is unusable -- transform() then leaves the image alone. */
std::shared_ptr<const Transform> build(const QByteArray &buf)
{
    if (!outProfile) return nullptr;
    cmsHPROFILE in = buf.isEmpty()
        ? cmsCreate_sRGBProfile()
        : cmsOpenProfileFromMem(buf.constData(), cmsUInt32Number(buf.size()));
    if (!in) {
        G::issue("Warning", "ICC hInProfile failed.", "ICC::transform", -1,
                 G::winOutProfilePath);
        return nullptr;
    }
    auto t = std::make_shared<Transform>();
    /* NOCACHE: one transform runs on many decoder threads and strips at once. */
    t->lcms = cmsCreateTransform(in, TYPE_BGRA_8, outProfile, TYPE_BGRA_8,
                                 INTENT_PERCEPTUAL, cmsFLAGS_NOCACHE);
    if (!t->lcms) {
        G::issue("Warning", "ICC cmsCreateTransform failed.", "ICC::transform", -1,
                 G::winOutProfilePath);
        cmsCloseProfile(in);
        return nullptr;
    }

    quint32 ref[kProbeN];
    cmsDoTransform(t->lcms, kProbe, ref, cmsUInt32Number(kProbeN));
    bool identity = true;
    for (int i = 0; i < kProbeN && identity; ++i)
        identity = maxChannelDiff(kProbe[i], ref[i]) <= 1;

    if (identity) {
        t->path = ICC::Path::Identity;
    }
    else {
        t->path = ICC::Path::Lcms;
        MatrixShaper src, dst;
        if (readMatrixShaper(in, LCMS_USED_AS_INPUT, src)
            && readMatrixShaper(outProfile, LCMS_USED_AS_OUTPUT, dst)
            && buildKernel(src, dst, *t)) {
            /* Vetted against lcms itself: a profile that reads as matrix/TRC but that
               lcms renders differently (an odd v2 profile, a curve the reverse cannot
               follow) stays on lcms rather than drifting. */
            bool agrees = true;
            for (int i = 0; i < kProbeN && agrees; ++i)
                agrees = maxChannelDiff(kernelPixel(*t, kProbe[i]), ref[i]) <= 1;
            if (agrees) t->path = ICC::Path::Fast;
        }
    }
    cmsCloseProfile(in);
    return t;
}

std::shared_ptr<const Transform> transformFor(const QByteArray &buf)
{
    QMutexLocker lock(&mutex);
    auto it = cache.constFind(buf);
    if (it != cache.constEnd()) return it.value();
    std::shared_ptr<const Transform> t = build(buf);
    if (cache.size() >= kMaxCached) cache.clear();
    cache.insert(buf, t);               // a null result is cached too: no retry per image
    return t;
}
} // namespace

namespace ICC
{

void err(cmsContext contextID, cmsUInt32Number errorCode, const char *text)
{
    Q_UNUSED(contextID)
    Q_UNUSED(errorCode)
    G::issue("Warning", text, "ICC::err", -1, G::winOutProfilePath);
}

bool setOutProfile()
{
    if (G::isLogger) G::log("ICC::setOutProfile");
    cmsSetLogErrorHandler(err);
    QMutexLocker lock(&mutex);
    /* Every cached transform targets the old display. Images mid-transform keep theirs
       alive (shared_ptr); the next lookup builds for the new one. */
    cache.clear();
    if (outProfile) {
        cmsCloseProfile(outProfile);
        outProfile = nullptr;
    }
    if (G::winOutProfilePath == "") {
        G::issue("Warning", "The outProfilePath has not been assigned.",
                 "ICC::setOutProfile", -1, G::winOutProfilePath);
        return false;
    }
    outProfile = cmsOpenProfileFromFile(QFile::encodeName(G::winOutProfilePath).constData(),
                                        "r");
    if (!outProfile) {
        G::issue("Error", "cmsOpenProfileFromFile returned null — output profile invalid",
                 "ICC::setOutProfile", -1, G::winOutProfilePath);
        return false;
    }
    return true;
}

Path pathFor(const QByteArray &buf)
{
    const std::shared_ptr<const Transform> t = transformFor(buf);
    return t ? t->path : Path::None;
}

void transform(const QByteArray &buf, QImage &image)
{
    if (G::isLogger) G::log("ICC::transform");
    if (image.isNull()) return;
    const std::shared_ptr<const Transform> t = transformFor(buf);
    if (!t || t->path == Path::Identity) return;

    if (image.depth() != 32) {
        image = image.convertToFormat(QImage::Format_ARGB32);
        if (image.isNull()) return;
    }
    const int W = image.width();
    const int H = image.height();
    const qsizetype stride = image.bytesPerLine();
    uchar *bits = image.bits();
    const bool fast = t->path == Path::Fast;
    auto strip = [&](int y0, int y1) {
        if (fast) {
            for (int y = y0; y < y1; ++y) {
                quint32 *p = reinterpret_cast<quint32 *>(bits + y * stride);
                for (int x = 0; x < W; ++x) p[x] = kernelPixel(*t, p[x]);
            }
        }
        else {
            cmsDoTransformLineStride(t->lcms, bits + y0 * stride, bits + y0 * stride,
                                     cmsUInt32Number(W), cmsUInt32Number(y1 - y0),
                                     cmsUInt32Number(stride), cmsUInt32Number(stride), 0, 0);
        }
    };
    /* Strips cut the latency of the image the user is waiting on (a cache miss while
       riffling); several decoders running at once just share the pool. */
    constexpr int kRows = 64;
    const int nStrips = (H + kRows - 1) / kRows;
    if (nStrips <= 1) {
        strip(0, H);
        return;
    }
    cv::parallel_for_(cv::Range(0, nStrips), [&](const cv::Range &r) {
        for (int i = r.start; i < r.end; ++i) strip(i * kRows, std::min(H, (i + 1) * kRows));
    });
}

} // namespace ICC
