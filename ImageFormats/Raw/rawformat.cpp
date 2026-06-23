#include "ImageFormats/Raw/rawformat.h"
#include "ImageFormats/Raw/demosaic.h"
#include "ImageFormats/Raw/rawcolor.h"
#include "Develop/workingimage.h"
#include "Develop/develop.h"
#include "Develop/outputtransform.h"
#include <algorithm>

/* Per-format sensor decoders register here as each UnpackCfa() lands (phase 2+). */
// #include "ImageFormats/Nikon/nikon.h"   // class NikonRaw
// #include "ImageFormats/Canon/canon.h"   // class CanonRaw

std::unique_ptr<RawFormat> RawFormat::Create(const QString &ext)
{
/*
    Until a per-format UnpackCfa() exists no extension has a decoder, so this returns
    nullptr and ImageDecoder keeps using the embedded-JPG path for RAW files. Add a
    case per format as it is implemented, e.g.:
        if (ext == "nef") return std::make_unique<NikonRaw>();
*/
    Q_UNUSED(ext)
    return nullptr;
}

bool RawFormat::Decode(QFile &file, const ImageMetadata &m, QImage &out,
                       const EditParams *edit)
{
/*
    Shared, camera-agnostic pipeline:
        UnpackCfa()         per-format bitstream -> normalised CFA mosaic   (virtual)
        SubtractBlack()                                                      (shared)
        Demosaic::Run()     mosaic -> linear camera RGB                      (shared)
        RawColor::ToWorking() white balance + matrix -> LINEAR WorkingImage  (shared)
        Develop::Apply()    parametric adjustments in linear space           (shared, opt)
        OutputTransform::ToImage() gamma/ICC -> display QImage               (shared)
*/
    RawImage raw;
    if (!UnpackCfa(file, m, raw)) {
        if (errMsg.isEmpty()) errMsg = "UnpackCfa failed.";
        return false;
    }
    if (!raw.isValid()) {
        errMsg = "UnpackCfa produced an invalid RawImage.";
        return false;
    }

    SubtractBlack(raw);

    Demosaic demosaic;
    std::vector<float> rgb;
    if (!demosaic.Run(raw, rgb)) {
        errMsg = "Demosaic failed (unsupported CFA pattern?).";
        return false;
    }

    RawColor color;
    WorkingImage work;
    if (!color.ToWorking(raw, rgb, work)) {
        errMsg = "Colour conversion failed.";
        return false;
    }

    /* RAW develops in its native linear float (better than an 8-bit round trip), so the
       develop stage runs here rather than in ImageDecoder for raw files. */
    if (edit && !edit->isIdentity()) {
        Develop develop;
        develop.Apply(work, *edit);
    }

    OutputTransform output;
    if (!output.ToImage(work, out)) {
        errMsg = "Output transform failed.";
        return false;
    }
    return true;
}

void RawFormat::SubtractBlack(RawImage &raw)
{
    uint16_t maxBlack = 0;
    for (int i = 0; i < 4; ++i) maxBlack = std::max(maxBlack, raw.black[i]);
    if (maxBlack == 0) return;

    const int W = raw.width;
    const int H = raw.height;
    for (int y = 0; y < H; ++y) {
        const int rowBit = (y & 1) << 1;
        for (int x = 0; x < W; ++x) {
            const uint16_t b = raw.black[rowBit | (x & 1)];
            uint16_t &v = raw.cfa[static_cast<size_t>(y) * W + x];
            v = (v > b) ? static_cast<uint16_t>(v - b) : uint16_t(0);
        }
    }
    raw.white = (raw.white > maxBlack)
                    ? static_cast<uint16_t>(raw.white - maxBlack)
                    : raw.white;
    for (int i = 0; i < 4; ++i) raw.black[i] = 0;
}
