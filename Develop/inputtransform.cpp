#include "Develop/inputtransform.h"
#include <cmath>

namespace {

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
