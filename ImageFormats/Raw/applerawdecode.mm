#include "ImageFormats/Raw/applerawdecode.h"

#include <QtGlobal>
#include <QByteArray>
#include <QCoreApplication>
#include <QHash>
#include <QMutex>
#include <atomic>
#include <dlfcn.h>
#include <vector>

#ifdef Q_OS_MAC

#import <Foundation/Foundation.h>
#import <CoreImage/CoreImage.h>
#import <CoreGraphics/CoreGraphics.h>
#import <ImageIO/ImageIO.h>

/*
    Engine A: macOS Core Image RAW decode -> scene-linear WorkingImage. See applerawdecode.h.

    NOTE (MRC): this .mm compiles under manual reference counting, not ARC. Cocoa objects from
    +filter.../+context... are autoreleased and stay valid until the @autoreleasepool drains;
    we copy all pixels into the WorkingImage before that, so no manual retain is needed. The
    Core Foundation objects we own outright are the two CGColorSpaceRefs (Create rule) --
    released explicitly below.
*/
namespace AppleRawDecode {

namespace {
std::atomic<int> versionPin{0};

/*  WHAT EACH DECODE CHOSE, for the Raw panel. Keyed on the decoded path; written on a
    decoder thread, read on the GUI thread. Only paths decoded this session are here,
    which in Develop is the current image as soon as it opens. */
struct Chosen { QList<int> newestFirst; int used = 0; };
QMutex chosenMutex;
QHash<QString, Chosen> chosenByPath;
std::function<void(const QString &)> decodedCallback;

int VersionNumber(NSString *v) { return v.intValue; }    // "9" and "9.dng" -> 9

/*  The pin applied to one file's list (oldest first): the newest version <= the pin, or
    the oldest the file has when the pin is older than all of them. A list holding both
    "7" and "7.dng" gives the later, the DNG variant, as Core Image orders it. */
NSString *PickVersion(NSArray<NSString *> *supported, int pin)
{
    if (supported.count == 0) return nil;
    if (pin <= 0) return supported.lastObject;
    NSString *pick = nil;
    for (NSString *v in supported)
        if (VersionNumber(v) <= pin) pick = v;
    return pick ? pick : supported.firstObject;
}

void Record(const QString &path, NSArray<NSString *> *supported, NSString *used)
{
    Chosen c;
    for (NSString *v in supported.reverseObjectEnumerator) {
        const int n = VersionNumber(v);
        if (!c.newestFirst.contains(n)) c.newestFirst << n;
    }
    c.used = VersionNumber(used);
    {
        QMutexLocker lock(&chosenMutex);
        chosenByPath.insert(path, c);
    }
    if (decodedCallback && qApp)
        QMetaObject::invokeMethod(qApp, [path] {
            if (decodedCallback) decodedCallback(path);
        }, Qt::QueuedConnection);
}
} // namespace

int VersionPin() { return versionPin.load(); }
void SetVersionPin(int pin) { versionPin.store(qMax(0, pin)); }

int EffectiveVersionPin()
{
    const int pin = VersionPin();
    return pin > 0 ? pin : LatestDecoderVersion();
}

bool DecodedVersions(const QString &path, QList<int> &newestFirst, int &used)
{
    QMutexLocker lock(&chosenMutex);
    const auto it = chosenByPath.constFind(path);
    if (it == chosenByPath.constEnd()) return false;
    newestFirst = it->newestFirst;
    used = it->used;
    return true;
}

void SetDecodedCallback(std::function<void(const QString &path)> cb)
{
    decodedCallback = std::move(cb);       // GUI thread, once, before any decode
}

bool Decode(const QString &path, WorkingImage &out, QString &err)
{
    if (@available(macOS 12.0, *)) {
        @autoreleasepool {
            NSURL *url = [NSURL fileURLWithPath:path.toNSString()];
            CIRAWFilter *f = [CIRAWFilter filterWithImageURL:url];
            if (f == nil) {
                err = QStringLiteral("CIRAWFilter could not open the file.");
                return false;
            }

            /* SCENE-LINEAR AND UNSHARPENED, LIKE THE WINNOW ENGINE -- BUT APPLE'S NOISE
               REDUCTION STAYS ON. Decided 2026-10-13.

               TONE: boostAmount 0 is "no global tone curve, linear response". The default
               (1) is Apple's tone curve, which Develop's OutputTransform then applied a
               second time -- mid-tones nearly doubled (0.062 -> 0.102 on a D850) with
               extra saturation, so Apple rendered brighter and harder than Winnow.
               Verified linear: with boost 0, +1 EV doubles every sampled pixel (2.00)
               on D850, CR3, X-T50 and A1 II files. boostShadowAmount only acts through
               the boost. Local tone map and contrast default to 0 on those files, but
               Apple says the default "will vary per image", so both are forced to 0.

               SHARPENING OFF (sharpness and edge "detail enhancement"): Develop has its
               own Detail sharpening with radius and masking, applied after the user's
               edits; Apple's would be a second, uncontrollable pass, and pre-alignment
               sharpening hurts focus stacking.

               NOISE REDUCTION LEFT AT APPLE'S PER-IMAGE DEFAULTS, on purpose: it is much
               improved in decoder 9 and it is why this engine is worth choosing. PMRID
               ("Denoise raw") needs the CFA mosaic and is Winnow-only, so the two do not
               compete. This reverses the earlier plan of handing PMRID a noisy image. */
            f.boostAmount = 0.0;
            f.boostShadowAmount = 0.0;
            if (f.isLocalToneMapSupported) f.localToneMapAmount = 0.0;
            if (f.isContrastSupported)     f.contrastAmount = 0.0;
            if (f.isSharpnessSupported)    f.sharpnessAmount = 0.0;
            if (f.isDetailSupported)       f.detailAmount = 0.0;

            /* Output SENSOR orientation -- do NOT bake in EXIF orientation. Raw formats are in
               metadata->rotateFormats, so the shared pipeline (ImageDecoder::rotate) applies
               orientation to the QImage AFTER decode, exactly as it does for engine B. By
               default CIRAWFilter orients its output from the file's metadata; left on, that
               pre-rotated image is then rotated a SECOND time downstream. Forcing "up" (identity)
               makes engine A hand off un-oriented pixels, matching engine B's contract. */
            f.orientation = kCGImagePropertyOrientationUp;

            /* THE DECODER VERSION IS ALWAYS SET, never left to Core Image. The header
               says a new filter defaults to the newest version, and it does not:
               measured on macOS 27, CR3, D850 NEF and X-T50 RAF all list 7,8,9 yet
               default to 8, while version 9 decodes them (different pixels) with its
               resources already installed. The user's pin (Raw panel, "Apple Raw N";
               0 = newest) picks from this file's list -- see PickVersion. */
            NSArray<NSString *> *supported = f.supportedDecoderVersions;
            NSString *defaultVersion = f.decoderVersion;
            NSString *wanted = PickVersion(supported, VersionPin());
            if (wanted != nil) f.decoderVersion = wanted;

            CIImage *img = f.outputImage;
            if (img == nil && wanted != nil && ![wanted isEqualToString:defaultVersion]) {
                /* Version 9 can need on-demand resources this Mac has not fetched. Decode
                   with Core Image's own choice now and ask for the download, so a later
                   decode gets the wanted version; a decode never waits on the network. */
#if defined(__MAC_OS_X_VERSION_MAX_ALLOWED) && __MAC_OS_X_VERSION_MAX_ALLOWED >= 270000
                if (@available(macOS 27.0, *))
                    [f downloadResourcesWithTimeout:300 completionHandler:^(NSError *) {}];
#endif
                f.decoderVersion = defaultVersion;
                img = f.outputImage;
            }
            NSLog(@"CIRAWFilter %@ supportedDecoderVersions=%@ default=%@ active=%@",
                  path.toNSString(), supported, defaultVersion, f.decoderVersion);
            if (img == nil) {
                err = QStringLiteral("CIRAWFilter produced no output image.");
                return false;
            }
            Record(path, supported, f.decoderVersion);

            /* VERIFY ON FIRST RUN: ROW ORDER -- render:toBitmap writes rows in Core Image's
               bottom-left origin; if the result is vertically flipped vs engine B, reverse the
               row copy below. (EXIF orientation is handled: f.orientation forced to Up above, so
               output is sensor orientation and ImageDecoder::rotate applies orientation once,
               matching engine B.) */

            const CGRect extent = img.extent;
            if (CGRectIsInfinite(extent) || extent.size.width < 1 || extent.size.height < 1) {
                err = QStringLiteral("CIRAWFilter output has no finite extent.");
                return false;
            }
            const int W = static_cast<int>(extent.size.width);
            const int H = static_cast<int>(extent.size.height);

            /* OUTPUT IN DEVELOP'S WORKING SPACE, extended linear Rec.2020 / D65
               (ColorSpaceMath::kWorking), into 32-bit float RGBA: scene-linear, with
               highlight headroom (>1.0) intact.

               IT WAS LINEAR sRGB, tagged kWorking, from before the working space became
               Rec.2020: every colour read as far more saturated than it was and greens
               swung to cyan (a grey-green background rendered teal). The assert makes a
               future change of working space fail here rather than in a picture.

               THE WORKING SPACE STAYS CORE IMAGE'S OWN, extended linear sRGB (extended,
               so nothing is clipped). Measured 2026-10-13 on a D850 NEF: decoder 9 with a
               Rec.2020 WORKING space renders wrong colour (centre pixel blue 0.245 ->
               0.162, red negative), while decoder 8 does not care; with an sRGB working
               space and Rec.2020 output, both versions match the BT.2087 sRGB->Rec.2020
               matrix to four places. */
            static_assert(ColorSpaceMath::kWorking
                              == ColorSpaceMath::ColorSpace::LinearRec2020,
                          "Core Image must output Develop's working space");
            CGColorSpaceRef workCs =
                CGColorSpaceCreateWithName(kCGColorSpaceExtendedLinearSRGB);
            CGColorSpaceRef outCs =
                CGColorSpaceCreateWithName(kCGColorSpaceExtendedLinearITUR_2020);
            if (workCs == nullptr || outCs == nullptr) {
                if (workCs) CGColorSpaceRelease(workCs);
                if (outCs) CGColorSpaceRelease(outCs);
                err = QStringLiteral("Could not create the linear colour spaces.");
                return false;
            }
            CIContext *ctx = [CIContext contextWithOptions:@{
                kCIContextWorkingColorSpace : (id)workCs,
                kCIContextOutputColorSpace  : (id)outCs,
                kCIContextWorkingFormat     : @(kCIFormatRGBAf),
            }];

            const size_t pixels   = static_cast<size_t>(W) * static_cast<size_t>(H);
            const size_t rowBytes = static_cast<size_t>(W) * 4 * sizeof(float);
            std::vector<float> rgba(pixels * 4);
            [ctx render:img
               toBitmap:rgba.data()
               rowBytes:rowBytes
                 bounds:extent
                 format:kCIFormatRGBAf
             colorSpace:outCs];

            CGColorSpaceRelease(workCs);
            CGColorSpaceRelease(outCs);

            /* Deinterleave RGBA float -> WorkingImage's interleaved RGB float (drop alpha). */
            out.width  = W;
            out.height = H;
            out.white  = 1.0f;
            out.sceneReferred = true;   // sensor data: scene-linear, with headroom
            /*
                Core Image has ALREADY applied its own camera profile and handed back
                linear Rec.2020 primaries, so unlike the Winnow engine this path arrives in
                the working space and Develop's stage 0 is a no-op for it. Tagged
                explicitly rather than left to the default: the whole point of the tag is
                that no stage has to guess, and `cam` is left invalid here, which is what
                tells Develop there is no camera characterisation to profile with. A
                camera-profile control is therefore meaningless under this engine and
                must be disabled (with a reason) rather than silently ignored.
            */
            out.space = ColorSpaceMath::kWorking;
            out.rgb.resize(pixels * 3);
            for (size_t i = 0; i < pixels; ++i) {
                out.rgb[i * 3 + 0] = rgba[i * 4 + 0];
                out.rgb[i * 3 + 1] = rgba[i * 4 + 1];
                out.rgb[i * 3 + 2] = rgba[i * 4 + 2];
            }
            return true;
        }
    }

    err = QStringLiteral("Core Image RAW decode requires macOS 12 or later.");
    return false;
}

int LatestDecoderVersion()
{
/*
    Looked up by SYMBOL NAME at run time rather than read from CIRAWDecoderVersion9: a
    direct reference fixes the answer at whatever the SDK knew (a later macOS adding
    version 10 would still say 9), and on an older macOS without the constant it is an
    unresolved symbol at launch. Each CIRAWDecoderVersionN is an exported NSString *.
*/
    static const int latest = [] {
        for (int v = 30; v >= 6; --v) {
            const QByteArray sym = "CIRAWDecoderVersion" + QByteArray::number(v);
            if (dlsym(RTLD_DEFAULT, sym.constData()) != nullptr) return v;
        }
        return 0;
    }();
    return latest;
}

} // namespace AppleRawDecode

#endif // Q_OS_MAC
