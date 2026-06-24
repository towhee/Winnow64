#include "ImageFormats/Raw/rawformat.h"
#include "ImageFormats/Raw/demosaic.h"
#include "ImageFormats/Raw/rawcolor.h"
#include "Develop/workingimage.h"
#include "Develop/develop.h"
#include "Develop/outputtransform.h"
#include <algorithm>

/* Per-format sensor decoders register here as each UnpackCfa() lands (phase 2+). */
#include "ImageFormats/Sony/sony.h"     // class SonyRaw
#include "ImageFormats/Dng/dng.h"       // class DngRaw
#include "ImageFormats/Canon/canon.h"   // class CanonRaw
#include "ImageFormats/Nikon/nikon.h"   // class NikonRaw
#include "ImageFormats/Olympus/olympus.h" // class OlympusRaw
#include "ImageFormats/Panasonic/panasonic.h" // class PanasonicRaw
#include "ImageFormats/Fuji/fuji.h"     // class FujiRaw

std::unique_ptr<RawFormat> RawFormat::Create(const QString &ext)
{
/*
    Map a (lower-case, dot-less) extension to its sensor decoder, or nullptr if none exists
    yet (ImageDecoder then keeps using the embedded-JPG path). A decoder that cannot handle a
    given file -- e.g. SonyRaw on a compressed ARW -- returns false from Decode(), and
    ImageDecoder falls back to the embedded JPG, so registering here is safe.
*/
    if (ext == "arw") return std::make_unique<SonyRaw>();
    if (ext == "dng") return std::make_unique<DngRaw>();
    if (ext == "cr2") return std::make_unique<CanonRaw>();
    if (ext == "nef") return std::make_unique<NikonRaw>();
    if (ext == "orf") return std::make_unique<OlympusRaw>();
    if (ext == "rw2") return std::make_unique<PanasonicRaw>();
    if (ext == "raf") return std::make_unique<FujiRaw>();
    return nullptr;
}

bool RawFormat::Decode(QFile &file, const ImageMetadata &m, QImage &out,
                       const EditParams *edit, const QAtomicInt *abort,
                       std::shared_ptr<const WorkingImage> *outWork)
{
/*
    Shared, camera-agnostic pipeline:
        UnpackCfa()         per-format bitstream -> normalised CFA mosaic   (virtual)
        SubtractBlack()                                                      (shared)
        Demosaic::Run()     mosaic -> linear camera RGB                      (shared)
        RawColor::ToWorking() white balance + matrix -> LINEAR WorkingImage  (shared)
        Develop::Apply()    parametric adjustments in linear space           (shared, opt)
        OutputTransform::ToImage() gamma/ICC -> display QImage               (shared)

    abort is polled between stages (and inside the demosaic loop) so a long decode bails
    promptly when the decoder thread is asked to stop -- otherwise ImageCache::stop()'s
    BlockingQueuedConnection waits for the whole demosaic and the UI beachballs.
*/
    const auto aborted = [abort]{ return abort && abort->loadAcquire(); };

    RawImage raw;
    if (!UnpackCfa(file, m, raw)) {
        if (errMsg.isEmpty()) errMsg = "UnpackCfa failed.";
        return false;
    }
    if (!raw.isValid()) {
        errMsg = "UnpackCfa produced an invalid RawImage.";
        return false;
    }
    if (aborted()) { errMsg = "Aborted"; return false; }

    SubtractBlack(raw);

    Demosaic demosaic;
    std::vector<float> rgb;
    if (!demosaic.Run(raw, rgb, Demosaic::Bilinear, abort)) {
        errMsg = aborted() ? "Aborted" : "Demosaic failed (unsupported CFA pattern?).";
        return false;
    }

    RawColor color;
    auto work = std::make_shared<WorkingImage>();
    if (!color.ToWorking(raw, rgb, *work)) {
        errMsg = "Colour conversion failed.";
        return false;
    }
    if (aborted()) { errMsg = "Aborted"; return false; }

    /* Hand the caller the pre-develop WorkingImage for the WorkingImageCache (shared, no
       copy). It must stay pristine, so a non-identity develop below runs on a private copy. */
    if (outWork) *outWork = work;

    /* RAW develops in its native linear float (better than an 8-bit round trip), so the
       develop stage runs here rather than in ImageDecoder for raw files. */
    OutputTransform output;
    if (edit && !edit->isIdentity()) {
        WorkingImage developed = *work;
        Develop develop;
        develop.Apply(developed, *edit);
        if (aborted()) { errMsg = "Aborted"; return false; }
        if (!output.ToImage(developed, out)) {
            errMsg = "Output transform failed.";
            return false;
        }
        return true;
    }
    if (aborted()) { errMsg = "Aborted"; return false; }

    if (!output.ToImage(*work, out)) {
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
