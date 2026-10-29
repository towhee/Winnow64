#include "ImageFormats/Raw/rawformat.h"
#include "ImageFormats/Raw/demosaic.h"
#include "ImageFormats/Raw/rawcolor.h"
#include "ImageFormats/Raw/pmrid.h"
#include "ImageFormats/Raw/pmridcache.h"
#include "ImageFormats/Raw/applerawdecode.h"
#include "Develop/workingimage.h"
#include "Develop/develop.h"
#include "Develop/outputtransform.h"
#include "Main/global.h"
#include <algorithm>
#include <QHash>
#include <QElapsedTimer>
#include <QFileInfo>
#include <mutex>
#include <optional>

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

bool RawFormat::PreviewTooSmall(const QString &ext, int previewW, int previewH)
{
    const int longEdge = qMax(previewW, previewH);
    return ext == "dng" && longEdge > 0 && longEdge < 1024;
}

/* Last denoise-decode timing snapshot (see rawformat.h). */
static std::mutex g_denoiseTimingMutex;
static RawFormat::DenoiseTiming g_lastDenoiseTiming;

RawFormat::DenoiseTiming RawFormat::LastDenoiseTiming()
{
    std::lock_guard<std::mutex> lock(g_denoiseTimingMutex);
    return g_lastDenoiseTiming;
}

QString RawFormat::DenoiseTiming::Summary() const
{
    if (!valid) return "Denoise decode: not run this session";
    auto ms = [](double v) { return QString::number(v, 'f', 1); };
    const double other = totalMs - (pmridWaitMs + unpackMs + blackMs + cleanDemosaicMs +
                                    cleanColorMs +
                                    mosaicCopyMs + pmridMs + denDemosaicMs + denColorMs +
                                    blendMs);
    const QString reuse = pmridReuse.isEmpty() ? QString("ran")
                                               : "reused, " + pmridReuse;
    return QString("Denoise decode (%1) %2: total %3 ms | cache wait %4, unpack %5, "
                   "black %6, clean demosaic %7, clean color %8, mosaic copy %9, "
                   "PMRID %10 (%11), denoised demosaic %12, denoised color %13, blend %14, "
                   "other %15")
        .arg(interactive ? "interactive" : "saved edit", file, ms(totalMs), ms(pmridWaitMs),
             ms(unpackMs), ms(blackMs), ms(cleanDemosaicMs), ms(cleanColorMs),
             ms(mosaicCopyMs))
        .arg(ms(pmridMs), reuse, ms(denDemosaicMs), ms(denColorMs), ms(blendMs), ms(other));
}

/* Publish a denoise-decode timing for the Develop diagnostics (not printed). */
static void PublishDenoiseTiming(const RawFormat::DenoiseTiming &t)
{
    std::lock_guard<std::mutex> lock(g_denoiseTimingMutex);
    g_lastDenoiseTiming = t;
}

bool RawFormat::Decode(QFile &file, const ImageMetadata &m, QImage &out,
                       const EditParams *edit, const QAtomicInt *abort,
                       std::shared_ptr<const WorkingImage> *outWork,
                       bool denoiseRaw,
                       const std::function<void(int, int)> &denoiseProgress,
                       std::shared_ptr<const WorkingImage> *outClean,
                       bool *outDenoiseApplied,
                       const RawRender::Choice *render)
{
/*
    Shared, camera-agnostic pipeline:
        UnpackCfa()         per-format bitstream -> normalised CFA mosaic   (virtual)
        SubtractBlack()                                                      (shared)
        Demosaic::Run()     mosaic -> linear camera RGB                      (shared)
        RawColor::ToCameraNative() characterise ONLY -> CameraNative WorkingImage (shared)
        Develop::Apply()    input profile (stage 0) + adjustments, in linear  (shared)
        OutputTransform::ToImage() view transform + primaries + gamma         (shared)

    NOTE the decode deliberately stops at CAMERA-NATIVE primaries: no white balance, no
    colour matrix, no clamp. Those moved into Develop so that the buffer cached by
    WorkingImageCache sits UPSTREAM of every colour decision -- changing a profile or the
    white balance is then a re-render, not a re-decode. See rawcolor.h.

    abort is polled between stages (and inside the demosaic loop) so a long decode bails
    promptly when the decoder thread is asked to stop -- otherwise ImageCache::stop()'s
    BlockingQueuedConnection waits for the whole demosaic and the UI beachballs.
*/
    const auto aborted = [abort]{ return abort && abort->loadAcquire(); };

    /* Stage timing, published only when PMRID actually ran (PublishDenoiseTiming). */
    QElapsedTimer totalT, lapT;
    totalT.start();
    lapT.start();
    auto lap = [&lapT]() { const double v = lapT.nsecsElapsed() / 1e6; lapT.restart(); return v; };
    DenoiseTiming dt;
    dt.file = QFileInfo(file.fileName()).fileName();
    dt.interactive = denoiseRaw;

    /* Reported to the caller only if PMRID really ran (see the header). Default false so
       every early return -- unsupported format, abort, Apple engine -- reads as "not
       denoised" rather than leaving the caller's flag untouched. */
    if (outDenoiseApplied) *outDenoiseApplied = false;

    /* The renderer: the image's own (per image -- Develop/rawrender.h), else the default.
       Every base this decode hands back is tagged with it for WorkingImageCache, even
       when an Apple decode soft-fails over to Winnow below: the tag says what the
       recipe asked for, which is what the next lookup will ask for too. */
    const RawRender::Choice choice = render ? *render : RawRender::defaultChoice();
    auto work = std::make_shared<WorkingImage>();
    work->rawRenderTag = choice.tag();
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
    if (choice.apple) {
        QString aerr;
        if (AppleRawDecode::Decode(file.fileName(), *work, aerr, choice.applePin)
            && work->isValid()) {
            work->rawRenderTag = choice.tag();      // Decode fills the whole image
            decoded = true;

            /*
                CHARACTERISE what Core Image handed back. It applies its own profile and
                returns linear sRGB, so it leaves cam invalid -- and an invalid cam makes
                WhiteBalance::resolve return its literal 6500 K / 0 default, which is what
                the Temp/Tint row showed on EVERY raw file under this engine regardless of
                what the camera actually metered.

                ReadAsShotColor re-walks only the file HEADER for the matrix and the
                multipliers -- the same read the vendor's UnpackCfa does, shared so the
                two cannot drift. Nothing is decoded. The model is what a camera profile
                would later be looked up by.

                SAFE FOR THE RENDER: the pixels stay tagged kWorking, and Develop's
                stage-0 preMat fold is gated on CameraNative, so the input matrix is
                still not applied twice. cam is used for the WB READOUT and for
                relativeGains -- and relativeGains is exactly right here, because Core
                Image's output already carries the as-shot balance this cam describes.
            */
            RawSensorInfo ci = m.rawInfo;       // seeded: Sony fills some at parse
            if (ReadAsShotColor(file, m, ci) || ci.hasColorMatrix) {
                RawColor::Characterise(ci.xyzToCam, ci.camMul, work->cam);
                work->cam.cameraModel = m.model;
            }
        }
        /* else: aerr is a soft failure -- fall through to the in-house engine. */
    }
#endif

    /*
        LINEAR RAW (UnpackLinear): already-demosaiced camera RGB, e.g. a Lightroom
        pano/HDR merge DNG. It goes straight to the camera-native base -- no
        SubtractBlack, demosaic or PMRID, all of which need a mosaic. Asked BEFORE the
        PMRID claim below, so such a file never claims (or waits on) a denoised base
        nobody can make. Setting decoded hands the rest of the function the same state
        the Apple engine leaves.
    */
    if (!decoded) {
        RawImage linRaw;
        std::vector<float> linRgb;
        const LinearResult linear = UnpackLinear(file, m, linRaw, linRgb, abort);
        if (linear == LinearResult::Failed) {
            if (errMsg.isEmpty()) errMsg = "Linear Raw unpack failed.";
            return false;
        }
        if (aborted()) { errMsg = "Aborted"; return false; }
        if (linear == LinearResult::Ok) {
            dt.unpackMs = lap();
            RawColor color;
            if (!color.ToCameraNative(linRaw, std::move(linRgb), *work)) {
                errMsg = "Colour conversion failed.";
                return false;
            }
            work->cam.cameraModel = m.model;
            work->rawRenderTag = choice.tag();
            decoded = true;
        }
    }

    /*
        SHARED PMRID BASE (PmridCache). Every decode that needs the full-strength denoised
        base -- the interactive pass (denoiseRaw) or a saved "Denoise raw" recipe -- gets it
        through the cache, so the Develop denoise and the ImageCache decode of the same
        image never both run the model. Winnow engine only, so on that engine the Apple
        branch above never ran and decoded is still false.

        CLAIM EARLY, WAIT LATE. tryAcquire up front, without blocking: Hit = reuse, Claimed
        = this decode makes it and publishes it (the Claim abandons on every early return,
        and a waiter takes over), InFlight = another decode is making it. An InFlight
        decode does NOT wait yet: it unpacks and demosaics its own CLEAN base first, in
        parallel with the producer, and blocks (awaitPmrid) only at the point it needs the
        denoised one. Waiting up front serialised that ~0.7 s behind the whole producer
        (measured 2026-10-13: the second decode finished 0.7 s after the first).

        ONE PRODUCER PATH. The denoised base is always made by makeDenoised() from a
        mosaic -- PMRID::Apply, then demosaic + RawColor -- and the clean base (*work) is
        always the main demosaic, so the interactive and saved-edit passes differ only in
        which of the two the caller gets.
    */
    const bool winnowEngine = !choice.apple;
    /* WANTED, not merely non-zero: the two amounts default NON-ZERO, so they cannot say
       whether denoise was asked for -- EditParams::denoiseRaw does (falling back to the
       Auto run preference when the recipe says nothing). Note the two spellings in this
       scope: the `denoiseRaw` PARAMETER is "this decode is the full-strength PMRID pass",
       edit->denoiseRaw is the stored intent. */
    const bool savedDenoise = !denoiseRaw && edit && winnowEngine &&
                              edit->wantsDenoiseRaw(G::autoRunDenoise) &&
                              (edit->denoiseLumaOr(G::denoiseLumaDefault) > 0.0f ||
                               edit->denoiseChromaOr(G::denoiseChromaDefault) > 0.0f);
    const bool wantPmrid = !decoded && ((denoiseRaw && winnowEngine) || savedDenoise);
    std::shared_ptr<const WorkingImage> pmridHit;
    std::optional<PmridCache::Claim> pmridClaim;
    bool pmridPending = false;          // InFlight elsewhere: awaitPmrid() before use
    if (wantPmrid) {
        lap();
        const PmridCache::Result r =
            PmridCache::instance().tryAcquire(file.fileName(), m.ISONum, pmridHit);
        dt.pmridWaitMs += lap();
        if (r == PmridCache::Result::Claimed) pmridClaim.emplace(file.fileName(), m.ISONum);
        else if (r == PmridCache::Result::Hit) dt.pmridReuse = "cached";
        else pmridPending = true;
    }
    /* Block for the base another decode is making. Ends Hit/Waited (reuse) or Claimed
       (its producer gave up: make it here). False = aborted while waiting. */
    auto awaitPmrid = [&]() -> bool {
        lap();
        const PmridCache::Result r =
            PmridCache::instance().acquire(file.fileName(), m.ISONum, abort, pmridHit);
        dt.pmridWaitMs += lap();
        pmridPending = false;
        if (r == PmridCache::Result::Aborted) return false;
        if (r == PmridCache::Result::Claimed) pmridClaim.emplace(file.fileName(), m.ISONum);
        else dt.pmridReuse = (r == PmridCache::Result::Hit) ? "cached" : "waited";
        return true;
    };

    /* An interactive pass that wants only the denoised base has nothing of its own to do
       while another decode makes it, so it waits now -- and on a reuse skips the decode. */
    if (denoiseRaw && pmridPending && !outClean && !awaitPmrid()) {
        errMsg = "Aborted";
        return false;
    }
    /* The base the caller gets (outWork) and the identity render below start from: work,
       unless this is the interactive pass and a denoised base was reused or made. */
    std::shared_ptr<const WorkingImage> resultBase;
    if (denoiseRaw && pmridHit) {
        resultBase = pmridHit;
        dt.valid = true;
        if (outDenoiseApplied) *outDenoiseApplied = true;
    }

    if (!decoded && !(denoiseRaw && pmridHit && !outClean)) {
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
        dt.unpackMs = lap();

        SubtractBlack(raw);
        dt.blackMs = lap();

        /* Combined status-bar progress for a "Denoise raw" decode: the clean demosaic,
           the PMRID model, and the denoised demosaic each drive a slice of one 0..1000 bar
           so the user sees continuous movement, not an empty bar through the demosaics.
           Inert when the caller passed no progress sink (a normal decode). */
        auto stage = [&denoiseProgress](int base, int span, int done, int total) {
            if (denoiseProgress && total > 0)
                denoiseProgress(base + span * done / total, 1000);
        };

        Demosaic demosaic;
        RawColor color;

        /* PMRID pre-demosaic denoise of mosaic (MUTATED in place -- pass a copy if the
           mosaic is still needed), then demosaic + RawColor: the full-strength base.
           nullptr when PMRID did not run (non-Bayer, no model) or a stage failed; then
           mosaic is unmodified (Apply returns true iff it changed it). */
        auto makeDenoised = [&](RawImage &mosaic, bool withProgress)
            -> std::shared_ptr<WorkingImage> {
            lap();
            PMRID::ProgressFn pmridProg;
            std::function<void(int, int)> denProg;
            if (withProgress) {
                pmridProg = [&stage](int d, int t) { stage(250, 500, d, t); };
                denProg   = [&stage](int d, int t) { stage(750, 250, d, t); };
            }
            const bool applied = PMRID::Apply(mosaic, m.ISONum, m.model, pmridProg,
                                              &dt.pmrid);
            dt.pmridMs = lap();
            if (!applied || aborted()) return nullptr;
            std::vector<float> rgbDen;
            auto den = std::make_shared<WorkingImage>();
            const bool demosaiced =
                demosaic.Run(mosaic, rgbDen, Demosaic::Bilinear, abort, denProg);
            dt.denDemosaicMs = lap();
            if (!demosaiced || !color.ToCameraNative(mosaic, rgbDen, *den)) return nullptr;
            dt.denColorMs = lap();
            den->cam.cameraModel = m.model;
            return den;
        };

        /* The CLEAN base: every decode but an interactive pass that did not ask for
           outClean (that one wants only the denoised base). */
        const bool needClean = !denoiseRaw || outClean;
        auto demosaicClean = [&]() -> bool {
            std::vector<float> rgb;
            /* The clean demosaic is the first slice of a denoise bar; a plain decode with
               a progress sink gives it the whole bar (MW::ensureDevelopWork on open). */
            std::function<void(int, int)> demProg;
            if (denoiseRaw) demProg = [&stage](int d, int t) { stage(0, 250, d, t); };
            else if (denoiseProgress) demProg = [&stage](int d, int t) { stage(0, 1000, d, t); };
            lap();
            if (!demosaic.Run(raw, rgb, Demosaic::Bilinear, abort, demProg)) {
                errMsg = aborted() ? "Aborted" : "Demosaic failed (unsupported CFA pattern?).";
                return false;
            }
            dt.cleanDemosaicMs = lap();
            if (!color.ToCameraNative(raw, rgb, *work)) {
                errMsg = "Colour conversion failed.";
                return false;
            }
            dt.cleanColorMs = lap();
            /* The camera profile a render may later select is found by MODEL, and nothing
               downstream of here knows the file -- WorkingImageCache hands back an image
               with no path attached. RawColor cannot fill it: it is handed the RawImage,
               not the metadata. See CameraColor::cameraModel. */
            work->cam.cameraModel = m.model;
            work->rawRenderTag = choice.tag();      // whatever ToCameraNative reset
            return true;
        };
        if (needClean && !demosaicClean()) return false;
        if (aborted()) { errMsg = "Aborted"; return false; }

        if (denoiseRaw) {
            /* Interactive full-strength pass (MW::ensureRawDenoise). */
            if (outClean) *outClean = work;
            if (pmridPending && !awaitPmrid()) { errMsg = "Aborted"; return false; }
            if (pmridHit) {
                resultBase = pmridHit;
                dt.valid = true;
                if (outDenoiseApplied) *outDenoiseApplied = true;
            }
            else if (pmridClaim) {
                /* raw's last use, so denoise it in place -- no copy. */
                if (auto den = makeDenoised(raw, true)) {
                    pmridClaim->publish(den);
                    resultBase = den;
                    dt.valid = true;
                    if (outDenoiseApplied) *outDenoiseApplied = true;
                }
                if (aborted()) { errMsg = "Aborted"; return false; }
            }
            /* Nothing denoised (non-Bayer, no model, not the Winnow engine): the caller
               gets the clean image, as before -- decode it if it was skipped above. raw
               is unmodified, since Apply changes it only when it applies. */
            if (!resultBase && !needClean && !demosaicClean()) return false;
        }

        /* Developed display/export base for a saved "Denoise raw" edit: blend the
           full-strength base toward the clean *work by the saved amounts -- matching the
           interactive render (MW::ensureRawDenoise) while leaving *work (the cached
           base) clean. */
        if (savedDenoise) {
            if (pmridPending && !awaitPmrid()) { errMsg = "Aborted"; return false; }
            std::shared_ptr<const WorkingImage> den = pmridHit;
            if (!den && pmridClaim) {
                lap();
                RawImage rawDen = raw;        // keep raw clean (it is *work's source)
                dt.mosaicCopyMs = lap();
                if (auto made = makeDenoised(rawDen, false)) {
                    pmridClaim->publish(made);
                    den = made;
                }
                if (aborted()) { errMsg = "Aborted"; return false; }
            }
            if (den) {
                lap();
                denoisedBase = std::make_shared<WorkingImage>();
                Develop::BlendRawDenoise(*work, *den,
                                         edit->denoiseLumaOr(G::denoiseLumaDefault),
                                         edit->denoiseChromaOr(G::denoiseChromaDefault),
                                         *denoisedBase);
                denoisedBase->rawRenderTag = choice.tag();
                dt.blendMs = lap();
                dt.valid = true;
            }
            if (outDenoiseApplied) *outDenoiseApplied = (den != nullptr);
        }
    }
    if (dt.valid) {
        dt.totalMs = totalT.nsecsElapsed() / 1e6;
        PublishDenoiseTiming(dt);
    }
    if (aborted()) { errMsg = "Aborted"; return false; }

    /* Hand the caller the pre-develop WorkingImage for the WorkingImageCache (shared, no copy). It
       stays CLEAN -- no denoise, no develop: the interactive develop path applies "Denoise raw" on
       top of this cached image with its own async cache (see MW::ensureRawDenoise), and the
       non-identity render below runs on a private copy so the cached image is never mutated. */
    if (!resultBase) resultBase = work;
    if (outWork) *outWork = resultBase;

    /* RAW develops in its native linear float (better than an 8-bit round trip), so the develop
       stage runs here rather than in ImageDecoder for raw files. This is the NON-interactive path
       (a decode carrying saved edits -> the display/export image). "Denoise raw" is the PMRID
       PRE-demosaic denoiser: when set it is baked into denoisedBase above (a blended copy),
       leaving *work clean, and the develop pass renders from that base so the exported/displayed
       image matches the interactive preview (MW::ensureRawDenoise). */
    OutputTransform output;
    /* The view transform is part of the recipe, so it has to travel with it here too:
       without it a Filmic (or AgX) image would render untone-mapped whenever it came
       through this decode instead of the interactive path. No recipe (edit == nullptr)
       means no tone mapping, the same as an untouched image. */
    const OutputTransform::ViewTransform view =
        edit ? OutputTransform::ViewFromInt(edit->viewTransform)
             : OutputTransform::ViewTransform::None;
    if (edit && !edit->isIdentity()) {
        WorkingImage developed = denoisedBase ? *denoisedBase : *resultBase;
        Develop develop;
        develop.Apply(developed, *edit);
        if (aborted()) { errMsg = "Aborted"; return false; }
        if (!output.ToImage(developed, out, OutputTransform::Space::sRGB, view)) {
            errMsg = "Output transform failed.";
            return false;
        }
        return true;
    }
    if (aborted()) { errMsg = "Aborted"; return false; }

    /* No (or identity) edits, so Develop::Apply never ran -- but its STAGE 0 is not an
       edit: the in-house engine hands over CameraNative pixels, and OutputTransform has
       no matrix for those (ColorSpaceMath::matrix returns the identity for CameraNative),
       so passing them straight through renders sensor data -- the green cast of an
       unbalanced raw. Convert a COPY: *work is the shared cache base and must stay
       camera-native for the develop path's own stage 0. */
    if (resultBase->space == ColorSpaceMath::ColorSpace::CameraNative) {
        WorkingImage converted = *resultBase;
        Develop::ToWorkingSpace(converted);
        if (!output.ToImage(converted, out, OutputTransform::Space::sRGB, view)) {
            errMsg = "Output transform failed.";
            return false;
        }
        return true;
    }

    if (!output.ToImage(*resultBase, out, OutputTransform::Space::sRGB, view)) {
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
