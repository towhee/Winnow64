#include "Develop/inputtransform.h"
#include "Develop/whitebalance.h"
#include "Lcms2/lcms2.h"
#include <opencv2/core/utility.hpp>     // parallel_for_
#include <algorithm>
#include <cmath>

namespace {

/* XYZ -> linear sRGB (D65), the "camera matrix" of a display-referred file. */
const float kXyzToSrgbIn[3][3] = {
    { 3.2404542f, -1.5371385f, -0.4985314f},
    {-0.9692660f,  1.8760108f,  0.0415560f},
    { 0.0556434f, -0.2040259f,  1.0572252f}
};

/* sRGB -> linear (inverse of IEC 61966-2-1). */
inline float SrgbInvGamma(float v)
{
    return v <= 0.04045f ? v / 12.92f
                         : std::pow((v + 0.055f) / 1.055f, 2.4f);
}

/* The linear working space as an lcms profile, built from ColorSpaceMath's own RGB->XYZ
   matrix (the primaries are its columns, the white its row sums), so it can never drift
   from kWorking. */
cmsHPROFILE CreateWorkingProfile()
{
    const ColorSpaceMath::Matrix3 m = ColorSpaceMath::rgbToXyz(ColorSpaceMath::kWorking);
    auto xyY = [](double X, double Y, double Z) {
        const double sum = X + Y + Z;
        return cmsCIExyY{X / sum, Y / sum, 1.0};
    };
    const cmsCIExyY white = xyY(m.m[0][0] + m.m[0][1] + m.m[0][2],
                                m.m[1][0] + m.m[1][1] + m.m[1][2],
                                m.m[2][0] + m.m[2][1] + m.m[2][2]);
    const cmsCIExyYTRIPLE primaries = {xyY(m.m[0][0], m.m[1][0], m.m[2][0]),
                                       xyY(m.m[0][1], m.m[1][1], m.m[2][1]),
                                       xyY(m.m[0][2], m.m[1][2], m.m[2][2])};
    cmsToneCurve *linear = cmsBuildGamma(nullptr, 1.0);
    cmsToneCurve *curves[3] = {linear, linear, linear};
    cmsHPROFILE p = cmsCreateRGBProfile(&white, &primaries, curves);
    cmsFreeToneCurve(linear);
    return p;
}

/* src (RGB888, the file's own encoding) through its embedded profile straight to the
   linear working space. False -- leaving out untouched -- if the profile is not an RGB
   profile lcms can use, so the caller falls back to sRGB. Float output is unbounded on
   a matrix-shaper profile, so a colour outside the working gamut is carried, not
   clipped. */
bool FromIcc(const QImage &src, const QByteArray &icc, std::vector<float> &rgb)
{
    cmsHPROFILE inProfile =
        cmsOpenProfileFromMem(icc.constData(), cmsUInt32Number(icc.size()));
    if (!inProfile) return false;
    if (cmsGetColorSpace(inProfile) != cmsSigRgbData) {
        cmsCloseProfile(inProfile);
        return false;
    }
    cmsHPROFILE outProfile = CreateWorkingProfile();
    /* Relative colorimetric: an input conversion characterises, it does not render.
       NOCACHE: the strips below share the one transform across threads. */
    cmsHTRANSFORM t = outProfile
        ? cmsCreateTransform(inProfile, TYPE_RGB_8, outProfile, TYPE_RGB_FLT,
                             INTENT_RELATIVE_COLORIMETRIC, cmsFLAGS_NOCACHE)
        : nullptr;
    cmsCloseProfile(inProfile);
    if (outProfile) cmsCloseProfile(outProfile);
    if (!t) return false;

    const int W = src.width();
    const int H = src.height();
    rgb.assign(static_cast<size_t>(W) * static_cast<size_t>(H) * 3, 0.0f);
    const qsizetype inStride = src.bytesPerLine();
    const cmsUInt32Number outStride = cmsUInt32Number(W) * 3 * sizeof(float);
    constexpr int kRows = 64;
    cv::parallel_for_(cv::Range(0, (H + kRows - 1) / kRows), [&](const cv::Range &r) {
        for (int i = r.start; i < r.end; ++i) {
            const int y0 = i * kRows;
            const int y1 = std::min(H, y0 + kRows);
            cmsDoTransformLineStride(t, src.constBits() + y0 * inStride,
                                     rgb.data() + static_cast<size_t>(y0) * W * 3,
                                     cmsUInt32Number(W), cmsUInt32Number(y1 - y0),
                                     cmsUInt32Number(inStride), outStride, 0, 0);
        }
    });
    cmsDeleteTransform(t);
    return true;
}

} // namespace

bool InputTransform::FromImage(const QImage &in, WorkingImage &out, const QByteArray &icc)
{
    if (in.isNull()) return false;

    const QImage src = (in.format() == QImage::Format_RGB888)
                           ? in
                           : in.convertToFormat(QImage::Format_RGB888);
    const int W = src.width();
    const int H = src.height();

    out.width = W;
    out.height = H;
    out.white = 1.0f;
    out.sceneReferred = false;  // display-referred: already carries the camera tone curve
    /* Unlike the raw path this arrives ALREADY converted: the sRGB -> working matrix is
       applied in the pixel loop below, so Develop's stage 0 is a no-op here and the
       cheap diagonal-gain path in applyPointOps is what runs. */
    out.space = ColorSpaceMath::kWorking;

    /* An embedded profile: the pixels go straight to the working space, so the synthetic
       camera IS the working space (XYZ -> working, then an identity camToWorking) -- the
       same XYZ -> working chain the sRGB camera below composes to, so the white-balance
       solver reads the as-shot white the same way (D65). */
    if (!icc.isEmpty() && FromIcc(src, icc, out.rgb)) {
        const ColorSpaceMath::Matrix3 xyzToWork =
            ColorSpaceMath::xyzToRgb(ColorSpaceMath::kWorking);
        out.cam.valid = true;
        for (int i = 0; i < 3; ++i) {
            out.cam.asShotMul[i] = 1.0f;
            for (int j = 0; j < 3; ++j) {
                out.cam.xyzToCam[i][j] = static_cast<float>(xyzToWork.m[i][j]);
                out.cam.camToWorking[i][j] = i == j ? 1.0f : 0.0f;
            }
        }
        WhiteBalance::resolveAsShot(out.cam);
        return true;
    }

    /* sRGB primaries -> the working space. Identity while the working space is sRGB. */
    float toWork[3][3];
    ColorSpaceMath::matrixF(ColorSpaceMath::ColorSpace::LinearSRGB,
                            ColorSpaceMath::kWorking, toWork);
    const bool convert = (ColorSpaceMath::kWorking != ColorSpaceMath::ColorSpace::LinearSRGB);

    /* A synthetic "camera" whose response IS linear sRGB, so the absolute white balance
       works uniformly on non-raw files: nothing was baked in (asShotMul = 1) and no
       matrix follows (camToWorking = identity), leaving XYZ -> linear sRGB as the whole
       chain. That resolves the as-shot temperature to sRGB's own white point, D65
       (~6500 K), which is the correct reading for a display-referred file. */
    out.cam.valid = true;
    for (int i = 0; i < 3; ++i) {
        out.cam.asShotMul[i] = 1.0f;
        for (int j = 0; j < 3; ++j) {
            out.cam.xyzToCam[i][j]  = kXyzToSrgbIn[i][j];
            /* The synthetic camera's response IS linear sRGB, so its "camera -> working"
               matrix is exactly the sRGB -> working matrix the pixels take below. The
               white-balance solver walks this chain, so it has to match what the pixels
               actually did or the as-shot temperature comes out wrong. */
            out.cam.camToWorking[i][j] = toWork[i][j];
        }
    }
    WhiteBalance::resolveAsShot(out.cam);
    out.rgb.assign(static_cast<size_t>(W) * static_cast<size_t>(H) * 3, 0.0f);

    for (int y = 0; y < H; ++y) {
        const uchar *line = src.constScanLine(y);
        for (int x = 0; x < W; ++x) {
            const size_t o = (static_cast<size_t>(y) * W + x) * 3;
            float v[3];
            for (int c = 0; c < 3; ++c) v[c] = SrgbInvGamma(line[x * 3 + c] / 255.0f);
            if (convert) ColorSpaceMath::apply(toWork, v[0], v[1], v[2]);
            for (int c = 0; c < 3; ++c) out.rgb[o + c] = v[c];
        }
    }
    return true;
}
