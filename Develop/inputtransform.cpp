#include "Develop/inputtransform.h"
#include "Develop/whitebalance.h"
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

} // namespace

bool InputTransform::FromImage(const QImage &in, WorkingImage &out)
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

    /* A synthetic "camera" whose response IS linear sRGB, so the absolute white balance
       works uniformly on non-raw files: nothing was baked in (asShotMul = 1) and no
       matrix follows (camToSrgb = identity), leaving XYZ -> linear sRGB as the whole
       chain. That resolves the as-shot temperature to sRGB's own white point, D65
       (~6500 K), which is the correct reading for a display-referred file. */
    out.cam.valid = true;
    for (int i = 0; i < 3; ++i) {
        out.cam.asShotMul[i] = 1.0f;
        for (int j = 0; j < 3; ++j) {
            out.cam.xyzToCam[i][j]  = kXyzToSrgbIn[i][j];
            out.cam.camToSrgb[i][j] = (i == j) ? 1.0f : 0.0f;
        }
    }
    WhiteBalance::resolveAsShot(out.cam);
    out.rgb.assign(static_cast<size_t>(W) * static_cast<size_t>(H) * 3, 0.0f);

    for (int y = 0; y < H; ++y) {
        const uchar *line = src.constScanLine(y);
        for (int x = 0; x < W; ++x) {
            const size_t o = (static_cast<size_t>(y) * W + x) * 3;
            for (int c = 0; c < 3; ++c)
                out.rgb[o + c] = SrgbInvGamma(line[x * 3 + c] / 255.0f);
        }
    }
    return true;
}
