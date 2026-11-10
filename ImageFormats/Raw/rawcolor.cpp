#include "ImageFormats/Raw/rawcolor.h"
#include "Develop/whitebalance.h"
#include "Develop/colorspace.h"
#include <opencv2/core/utility.hpp>     // parallel_for_
#include <algorithm>
#include <cmath>

namespace {

/* Linear sRGB (D65 primaries) -> CIE XYZ. dcraw/libraw "xyz_rgb". */
const double kRgbToXyz[3][3] = {
    {0.412453, 0.357580, 0.180423},
    {0.212671, 0.715160, 0.072169},
    {0.019334, 0.119193, 0.950227}
};

/* 3x3 inverse; returns false if (near-)singular. */
bool Invert3x3(const double a[3][3], double inv[3][3])
{
    const double det =
        a[0][0] * (a[1][1] * a[2][2] - a[1][2] * a[2][1]) -
        a[0][1] * (a[1][0] * a[2][2] - a[1][2] * a[2][0]) +
        a[0][2] * (a[1][0] * a[2][1] - a[1][1] * a[2][0]);
    if (std::fabs(det) < 1e-12) return false;
    const double idet = 1.0 / det;
    inv[0][0] = (a[1][1] * a[2][2] - a[1][2] * a[2][1]) * idet;
    inv[0][1] = (a[0][2] * a[2][1] - a[0][1] * a[2][2]) * idet;
    inv[0][2] = (a[0][1] * a[1][2] - a[0][2] * a[1][1]) * idet;
    inv[1][0] = (a[1][2] * a[2][0] - a[1][0] * a[2][2]) * idet;
    inv[1][1] = (a[0][0] * a[2][2] - a[0][2] * a[2][0]) * idet;
    inv[1][2] = (a[0][2] * a[1][0] - a[0][0] * a[1][2]) * idet;
    inv[2][0] = (a[1][0] * a[2][1] - a[1][1] * a[2][0]) * idet;
    inv[2][1] = (a[0][1] * a[2][0] - a[0][0] * a[2][1]) * idet;
    inv[2][2] = (a[0][0] * a[1][1] - a[0][1] * a[1][0]) * idet;
    return true;
}

} // namespace

bool RawColor::ToCameraNative(const RawImage &raw,
                              const std::vector<float> &rgb,
                              WorkingImage &out)
{
    std::vector<float> copy = rgb;
    return ToCameraNative(raw, std::move(copy), out);
}

bool RawColor::ToCameraNative(const RawImage &raw,
                              std::vector<float> &&rgb,
                              WorkingImage &out)
{
    const int W = raw.width;
    const int H = raw.height;
    if (W <= 0 || H <= 0) return false;
    if (rgb.size() != static_cast<size_t>(W) * static_cast<size_t>(H) * 3) return false;

    /* Colour characterisation -- the matrix, the as-shot white balance and the Kelvin
       it resolves to. Shared with the Apple Core Image engine, which has no RawImage
       to hand; see RawColor::Characterise. */
    Characterise(raw.xyzToCam, raw.camMul, out.cam, raw.asShotStated);

    out.width = W;
    out.height = H;
    out.white = 1.0f;       // rgb was scaled to 0..1 by Demosaic using raw.white
    out.sceneReferred = true;   // sensor data -> output stage applies a view transform
    out.space = ColorSpaceMath::ColorSpace::CameraNative;

    /*
        THE PIXELS PASS THROUGH UNCHANGED.

        The white balance and the colour matrix used to be applied right here. They are
        now Develop's stage 0 (Develop::ToWorkingSpace, normally folded into
        PointCoeffs::preMat), for one reason: this buffer is what WorkingImageCache
        stores, so anything baked in here can only be changed by DECODING THE RAW AGAIN.
        With the conversion downstream of the cache, changing a camera profile or the
        white balance is a re-render off pixels we already have.

        Nothing is clamped either. The old code clamped negatives to zero, which threw
        away every colour outside the working gamut before the user had touched the
        image -- irrecoverably, since a later stage cannot invent data that was zeroed.
        Out-of-gamut values now survive to the output stage, which is the only place that
        knows what gamut it is rendering into.
    */
    out.rgb = std::move(rgb);
    return true;
}

void RawColor::Characterise(const float xyzToCam[3][3], const float camMul[4],
                            CameraColor &out, bool asShotStated)
{
    /*
        Build the camera-RGB -> linear-sRGB matrix and a neutral white balance from the model's
        XYZ->camera matrix, following dcraw's cam_xyz_coeff:
          camRgb  = xyzToCam . rgbToXyz      (RGB -> camera)
          row-normalise camRgb so camRgb.(1,1,1) = (1,1,1); preMul[i] = 1 / rowSum[i]
          rgbCam  = inverse(camRgb)          (camera -> linear sRGB)
        preMul is the white balance that makes a neutral scene map to neutral; it is used when
        the file's as-shot WB (camMul) is not available. With a default identity xyzToCam
        this reduces to an sRGB-assumed pass-through, so unknown cameras still render (approx).
    */
    double camRgb[3][3];
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j) {
            double s = 0.0;
            for (int k = 0; k < 3; ++k)
                s += static_cast<double>(xyzToCam[i][k]) * kRgbToXyz[k][j];
            camRgb[i][j] = s;
        }

    double preMul[3];
    for (int i = 0; i < 3; ++i) {
        double num = camRgb[i][0] + camRgb[i][1] + camRgb[i][2];
        if (std::fabs(num) < 1e-12) num = 1.0;
        for (int j = 0; j < 3; ++j) camRgb[i][j] /= num;
        preMul[i] = 1.0 / num;
    }

    double rgbCam[3][3];
    if (!Invert3x3(camRgb, rgbCam)) {
        /* Singular matrix (bad data): fall back to identity so the image still renders. */
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 3; ++j) rgbCam[i][j] = (i == j) ? 1.0 : 0.0;
        preMul[0] = preMul[1] = preMul[2] = 1.0;
    }

    /* White balance: as-shot multipliers when the file provided them (stated, or not all
       equal), else the matrix-derived neutral preMul. Normalised so green is unity to
       keep exposure stable. */
    const bool haveAsShot = asShotStated ||
        !(camMul[0] == camMul[1] && camMul[1] == camMul[2]);
    double mul[3];
    if (haveAsShot) {
        mul[0] = camMul[0]; mul[1] = camMul[1]; mul[2] = camMul[2];
    } else {
        mul[0] = preMul[0]; mul[1] = preMul[1]; mul[2] = preMul[2];
    }
    const double g = (mul[1] != 0.0) ? mul[1] : 1.0;
    const float wb[3] = {
        static_cast<float>(mul[0] / g), 1.0f, static_cast<float>(mul[2] / g)
    };

    /* camera -> the WORKING space: rgbCam lands in linear sRGB, so compose it with
       sRGB -> kWorking. That second matrix is the identity while the working space is
       sRGB, and becomes a real matrix the day it widens -- composing here means the
       widening is a one-line change in colorspace.h and nothing else moves. */
    const ColorSpaceMath::Matrix3 toWork =
        ColorSpaceMath::matrix(ColorSpaceMath::ColorSpace::LinearSRGB,
                               ColorSpaceMath::kWorking);
    float m[3][3];
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j) {
            double s = 0.0;
            for (int k = 0; k < 3; ++k) s += toWork.m[i][k] * rgbCam[k][j];
            m[i][j] = static_cast<float>(s);
        }

    /* Hand the colour characterisation to the Develop stage. It needs it for two jobs
       now: the ABSOLUTE white balance (Kelvin + tint solved against the model matrix),
       and the INPUT PROFILE ITSELF -- because this function no longer applies the
       matrix. resolveAsShot back-solves the temperature the shot was balanced for,
       which is what the Temp slider reads before the user touches it. See
       Develop/whitebalance.h. */
    out.valid = true;
    for (int i = 0; i < 3; ++i) {
        out.asShotMul[i] = wb[i];
        for (int j = 0; j < 3; ++j) {
            out.xyzToCam[i][j] = xyzToCam[i][j];
            out.camToWorking[i][j] = m[i][j];
        }
    }
    WhiteBalance::resolveAsShot(out);
}

void RawColor::NeutraliseClipped(WorkingImage &img)
{
    const size_t n = static_cast<size_t>(img.width) * static_cast<size_t>(img.height);
    if (n == 0 || img.rgb.size() != n * 3) return;
    float *px = img.rgb.data();

    constexpr int kRows = 64;
    const int W = img.width;
    const int strips = (img.height + kRows - 1) / kRows;

    /* Per-strip channel maxima, then the count sitting within kPile of each maximum.
       A clipped plateau piles thousands of pixels at one value; an unclipped highlight
       is a lone peak. */
    std::vector<float> stripMax(static_cast<size_t>(strips) * 3, 0.0f);
    cv::parallel_for_(cv::Range(0, strips), [&](const cv::Range &r) {
        for (int i = r.start; i < r.end; ++i) {
            const size_t a = static_cast<size_t>(i) * kRows * W;
            const size_t b = std::min(n, a + static_cast<size_t>(kRows) * W);
            float m[3] = {0.0f, 0.0f, 0.0f};
            for (size_t p = a; p < b; ++p)
                for (int c = 0; c < 3; ++c) m[c] = std::max(m[c], px[p * 3 + c]);
            for (int c = 0; c < 3; ++c) stripMax[static_cast<size_t>(i) * 3 + c] = m[c];
        }
    });
    float chMax[3] = {0.0f, 0.0f, 0.0f};
    for (int i = 0; i < strips; ++i)
        for (int c = 0; c < 3; ++c)
            chMax[c] = std::max(chMax[c], stripMax[static_cast<size_t>(i) * 3 + c]);

    constexpr float kPile = 0.995f;
    std::vector<size_t> stripCount(static_cast<size_t>(strips) * 3, 0);
    cv::parallel_for_(cv::Range(0, strips), [&](const cv::Range &r) {
        for (int i = r.start; i < r.end; ++i) {
            const size_t a = static_cast<size_t>(i) * kRows * W;
            const size_t b = std::min(n, a + static_cast<size_t>(kRows) * W);
            size_t k[3] = {0, 0, 0};
            for (size_t p = a; p < b; ++p)
                for (int c = 0; c < 3; ++c)
                    if (px[p * 3 + c] >= chMax[c] * kPile) ++k[c];
            for (int c = 0; c < 3; ++c) stripCount[static_cast<size_t>(i) * 3 + c] = k[c];
        }
    });
    const size_t minPile = std::max<size_t>(64, n / 100000);
    float clip[3];
    bool any = false;
    for (int c = 0; c < 3; ++c) {
        size_t k = 0;
        for (int i = 0; i < strips; ++i) k += stripCount[static_cast<size_t>(i) * 3 + c];
        /* A pile-up well below the declared white is a dark frame's own maximum, not a
           sensor clip. */
        const bool clipped = k >= minPile && chMax[c] >= 0.5f;
        clip[c] = clipped ? chMax[c] * kPile : 0.0f;
        any = any || clipped;
    }
    if (!any) return;

    const float *mul = img.cam.asShotMul;
    float ceil[3], kneeLo[3], invKnee[3];
    constexpr float kKnee = 0.10f;      // the top 10% of the clip level eases in
    for (int c = 0; c < 3; ++c) {
        ceil[c] = clip[c] * mul[c];
        kneeLo[c] = clip[c] * (1.0f - kKnee);
        invKnee[c] = clip[c] > 0.0f ? 1.0f / (clip[c] * kKnee) : 0.0f;
    }
    cv::parallel_for_(cv::Range(0, strips), [&](const cv::Range &r) {
        for (int i = r.start; i < r.end; ++i) {
            const size_t a = static_cast<size_t>(i) * kRows * W;
            const size_t b = std::min(n, a + static_cast<size_t>(kRows) * W);
            for (size_t p = a; p < b; ++p) {
                float *v = px + p * 3;
                /* The lowest white-balanced ceiling among the (softly) clipped channels:
                   ceil / smoothstep, so an unclipped channel contributes infinity. */
                float cap = HUGE_VALF;
                for (int c = 0; c < 3; ++c) {
                    if (clip[c] <= 0.0f || v[c] <= kneeLo[c]) continue;
                    float t = (v[c] - kneeLo[c]) * invKnee[c];
                    if (t > 1.0f) t = 1.0f;
                    const float s = t * t * (3.0f - 2.0f * t);
                    if (s > 0.0f) cap = std::min(cap, ceil[c] / s);
                }
                if (cap == HUGE_VALF) continue;
                for (int c = 0; c < 3; ++c) {
                    const float lim = cap / mul[c];
                    if (v[c] > lim) v[c] = lim;
                }
            }
        }
    });
}
