#ifndef LOSSLESSJPEG_H
#define LOSSLESSJPEG_H

#include <cstdint>
#include <vector>
#include <QString>

/*
    Shared lossless-JPEG decoder (ITU-T T.81 process 14, SOF3) for RAW formats.

    Raw files do NOT use the familiar DCT JPEG: they use the lossless mode, where each sample
    is Huffman-coded as the difference from a spatial predictor of its neighbours. This is the
    compression behind Canon CR2, most compressed Adobe DNG, and lossless NEF/ARW variants.

    The decoder is camera-agnostic: it takes a complete SOI..EOI bytestream and returns the
    decoded integer samples, organised by component. Each vendor decoder (DngRaw, CanonRaw, ...)
    extracts the bytestream from its container and then reassembles the components/tiles/slices
    into the CFA mosaic -- that mapping is format-specific and lives in the caller, not here.

    Samples are interleaved by component: samples[(y * width + x) * components + c]. For a Bayer
    sensor encoded as 2 components, width is half the sensor width and the caller interleaves the
    two components back to full width.

    SCOPE: single scan, Huffman (not arithmetic), no restart intervals (DRI/RSTn) yet -- those
    return false so the caller can fall back. Covers CR2 and uncompressed/lossless-JPEG DNG.
*/
namespace LosslessJpeg {

struct Image {
    int width = 0;          // samples per line  (X in SOF3)
    int height = 0;         // number of lines    (Y)
    int components = 1;     // Nf
    int precision = 0;      // bits per sample
    std::vector<uint16_t> samples;  // interleaved: samples[(y*width + x)*components + c]

    bool isValid() const {
        return width > 0 && height > 0 && components > 0 &&
               samples.size() == static_cast<size_t>(width) *
                                 static_cast<size_t>(height) *
                                 static_cast<size_t>(components);
    }
};

/*
    Decode a complete lossless-JPEG bytestream. Returns false (and sets *err when non-null) on
    malformed input or an unsupported feature (arithmetic coding, restart intervals, a non-SOF3
    frame). On success, out holds the decoded samples.
*/
bool Decode(const uint8_t *data, size_t size, Image &out, QString *err = nullptr);

} // namespace LosslessJpeg

#endif // LOSSLESSJPEG_H
