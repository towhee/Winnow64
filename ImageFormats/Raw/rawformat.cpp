#include "ImageFormats/Raw/rawformat.h"
#include "ImageFormats/Raw/demosaic.h"
#include "ImageFormats/Raw/rawcolor.h"
#include "ImageFormats/Raw/pmrid.h"
#include "ImageFormats/Raw/applerawdecode.h"
#include "Develop/workingimage.h"
#include "Develop/develop.h"
#include "Develop/outputtransform.h"
#include "Main/global.h"
#include <algorithm>
#include <QHash>

/* Per-format sensor decoders register here as each UnpackCfa() lands (phase 2+). */
#include "ImageFormats/Sony/sony.h"     // class SonyRaw
#include "ImageFormats/Dng/dng.h"       // class DngRaw
#include "ImageFormats/Canon/canon.h"   // class CanonRaw
#include "ImageFormats/Nikon/nikon.h"   // class NikonRaw
#include "ImageFormats/Olympus/olympus.h" // class OlympusRaw
#include "ImageFormats/Panasonic/panasonic.h" // class PanasonicRaw
#include "ImageFormats/Fuji/fuji.h"     // class FujiRaw

namespace {
/*
    Single source of truth mapping a (lower-case, dot-less) file extension to its sensor
    decoder factory. Create() and HasSensorDecoder() both derive from this table, so adding
    a new format's sensor decoder is a one-line change here with no second list to keep in
    sync. The factory returns unique_ptr<RawFormat> (each concrete decoder up-casts). The
    table is a function-local static: initialised once, thread-safe (magic statics), and
    const reads are safe from the concurrent decoder threads that call Create().
*/
using RawFactory = std::unique_ptr<RawFormat> (*)();

const QHash<QString, RawFactory> &rawFactories()
{
    static const QHash<QString, RawFactory> table = {
        {"arw", []() -> std::unique_ptr<RawFormat> { return std::make_unique<SonyRaw>();      }},
        {"dng", []() -> std::unique_ptr<RawFormat> { return std::make_unique<DngRaw>();       }},
        {"cr2", []() -> std::unique_ptr<RawFormat> { return std::make_unique<CanonRaw>();     }},
        {"cr3", []() -> std::unique_ptr<RawFormat> { return std::make_unique<CanonCR3Raw>();  }},
        {"nef", []() -> std::unique_ptr<RawFormat> { return std::make_unique<NikonRaw>();     }},
        {"orf", []() -> std::unique_ptr<RawFormat> { return std::make_unique<OlympusRaw>();   }},
        {"rw2", []() -> std::unique_ptr<RawFormat> { return std::make_unique<PanasonicRaw>(); }},
        {"raf", []() -> std::unique_ptr<RawFormat> { return std::make_unique<FujiRaw>();      }},
    };
    return table;
}
} // namespace

std::unique_ptr<RawFormat> RawFormat::Create(const QString &ext)
{
/*
    Build the sensor decoder for a (lower-case, dot-less) file extension, or nullptr if none
    exists yet (ImageDecoder then keeps using the embedded-JPG path). A decoder that cannot
    handle a given file -- e.g. SonyRaw on a compressed ARW -- returns false from Decode(),
    and ImageDecoder falls back to the embedded JPG, so registering here is safe.
*/
    const auto it = rawFactories().constFind(ext);
    return it != rawFactories().cend() ? (*it)() : nullptr;
}

bool RawFormat::HasSensorDecoder(const QString &ext)
{
    /* Cheap membership test, no allocation. Shares rawFactories() with Create(). */
    return rawFactories().contains(ext);
}

bool RawFormat::Decode(QFile &file, const ImageMetadata &m, QImage &out,
                       const EditParams *edit, const QAtomicInt *abort,
                       std::shared_ptr<const WorkingImage> *outWork,
                       bool denoiseRaw,
                       const std::function<void(int, int)> &denoiseProgress,
                       std::shared_ptr<const WorkingImage> *outClean)
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

    auto work = std::make_shared<WorkingImage>();
    bool decoded = false;
    /* Separate PMRID-denoised base for the DEVELOPED display/export image, built only when a saved
       "Denoise raw" edit is present (below). *work (the cached base) always stays CLEAN; the
       develop pass renders from this when set. Null on the Apple engine (no CFA) and with no
       denoise edit. */
    std::shared_ptr<WorkingImage> denoisedBase;

#ifdef Q_OS_MAC
    /* Engine A (macOS only): Core Image decodes file -> WorkingImage directly, bypassing the
       in-house UnpackCfa/Demosaic/RawColor stages. A soft failure falls through to engine B
       below, so a format Core Image can't read still decodes. Off-mac this branch is compiled
       out and the in-house engine is always used. */
    if (G::decodeRawEngine == G::DecodeRawEngine::appleDecodeRawEngine) {
        QString aerr;
        if (AppleRawDecode::Decode(file.fileName(), *work, aerr) && work->isValid()) {
            decoded = true;
        }
        /* else: aerr is a soft failure -- fall through to the in-house engine. */
    }
#endif

    if (!decoded) {
        /* Engine B (portable): in-house sensor decode. */
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

        /* Combined status-bar progress for a "Denoise raw" decode: the clean pre-PMRID
           demosaic, the PMRID model, and the denoised demosaic each drive a slice of one
           0..1000 bar so the user sees continuous movement, not an empty bar through the
           demosaics. Inert when the caller passed no progress sink (a normal decode). */
        auto stage = [&denoiseProgress](int base, int span, int done, int total) {
            if (denoiseProgress && total > 0)
                denoiseProgress(base + span * done / total, 1000);
        };

        /* PMRID pre-demosaic raw denoise (in-house/Winnow engine only). Runs at full
           strength on the mosaic before demosaic; the caller (MW::ensureRawDenoise)
           requests it to build the denoised base and blends the user's amount. No-op for
           non-Bayer patterns or if the model is absent. */
        if (denoiseRaw && G::decodeRawEngine == G::DecodeRawEngine::winnowDecodeRawEngine) {
            /* Clean base for the blend, from the SAME mosaic demosaiced before PMRID --
               so one UnpackCfa yields both the clean and denoised bases
               (MW::ensureRawDenoise) instead of a second full decode. PMRID::Apply
               mutates raw in place, so this must run first. Demosaic/RawColor do not
               mutate raw, so raw stays valid for the denoised pass below. */
            if (outClean) {
                Demosaic cleanDemosaic;
                std::vector<float> rgbClean;
                RawColor cleanColor;
                auto cleanWork = std::make_shared<WorkingImage>();
                auto cleanProg = [&stage](int d, int t) { stage(0, 250, d, t); };
                if (cleanDemosaic.Run(raw, rgbClean, Demosaic::Bilinear, abort, cleanProg) &&
                    cleanColor.ToWorking(raw, rgbClean, *cleanWork)) {
                    *outClean = cleanWork;
                }
                if (aborted()) { errMsg = "Aborted"; return false; }
            }
            PMRID::Apply(raw, m.ISONum, m.model,
                         [&stage](int d, int t) { stage(250, 500, d, t); });
            if (aborted()) { errMsg = "Aborted"; return false; }
        }

        Demosaic demosaic;
        std::vector<float> rgb;
        /* The denoised (post-PMRID) demosaic is the last slice of the progress bar. */
        std::function<void(int, int)> demProg;
        if (denoiseRaw) demProg = [&stage](int d, int t) { stage(750, 250, d, t); };
        if (!demosaic.Run(raw, rgb, Demosaic::Bilinear, abort, demProg)) {
            errMsg = aborted() ? "Aborted" : "Demosaic failed (unsupported CFA pattern?).";
            return false;
        }

        RawColor color;
        if (!color.ToWorking(raw, rgb, *work)) {
            errMsg = "Colour conversion failed.";
            return false;
        }

        /* Developed display/export base for a saved "Denoise raw" edit (the normal cache decode,
           denoiseRaw == false; the denoiseRaw path already put the full-strength result in *work).
           PMRID is pre-demosaic, so denoise a COPY of the mosaic, demosaic it a second time, and
           blend toward the clean *work by the saved amounts -- matching the interactive render
           (MW::ensureRawDenoise) while leaving *work (the cached base) clean. Winnow engine + Bayer
           only; PMRID::Apply returns false otherwise, so we skip the extra work. */
        if (!denoiseRaw && edit &&
            (edit->denoiseLuma > 0.0f || edit->denoiseChroma > 0.0f) &&
            G::decodeRawEngine == G::DecodeRawEngine::winnowDecodeRawEngine) {
            RawImage rawDen = raw;
            if (PMRID::Apply(rawDen, m.ISONum, m.model)) {
                std::vector<float> rgbDen;
                WorkingImage pmridWork;
                if (demosaic.Run(rawDen, rgbDen, Demosaic::Bilinear, abort) &&
                    color.ToWorking(rawDen, rgbDen, pmridWork)) {
                    denoisedBase = std::make_shared<WorkingImage>();
                    Develop::BlendRawDenoise(*work, pmridWork, edit->denoiseLuma,
                                             edit->denoiseChroma, *denoisedBase);
                }
            }
            if (aborted()) { errMsg = "Aborted"; return false; }
        }
    }
    if (aborted()) { errMsg = "Aborted"; return false; }

    /* Hand the caller the pre-develop WorkingImage for the WorkingImageCache (shared, no copy). It
       stays CLEAN -- no denoise, no develop: the interactive develop path applies "Denoise raw" on
       top of this cached image with its own async cache (see MW::ensureRawDenoise), and the
       non-identity render below runs on a private copy so the cached image is never mutated. */
    if (outWork) *outWork = work;

    /* RAW develops in its native linear float (better than an 8-bit round trip), so the develop
       stage runs here rather than in ImageDecoder for raw files. This is the NON-interactive path
       (a decode carrying saved edits -> the display/export image). "Denoise raw" is the PMRID
       PRE-demosaic denoiser: when set it is baked into denoisedBase above (a blended copy),
       leaving *work clean, and the develop pass renders from that base so the exported/displayed
       image matches the interactive preview (MW::ensureRawDenoise). */
    OutputTransform output;
    if (edit && !edit->isIdentity()) {
        WorkingImage developed = denoisedBase ? *denoisedBase : *work;
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
