#include "Develop/outputtransform.h"
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

} // namespace

bool OutputTransform::ToImage(const WorkingImage &img, QImage &out)
{
    if (!img.isValid()) return false;

    const int W = img.width;
    const int H = img.height;
    const float scale = img.white > 0.0f ? 1.0f / img.white : 1.0f;

    out = QImage(W, H, QImage::Format_RGB888);
    if (out.isNull()) return false;

    for (int y = 0; y < H; ++y) {
        uchar *line = out.scanLine(y);
        for (int x = 0; x < W; ++x) {
            const size_t o = (static_cast<size_t>(y) * W + x) * 3;
            for (int c = 0; c < 3; ++c)
                line[x * 3 + c] =
                    static_cast<uchar>(std::lround(SrgbGamma(img.rgb[o + c] * scale) * 255.0f));
        }
    }
    return true;
}
