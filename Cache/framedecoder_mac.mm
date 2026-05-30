// Compiled only on macOS (added to SOURCES in the macx qmake scope).
//
// Fast video thumbnail decode via AVFoundation's AVAssetImageGenerator.
// Replaces the QMediaPlayer + QVideoSink pipeline in FrameDecoder for
// videos AVFoundation can open. Synchronous: returns a QImage and the
// asset duration in milliseconds.

#include <QImage>
#include <QString>

#import <Foundation/Foundation.h>
#import <AVFoundation/AVFoundation.h>
#import <CoreMedia/CoreMedia.h>
#include <CoreGraphics/CoreGraphics.h>

namespace {

bool renderCGImageToQImage(CGImageRef cg, QImage &out)
{
    if (!cg) return false;

    const size_t w = CGImageGetWidth(cg);
    const size_t h = CGImageGetHeight(cg);
    if (w == 0 || h == 0) return false;

    QImage img(static_cast<int>(w), static_cast<int>(h),
               QImage::Format_ARGB32_Premultiplied);
    if (img.isNull()) return false;
    img.fill(Qt::transparent);

    CGColorSpaceRef cs = CGColorSpaceCreateDeviceRGB();
    if (!cs) return false;

    CGContextRef ctx = CGBitmapContextCreate(
        img.bits(),
        static_cast<size_t>(img.width()),
        static_cast<size_t>(img.height()),
        8,
        static_cast<size_t>(img.bytesPerLine()),
        cs,
        kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Little);
    CGColorSpaceRelease(cs);

    if (!ctx) return false;

    CGContextSetBlendMode(ctx, kCGBlendModeCopy);
    CGContextDrawImage(ctx, CGRectMake(0, 0, w, h), cg);
    CGContextRelease(ctx);

    out = img;
    return !out.isNull();
}

} // anonymous namespace

/*
    Decode a thumbnail from a video file using AVAssetImageGenerator.

    Returns the first representable frame (time = 0 with infinite tolerance,
    so the decoder picks the nearest keyframe — orders of magnitude faster
    than seeking to an exact time). appliesPreferredTrackTransform applies
    the asset's display transform during decode, so the caller does not need
    to rotate the result.

    On success, `out` is an ARGB32_Premultiplied QImage scaled so its longest
    edge is `longSide`, and `outDurationMs` is the asset duration in
    milliseconds (0 if unknown).

    Returns false if the asset cannot be opened, has no video track, or the
    image generator fails — caller should fall back to FrameDecoder's
    QMediaPlayer pipeline in that case.
*/
bool macAVFoundationVideoThumbnail(const QString &fPath, int longSide,
                                   QImage &out, qint64 &outDurationMs)
{
    if (longSide <= 0) return false;

    @autoreleasepool {
        NSString *path = [NSString stringWithUTF8String:fPath.toUtf8().constData()];
        if (!path) return false;

        NSURL *url = [NSURL fileURLWithPath:path];
        if (!url) return false;

        AVURLAsset *asset = [AVURLAsset URLAssetWithURL:url options:nil];
        if (!asset) return false;

        // Reject if no video tracks — saves a doomed generator round-trip
        // and matches FrameDecoder's "invalid frame" failure path.
        if ([[asset tracksWithMediaType:AVMediaTypeVideo] count] == 0) {
            return false;
        }

        AVAssetImageGenerator *gen =
            [[AVAssetImageGenerator alloc] initWithAsset:asset];
        gen.appliesPreferredTrackTransform = YES;
        gen.maximumSize = CGSizeMake(longSide, longSide);
        // Any nearby keyframe is fine for a thumb — exact-time seeking forces
        // a decode from the previous keyframe and is much slower.
        gen.requestedTimeToleranceBefore = kCMTimePositiveInfinity;
        gen.requestedTimeToleranceAfter  = kCMTimePositiveInfinity;

        CMTime requested = CMTimeMake(0, 600);
        CMTime actual = kCMTimeZero;
        NSError *err = nil;
        CGImageRef cg = [gen copyCGImageAtTime:requested
                                     actualTime:&actual
                                          error:&err];

        // Capture duration before releasing the generator/asset.
        const CMTime dur = asset.duration;
        outDurationMs = CMTIME_IS_NUMERIC(dur)
                            ? static_cast<qint64>(CMTimeGetSeconds(dur) * 1000.0)
                            : 0;

        [gen release];

        if (!cg) {
            return false;
        }

        const bool ok = renderCGImageToQImage(cg, out);
        CGImageRelease(cg);
        return ok;
    }
}
