#include "ImageFormats/Raw/rawcolor.h"
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

bool RawColor::ToWorking(const RawImage &raw,
                         const std::vector<float> &rgb,
                         WorkingImage &out)
{
    const int W = raw.width;
    const int H = raw.height;
    if (W <= 0 || H <= 0) return false;
    if (rgb.size() != static_cast<size_t>(W) * static_cast<size_t>(H) * 3) return false;

    /*
        Build the camera-RGB -> linear-sRGB matrix and a neutral white balance from the model's
        XYZ->camera matrix, following dcraw's cam_xyz_coeff:
          camRgb  = xyzToCam . rgbToXyz      (RGB -> camera)
          row-normalise camRgb so camRgb.(1,1,1) = (1,1,1); preMul[i] = 1 / rowSum[i]
          rgbCam  = inverse(camRgb)          (camera -> linear sRGB)
        preMul is the white balance that makes a neutral scene map to neutral; it is used when
        the file's as-shot WB (raw.camMul) is not available. With the default identity xyzToCam
        this reduces to an sRGB-assumed pass-through, so unknown cameras still render (approx).
    */
    double camRgb[3][3];
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j) {
            double s = 0.0;
            for (int k = 0; k < 3; ++k)
                s += static_cast<double>(raw.xyzToCam[i][k]) * kRgbToXyz[k][j];
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

    /* White balance: as-shot multipliers when the file provided them (not all equal), else the
       matrix-derived neutral preMul. Normalised so green is unity to keep exposure stable. */
    const bool haveAsShot =
        !(raw.camMul[0] == raw.camMul[1] && raw.camMul[1] == raw.camMul[2]);
    double mul[3];
    if (haveAsShot) {
        mul[0] = raw.camMul[0]; mul[1] = raw.camMul[1]; mul[2] = raw.camMul[2];
    } else {
        mul[0] = preMul[0]; mul[1] = preMul[1]; mul[2] = preMul[2];
    }
    const double g = (mul[1] != 0.0) ? mul[1] : 1.0;
    const float wb[3] = {
        static_cast<float>(mul[0] / g), 1.0f, static_cast<float>(mul[2] / g)
    };

    /* cam -> linear sRGB matrix as float for the hot loop. */
    float m[3][3];
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j) m[i][j] = static_cast<float>(rgbCam[i][j]);

    out.width = W;
    out.height = H;
    out.white = 1.0f;       // rgb was scaled to 0..1 by Demosaic using raw.white
    out.sceneReferred = true;   // sensor data -> OutputTransform applies the baseline tone curve
    out.rgb.assign(static_cast<size_t>(W) * static_cast<size_t>(H) * 3, 0.0f);

    for (int y = 0; y < H; ++y) {
        for (int x = 0; x < W; ++x) {
            const size_t o = (static_cast<size_t>(y) * W + x) * 3;
            /* White balance in camera space. */
            const float cam[3] = {
                rgb[o + 0] * wb[0],
                rgb[o + 1] * wb[1],
                rgb[o + 2] * wb[2]
            };
            /* Camera -> linear sRGB. Clamp negatives (out-of-gamut) but keep highlight
               headroom (values may exceed 1) so OutputTransform's baseline tone curve can roll
               highlights off instead of hard-clipping them here. */
            for (int c = 0; c < 3; ++c) {
                const float v = m[c][0] * cam[0] + m[c][1] * cam[1] + m[c][2] * cam[2];
                out.rgb[o + c] = v < 0.0f ? 0.0f : v;
            }
        }
    }
    return true;
}
