#include "ImageFormats/Raw/applerawdecode.h"

#include <QtGlobal>
#include <QByteArray>
#include <QCoreApplication>
#include <QHash>
#include <QMutex>
#include <QSet>
#include <QThreadPool>
#include <QDir>
#include <QCryptographicHash>
#include <sys/sysctl.h>
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
struct Chosen { QList<int> newestFirst; int used = 0; bool appleLook = false; };
QMutex chosenMutex;
QHash<QString, Chosen> chosenByPath;
std::function<void(const QString &)> decodedCallback;
std::atomic<bool> fingerprintStale{true};

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

/*  Does Core Image turn its local tone map on for this file by default? Apple sets the
    default per image: 0 on ordinary raws, 1 on iPhone ProRAW, where it is how the file's
    ProfileGainTableMap is applied. That is the test for keeping Apple's own rendering
    (see Decode). Read BEFORE anything is set on the filter. */
bool WantsAppleLook(CIRAWFilter *f) API_AVAILABLE(macos(12.0))
{
    return f.isLocalToneMapSupported && f.localToneMapAmount > 0.0f;
}

void Record(const QString &path, NSArray<NSString *> *supported, NSString *used,
            bool appleLook)
{
    Chosen c;
    c.appleLook = appleLook;
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
    return EffectiveVersionPin(VersionPin());
}

int EffectiveVersionPin(int pin)
{
    return pin > 0 ? pin : LatestDecoderVersion();
}

QString SupportFingerprint()
{
/*
    See the header. Three ingredients, read once and again only when marked stale (a
    resource download this process asked for completed):

      - the macOS build: Core Image's decoders ship with the OS;
      - RawCamera.bundle's version: Apple's RAW engine, which the Digital Camera RAW
        Compatibility updates replace;
      - the names of the downloaded camera assets (MobileAsset RawCamera.Camera), each a
        content-hashed directory: on macOS 27 support arrives this way too, and a
        decoder version whose resources are missing falls back to an older one.

    Hashed to 8 hex characters. A change to any of them misses every Apple preview once,
    which is the point: the version a file decodes with is not knowable without opening
    it, but it cannot change unless one of these does.
*/
    static QMutex m;
    static QString cached;
    QMutexLocker lock(&m);
    if (!fingerprintStale.exchange(false) && !cached.isEmpty()) return cached;

    QByteArray d;
    char build[64] = {0};
    size_t len = sizeof(build);
    if (sysctlbyname("kern.osversion", build, &len, nullptr, 0) == 0) d += build;
    @autoreleasepool {
        NSBundle *raw =
            [NSBundle bundleWithPath:@"/System/Library/CoreServices/RawCamera.bundle"];
        NSString *v = [raw objectForInfoDictionaryKey:@"CFBundleVersion"];
        if (v) d += "|raw=" + QByteArray(v.UTF8String);
    }
    static const char *const kAssetRoots[] = {
        "/System/Library/AssetsV2/com_apple_MobileAsset_RawCamera_Camera",
        "/private/var/MobileAsset/AssetsV2/com_apple_MobileAsset_RawCamera_Camera",
    };
    for (const char *root : kAssetRoots) {
        QStringList assets = QDir(QString::fromLatin1(root))
            .entryList({"*.asset"}, QDir::Dirs | QDir::NoDotAndDotDot);
        assets.sort();
        if (!assets.isEmpty()) d += "|assets=" + assets.join(',').toLatin1();
    }
    cached = QString::fromLatin1(
        QCryptographicHash::hash(d, QCryptographicHash::Sha1).toHex().left(8));
    return cached;
}

bool UsesAppleLook(const QString &path, bool &appleLook)
{
    QMutexLocker lock(&chosenMutex);
    const auto it = chosenByPath.constFind(path);
    if (it == chosenByPath.constEnd()) return false;
    appleLook = it->appleLook;
    return true;
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

void RequestVersions(const QString &path)
{
/*
    THE LIST UP FRONT, WHATEVER IS RENDERING. Decode records the versions as a side
    effect, which left the "Render using" list holding a bare "Apple Raw" and a promise
    whenever Winnow was the engine -- the Apple decode never ran, so the user could not
    see what Apple offered for this image without first switching to it.

    Opening the file with CIRAWFilter and asking supportedDecoderVersions is the only
    reliable source (the camera model is not -- see Documentation "Apple decoder
    version") and costs 50-200 ms, so it runs on the global pool; Record posts the same
    queued callback a decode does. One request per path: an image already known, or
    already being asked about, is not opened again. The recorded "used" version is the
    one a decode WOULD use under the current pin; a real decode overwrites it.
*/
    if (path.isEmpty()) return;
    static QSet<QString> inFlight;          // under chosenMutex
    {
        QMutexLocker lock(&chosenMutex);
        if (chosenByPath.contains(path) || inFlight.contains(path)) return;
        inFlight.insert(path);
    }
    QThreadPool::globalInstance()->start([path] {
        if (@available(macOS 12.0, *)) {
            @autoreleasepool {
                NSURL *url = [NSURL fileURLWithPath:path.toNSString()];
                CIRAWFilter *f = [CIRAWFilter filterWithImageURL:url];
                NSArray<NSString *> *supported = f ? f.supportedDecoderVersions : nil;
                if (supported.count)
                    Record(path, supported, PickVersion(supported, VersionPin()),
                           WantsAppleLook(f));
            }
        }
        QMutexLocker lock(&chosenMutex);
        inFlight.remove(path);
    });
}

bool Decode(const QString &path, WorkingImage &out, QString &err, int pin)
{
    if (pin < 0) pin = VersionPin();
    if (@available(macOS 12.0, *)) {
        @autoreleasepool {
            NSURL *url = [NSURL fileURLWithPath:path.toNSString()];
            CIRAWFilter *f = [CIRAWFilter filterWithImageURL:url];
            if (f == nil) {
                err = QStringLiteral("CIRAWFilter could not open the file.");
                return false;
            }

            /* SCENE-LINEAR AND UNSHARPENED, LIKE THE WINNOW ENGINE -- EXCEPT iPHONE
               PRORAW, WHICH KEEPS APPLE'S OWN RENDERING. Apple's noise reduction stays on
               for both. Decided 2026-10-13; ProRAW split 2026-11-09.

               TONE: boostAmount 0 is "no global tone curve, linear response". The default
               (1) is Apple's tone curve, which Develop's OutputTransform then applied a
               second time -- mid-tones nearly doubled (0.062 -> 0.102 on a D850) with
               extra saturation, so Apple rendered brighter and harder than Winnow.
               Verified linear: with boost 0, +1 EV doubles every sampled pixel (2.00)
               on D850, CR3, X-T50 and A1 II files. boostShadowAmount only acts through
               the boost. Local tone map and contrast are forced to 0 (both already
               default to 0 on those files).

               EXTENDED DYNAMIC RANGE ON for the linear decode (2026-11-09). It defaults
               to 0, which CLIPS every decode at 1.0 before exposure (ARW, NEF and ProRAW
               all measured max 1.000), so highlights above white were gone before
               Develop's Highlights could recover them. At 1 the maxima are 3.0 (ARW,
               NEF) with the median unchanged; 2 adds nothing.

               iPHONE PRORAW: APPLE'S RENDERING, NOT A LINEAR DECODE. Recognised by
               Apple's own default (WantsAppleLook: local tone map on by default; 0 on
               ordinary raws). The file carries a +5.16 EV BaselineExposure whose other
               half is a ProfileGainTableMap -- Apple's local tone map. Measured on an
               iPhone 17 Pro DNG (linear luminance, median / 90th pct / share >= 0.95):
                 local tone map forced off            0.663 / 0.989 / 20.1%
                 tone map on, then Standard roll-off  0.795 / 0.921 /  5.7%
                 Apple's own render, EDR 0            0.539 / 0.735 /  0.1%
                 the file's embedded preview          0.521 / 0.730 /  0.1%
               With the tone map on, the "linear" output is already in display range,
               so Winnow's roll-off brightened it a second time. So boost, contrast and
               local tone map stay at Apple's defaults, EDR stays 0 (with the boost on it
               brightens rather than adds headroom: median 0.539 -> 0.624), and the image
               is marked decoderToneMapped so OutputTransform renders it with None. The
               Winnow engine's own ProRAW rendering (embedded profile + gain table map)
               is separate work.

               SHARPENING OFF (sharpness and edge "detail enhancement"): Develop has its
               own Detail sharpening with radius and masking, applied after the user's
               edits; Apple's would be a second, uncontrollable pass, and pre-alignment
               sharpening hurts focus stacking.

               NOISE REDUCTION LEFT AT APPLE'S PER-IMAGE DEFAULTS, on purpose: it is much
               improved in decoder 9 and it is why this engine is worth choosing. PMRID
               ("Denoise raw") needs the CFA mosaic and is Winnow-only, so the two do not
               compete. This reverses the earlier plan of handing PMRID a noisy image. */
            const bool appleLook = WantsAppleLook(f);
            if (!appleLook) {
                f.boostAmount = 0.0;
                f.boostShadowAmount = 0.0;
                if (f.isLocalToneMapSupported) f.localToneMapAmount = 0.0;
                if (f.isContrastSupported)     f.contrastAmount = 0.0;
                f.extendedDynamicRangeAmount = 1.0;
            }
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
            NSString *wanted = PickVersion(supported, pin);
            if (wanted != nil) f.decoderVersion = wanted;

            CIImage *img = f.outputImage;
            if (img == nil && wanted != nil && ![wanted isEqualToString:defaultVersion]) {
                /* Version 9 can need on-demand resources this Mac has not fetched. Decode
                   with Core Image's own choice now and ask for the download, so a later
                   decode gets the wanted version; a decode never waits on the network. */
#if defined(__MAC_OS_X_VERSION_MAX_ALLOWED) && __MAC_OS_X_VERSION_MAX_ALLOWED >= 270000
                if (@available(macOS 27.0, *))
                    [f downloadResourcesWithTimeout:300
                                  completionHandler:^(NSError *e) {
                        if (!e) fingerprintStale.store(true);   // new support landed
                    }];
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
            Record(path, supported, f.decoderVersion, appleLook);

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
            out.decoderToneMapped = appleLook;  // ProRAW: Apple's rendering, see above
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

QList<int> AvailableVersions()
{
    /* The same symbol test as LatestDecoderVersion, for every version: what this macOS
       can decode with at all, newest first. */
    static const QList<int> versions = [] {
        QList<int> v;
        for (int n = 30; n >= 6; --n) {
            const QByteArray sym = "CIRAWDecoderVersion" + QByteArray::number(n);
            if (dlsym(RTLD_DEFAULT, sym.constData()) != nullptr) v << n;
        }
        return v;
    }();
    return versions;
}

QStringList SupportedCameraModels(int version, bool *perVersion)
{
/*
    See the header. Cached per version for the process: each list is ~0.2 s (the
    all-versions one ~0.4 s) and Apple's support does not change under a running
    process except by a download, which the dialog is not worth re-querying for.
*/
    static QMutex m;
    static QHash<int, QStringList> cache;
    QMutexLocker lock(&m);

    bool byVersion = false;
#if defined(__MAC_OS_X_VERSION_MAX_ALLOWED) && __MAC_OS_X_VERSION_MAX_ALLOWED >= 270000
    if (@available(macOS 27.0, *)) byVersion = version > 0;
#endif
    const int key = byVersion ? version : 0;
    if (perVersion) *perVersion = byVersion;
    if (cache.contains(key)) return cache.value(key);

    QStringList models;
    @autoreleasepool {
        NSArray<NSString *> *list = nil;
#if defined(__MAC_OS_X_VERSION_MAX_ALLOWED) && __MAC_OS_X_VERSION_MAX_ALLOWED >= 270000
        if (byVersion) {
            if (@available(macOS 27.0, *))
                list = [CIRAWFilter supportedCameraModelsWithVersion:
                            [NSString stringWithFormat:@"%d", version]];
        }
#endif
        if (!byVersion) {
            if (@available(macOS 12.0, *)) list = CIRAWFilter.supportedCameraModels;
        }
        for (NSString *s in list) models << QString::fromNSString(s);
    }
    models.sort(Qt::CaseInsensitive);
    cache.insert(key, models);
    return models;
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
