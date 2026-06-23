#ifndef RAWSENSORINFO_H
#define RAWSENSORINFO_H

#include <cstdint>

/*
    RawSensorInfo carries the per-file facts a RAW decoder needs to locate and unpack the
    sensor mosaic: where the CFA data lives in the file, its geometry and bit depth, the
    compression scheme, and the sensor levels / CFA layout. It is populated once, during the
    normal metadata read (vendor parse()), and then travels on ImageMetadata so the decode
    path can rely on it instead of re-walking the file.

    Deliberately dependency-free (plain ints, no Qt) so it can sit on ImageMetadata, be stored
    in the DataModel, and be consumed by the ImageFormats/Raw pipeline without coupling those
    layers. White-balance multipliers and the colour matrix are intentionally NOT here yet --
    they come from the makernote and belong with the colour stage (see task: real RawColor).

    cfaPlaneColor holds the 2x2 mosaic colour codes (0=R, 1=G, 2=B) at (r0,c0)(r0,c1)(r1,c0)
    (r1,c1); default is RGGB. black[] is indexed the same 2x2 way, ((row&1)<<1)|(col&1).
*/
struct RawSensorInfo {
    bool isRaw = false;                 // true once a vendor parser has filled this in
    uint32_t stripOffset = 0;           // file offset of the CFA strip/tile data
    uint32_t stripLength = 0;           // byte length of that data
    int width = 0;                      // CFA active-area width  (photosites)
    int height = 0;                     // CFA active-area height (photosites)
    int bitsPerSample = 0;              // e.g. 12 / 14 / 16
    int compression = 0;                // TIFF Compression tag (1 = uncompressed)
    int samplesPerPixel = 1;            // 1 for a CFA mosaic
    bool littleEndianSamples = true;    // byte order of the packed 16-bit CFA samples
    uint8_t cfaPlaneColor[4] = {0, 1, 1, 2};   // 2x2 colour codes, default RGGB
    uint32_t white = 0;                 // saturation level (0 = unknown -> derive from bps)
    uint32_t black[4] = {0, 0, 0, 0};   // per-2x2-position black level

    /* Enough to unpack: known geometry and a real strip. Compression handling is the
       decoder's call (e.g. SonyRaw rejects what it cannot unpack yet). */
    bool isValid() const {
        return isRaw && width > 0 && height > 0 &&
               stripOffset > 0 && stripLength > 0 && bitsPerSample > 0;
    }
};

#endif // RAWSENSORINFO_H
