#ifndef DNGJXL_H
#define DNGJXL_H

#include <QString>
#include <cstddef>
#include <cstdint>
#include <vector>

/*
    JPEG XL codestream decode for DNG 1.7 (Compression 52546). Lightroom writes it for
    pano/HDR merges and when "JPEG XL" DNG compression is on: every tile or strip of every
    image in the file -- the Linear Raw main image, its reduced-resolution copies and the
    RGB previews -- is a separate, complete JPEG XL codestream.

    Thin wrapper over libjxl's decoder, compiled only with WINNOW_HAVE_JXL (CMake finds
    libjxl on each platform; see notes/Documentation.md "Adobe DNG"). Without it
    Available() is false and Decode() always fails, so callers fall back to the embedded
    JPEG.

    Single-threaded per call by design: DNG tiles are small (a few hundred pixels square)
    and the caller decodes many of them in parallel, which scales far better than
    libjxl's own runner on one tile at a time.
*/
namespace DngJxl {

bool Available();

/*
    Decode one codestream to interleaved float samples in [0,1] of the stored range, i.e.
    stored value = sample * (2^bitsPerSample - 1). Requests channels samples per pixel
    (1 or 3); a codestream with a different colour channel count fails. libjxl returns the
    stored samples, not a colour-converted rendering: with no preferred output profile it
    decodes to the encoding the file signals.
*/
bool Decode(const uint8_t *data, size_t size, int channels,
            std::vector<float> &out, int &width, int &height, QString *err = nullptr);

} // namespace DngJxl

#endif // DNGJXL_H
