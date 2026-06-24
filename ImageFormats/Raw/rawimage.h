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
    Map the four 2x2 CFA colour codes (0=R, 1=G, 2=B) at (r0,c0)(r0,c1)(r1,c0)(r1,c1) -- the
    TIFF/EP CFAPattern / RawSensorInfo::cfaPlaneColor convention -- to a CfaPattern. Shared by
    every vendor unpack so the mapping lives in one place. Returns Unknown for anything that
    is not one of the four Bayer phases.
*/
inline CfaPattern cfaPatternFromPlaneColor(const uint8_t p[4]) {
    const int a = p[0], b = p[1], c = p[2], d = p[3];
    if (a==0 && b==1 && c==1 && d==2) return CfaPattern::RGGB;
    if (a==2 && b==1 && c==1 && d==0) return CfaPattern::BGGR;
    if (a==1 && b==0 && c==2 && d==1) return CfaPattern::GRBG;
    if (a==1 && b==2 && c==0 && d==1) return CfaPattern::GBRG;
    return CfaPattern::Unknown;
}

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

    /* When pattern == XTrans, this is the 6x6 colour map (0=R, 1=G, 2=B) at the mosaic origin;
       the demosaicer tiles it across the image. Unused for Bayer patterns. */
    uint8_t xtrans[6][6] = {{0}};

    uint16_t black[4] = {0, 0, 0, 0};       // per-2x2-position black level
    uint16_t white = 65535;                 // saturation (clip) level

    float camMul[4] = {1, 1, 1, 1};         // as-shot white-balance multipliers (R,G,B,G2)
    float xyzToCam[3][3] = {                // CIE XYZ (D65) -> camera-native RGB
        {1, 0, 0}, {0, 1, 0}, {0, 0, 1}     // (DNG ColorMatrix / dcraw-libraw adobe_coeff convention)
    };

    bool isValid() const {
        return width > 0 && height > 0 &&
               cfa.size() == static_cast<size_t>(width) * static_cast<size_t>(height);
    }
};

#endif // RAWIMAGE_H
