#ifndef FUJICOMPRESSED_H
#define FUJICOMPRESSED_H

#include <cstdint>
#include <vector>
#include <QString>

/*
    Fujifilm RAF lossless/lossy compressed CFA decoder.

    A faithful, self-contained port of LibRaw's fuji_compressed.cpp (Alexey Danilchenko's
    decoder) with all decoder state held in a per-call context struct rather than globals, so
    several images may decode concurrently on the cache threads. Validated byte-identical to
    LibRaw on the compressed X-Trans (X-T2) and Bayer (GFX 50S II) sample RAFs.

    The compressed data block begins with a 16-byte big-endian header (signature 0x4953) that
    carries the geometry, bit depth, block layout and lossless flag; decode() parses it, walks
    the per-block bitstreams and writes one uint16 sample per photosite, row-major over the full
    raw_width x raw_height sensor area (the same layout the uncompressed path produces).

    'data' points at the whole RAF mapped in memory; 'dataOffset' is the absolute offset of the
    16-byte header (cfaOff + FujiIFD StripOffset 0xf007). 'xtrans' is the 6x6 colour map
    (0=R,1=G,2=B) for X-Trans bodies; 'bayer' is the 2x2 FC map for Bayer bodies (e.g. GFX).
    On success out is sized rawW*rawH and rawW/rawH are set from the header. Returns false with
    err set on malformed input.
*/
namespace FujiCompressed {

bool isCompressed(const uint8_t *data, int64_t size, int64_t dataOffset);

bool decode(const uint8_t *data, int64_t size, int64_t dataOffset,
            std::vector<uint16_t> &out, int &rawW, int &rawH,
            const uint8_t xtrans[6][6], const int bayer[2][2], QString &err);

}

#endif // FUJICOMPRESSED_H
