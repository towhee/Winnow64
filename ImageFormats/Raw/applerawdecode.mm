#include "ImageFormats/Raw/applerawdecode.h"

#include <QtGlobal>
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
    we copy all pixels into the WorkingImage before that, so no manual retain is needed. The one
    Core Foundation object we own outright is the CGColorSpaceRef (Create rule) -- released
    explicitly below.
*/
namespace AppleRawDecode {

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

            /* Disable Apple's tone boost and sharpening so the result is scene-linear and
               unsharpened: the tone curve is OutputTransform's job, and pre-alignment
               sharpening hurts focus stacking. Colour NR + baseline luminance NR are LEFT at
               their defaults -- that is the global NR step. */
            f.boostAmount       = 0.0;   // no global tone boost -> keep linear headroom
            f.boostShadowAmount = 0.0;
            f.sharpnessAmount   = 0.0;
            f.detailAmount      = 0.0;

            /* Output SENSOR orientation -- do NOT bake in EXIF orientation. Raw formats are in
               metadata->rotateFormats, so the shared pipeline (ImageDecoder::rotate) applies
               orientation to the QImage AFTER decode, exactly as it does for engine B. By
               default CIRAWFilter orients its output from the file's metadata; left on, that
               pre-rotated image is then rotated a SECOND time downstream. Forcing "up" (identity)
               makes engine A hand off un-oriented pixels, matching engine B's contract. */
            f.orientation = kCGImagePropertyOrientationUp;

            /* Decoder version is unpinned -- Core Image uses its default (the newest version
               supported for this file on the running macOS), so it can drift across OS updates.
               Log what it actually selected. */
            NSLog(@"CIRAWFilter %@ supportedDecoderVersions=%@ active=%@",
                  path.toNSString(), f.supportedDecoderVersions, f.decoderVersion);

            CIImage *img = f.outputImage;
            if (img == nil) {
                err = QStringLiteral("CIRAWFilter produced no output image.");
                return false;
            }

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

            /* Render through an EXTENDED linear sRGB / D65 working+output space so values keep
               scene-linear semantics and highlight headroom (>1.0) survives, into 32-bit float
               RGBA. This matches WorkingImage's working space (linear sRGB primaries / D65). */
            CGColorSpaceRef cs = CGColorSpaceCreateWithName(kCGColorSpaceExtendedLinearSRGB);
            if (cs == nullptr) {
                err = QStringLiteral("Could not create extended linear sRGB colour space.");
                return false;
            }
            CIContext *ctx = [CIContext contextWithOptions:@{
                kCIContextWorkingColorSpace : (id)cs,
                kCIContextOutputColorSpace  : (id)cs,
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
             colorSpace:cs];

            CGColorSpaceRelease(cs);

            /* Deinterleave RGBA float -> WorkingImage's interleaved RGB float (drop alpha). */
            out.width  = W;
            out.height = H;
            out.white  = 1.0f;
            out.sceneReferred = true;   // sensor data: scene-referred linear with headroom
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

} // namespace AppleRawDecode

#endif // Q_OS_MAC
