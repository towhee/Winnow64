#ifndef RAWIMAGE_H
#define RAWIMAGE_H

#include <cstdint>
#include <vector>

/*
    CFA (colour filter array) mosaic pattern of the sensor. The 2x2 codes name the
    colour at (row0,col0),(row0,col1),(row1,col0),(row1,col1). XTrans is Fuji's 6x6
    pattern, handled separately by the demosaicer (not supported in the scaffold).
*/
enum class CfaPattern {
    Unknown,
    RGGB,
    BGGR,
    GRBG,
    GBRG,
    XTrans
};

/*
    Normalised sensor data produced by RawFormat::UnpackCfa(). One uint16 sample per
    photosite (the mosaic), row-major over the active area, together with the metadata
    the shared pipeline needs to turn the mosaic into a colour image.

    This is the single hand-off point between the per-format unpack step (which knows
    the vendor bitstream) and the shared demosaic/colour stages (which do not care
    which camera produced the data).

    The four-element black[] / camMul[] arrays are indexed by 2x2 photosite position,
    i.e. ((row & 1) << 1) | (col & 1), so a single convention covers both green sites.
*/
struct RawImage {
    std::vector<uint16_t> cfa;              // one sample per photosite, row-major
    int width = 0;                          // active-area width  (photosites)
    int height = 0;                         // active-area height (photosites)

    CfaPattern pattern = CfaPattern::Unknown;

    uint16_t black[4] = {0, 0, 0, 0};       // per-2x2-position black level
    uint16_t white = 65535;                 // saturation (clip) level

    float camMul[4] = {1, 1, 1, 1};         // as-shot white-balance multipliers (R,G,B,G2)
    float camToXyz[3][3] = {                // camera-native RGB -> CIE XYZ (D65)
        {1, 0, 0}, {0, 1, 0}, {0, 0, 1}
    };

    bool isValid() const {
        return width > 0 && height > 0 &&
               cfa.size() == static_cast<size_t>(width) * static_cast<size_t>(height);
    }
};

#endif // RAWIMAGE_H
