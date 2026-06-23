#include "ImageFormats/Raw/rawcolor.h"

bool RawColor::ToWorking(const RawImage &raw,
                         const std::vector<float> &rgb,
                         WorkingImage &out)
{
    const int W = raw.width;
    const int H = raw.height;
    if (W <= 0 || H <= 0) return false;
    if (rgb.size() != static_cast<size_t>(W) * static_cast<size_t>(H) * 3) return false;

    /* White-balance multipliers, normalised so green is unity. */
    const float gMul = raw.camMul[1] != 0.0f ? raw.camMul[1] : 1.0f;
    const float wb[3] = {
        raw.camMul[0] / gMul,
        1.0f,
        raw.camMul[2] / gMul
    };

    /* Camera RGB -> XYZ (from metadata) then XYZ -> linear sRGB (D65). The default
       identity camToXyz gives an approximate result; a real per-camera matrix lands in a
       later phase. No gamma is applied here -- output stays linear for Develop. */
    static const float xyzToSrgb[3][3] = {
        { 3.2404542f, -1.5371385f, -0.4985314f},
        {-0.9692660f,  1.8760108f,  0.0415560f},
        { 0.0556434f, -0.2040259f,  1.0572252f}
    };
    float camToSrgb[3][3];
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j) {
            float s = 0.0f;
            for (int k = 0; k < 3; ++k)
                s += xyzToSrgb[i][k] * raw.camToXyz[k][j];
            camToSrgb[i][j] = s;
        }

    out.width = W;
    out.height = H;
    out.white = 1.0f;       // rgb was scaled to 0..1 by Demosaic using raw.white
    out.rgb.assign(static_cast<size_t>(W) * static_cast<size_t>(H) * 3, 0.0f);

    for (int y = 0; y < H; ++y) {
        for (int x = 0; x < W; ++x) {
            const size_t o = (static_cast<size_t>(y) * W + x) * 3;
            const float cam[3] = {
                rgb[o + 0] * wb[0],
                rgb[o + 1] * wb[1],
                rgb[o + 2] * wb[2]
            };
            for (int c = 0; c < 3; ++c)
                out.rgb[o + c] = camToSrgb[c][0] * cam[0] +
                                 camToSrgb[c][1] * cam[1] +
                                 camToSrgb[c][2] * cam[2];
        }
    }
    return true;
}
