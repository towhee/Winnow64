// Compiled only on macOS (added to SOURCES in the macx qmake scope).
//
// Fast image / thumbnail decode via Apple's ImageIO framework.
// EXIF orientation is NOT applied here — callers (Thumb, ImageDecoder)
// apply rotation afterwards using metadata-driven logic.

#include <QImage>
#include <QString>

#include <ImageIO/ImageIO.h>
#include <CoreGraphics/CoreGraphics.h>
#include <CoreFoundation/CoreFoundation.h>

namespace {

CFURLRef makeFileUrl(const QString &fPath)
{
    CFStringRef cfPath = CFStringCreateWithCString(
        kCFAllocatorDefault,
        fPath.toUtf8().constData(),
        kCFStringEncodingUTF8);
    if (!cfPath) return nullptr;

    CFURLRef url = CFURLCreateWithFileSystemPath(
        kCFAllocatorDefault, cfPath, kCFURLPOSIXPathStyle, false);
    CFRelease(cfPath);
    return url;
}

// Render a CGImage into a QImage-owned ARGB32_Premultiplied buffer via
// CoreGraphics. Releases cg. Returns true on success.
bool renderCGImageToQImage(CGImageRef cg, QImage &out)
{
    if (!cg) return false;

    const size_t w = CGImageGetWidth(cg);
    const size_t h = CGImageGetHeight(cg);
    if (w == 0 || h == 0) {
        CGImageRelease(cg);
        return false;
    }

    QImage img(static_cast<int>(w), static_cast<int>(h),
               QImage::Format_ARGB32_Premultiplied);
    if (img.isNull()) {
        CGImageRelease(cg);
        return false;
    }
    img.fill(Qt::transparent);

    CGColorSpaceRef cs = CGColorSpaceCreateDeviceRGB();
    if (!cs) {
        CGImageRelease(cg);
        return false;
    }

    // Premultiplied BGRA (little-endian) matches Qt's
    // Format_ARGB32_Premultiplied byte layout on macOS.
    CGContextRef ctx = CGBitmapContextCreate(
        img.bits(),
        static_cast<size_t>(img.width()),
        static_cast<size_t>(img.height()),
        8,
        static_cast<size_t>(img.bytesPerLine()),
        cs,
        kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Little);
    CGColorSpaceRelease(cs);

    if (!ctx) {
        CGImageRelease(cg);
        return false;
    }

    CGContextSetBlendMode(ctx, kCGBlendModeCopy);
    CGContextDrawImage(ctx, CGRectMake(0, 0, w, h), cg);
    CGContextRelease(ctx);
    CGImageRelease(cg);

    out = img;
    return !out.isNull();
}

} // anonymous namespace

/*
    Decode an embedded (or downscaled) thumbnail.

    ImageIO returns the embedded thumbnail if present, or decodes the
    primary image at the requested max longest-edge size — both far
    cheaper than QImage::load() of the full-resolution image.
*/
bool macImageIOThumbnail(const QString &fPath, int maxPixelSize, QImage &out)
{
    if (maxPixelSize <= 0) return false;

    CFURLRef url = makeFileUrl(fPath);
    if (!url) return false;

    CGImageSourceRef src = CGImageSourceCreateWithURL(url, nullptr);
    CFRelease(url);
    if (!src) return false;

    CFNumberRef maxSize = CFNumberCreate(
        kCFAllocatorDefault, kCFNumberIntType, &maxPixelSize);

    const void *keys[] = {
        kCGImageSourceCreateThumbnailFromImageIfAbsent,
        kCGImageSourceCreateThumbnailWithTransform,
        kCGImageSourceShouldCacheImmediately,
        kCGImageSourceThumbnailMaxPixelSize
    };
    const void *vals[] = {
        kCFBooleanTrue,     // create thumbnail from image if no embedded thumb
        kCFBooleanFalse,    // do NOT apply EXIF transform — caller does it
        kCFBooleanTrue,     // cache decoded data immediately
        maxSize             // longest-edge cap in pixels
    };
    CFDictionaryRef opts = CFDictionaryCreate(
        kCFAllocatorDefault, keys, vals, 4,
        &kCFTypeDictionaryKeyCallBacks,
        &kCFTypeDictionaryValueCallBacks);

    CGImageRef cg = CGImageSourceCreateThumbnailAtIndex(src, 0, opts);

    CFRelease(opts);
    CFRelease(maxSize);
    CFRelease(src);

    return renderCGImageToQImage(cg, out);
}

/*
    Decode the full-resolution primary image.

    Used by ImageDecoder for the full image cache. Faster than
    QImage::load() for HEIC because it goes through Apple's hardware-
    accelerated HEVC decoder directly, without round-tripping through
    Qt's image plugin layer.
*/
bool macImageIOImage(const QString &fPath, QImage &out)
{
    CFURLRef url = makeFileUrl(fPath);
    if (!url) return false;

    CGImageSourceRef src = CGImageSourceCreateWithURL(url, nullptr);
    CFRelease(url);
    if (!src) return false;

    const void *keys[] = {
        kCGImageSourceShouldCacheImmediately,
        kCGImageSourceShouldAllowFloat
    };
    const void *vals[] = {
        kCFBooleanTrue,
        kCFBooleanFalse
    };
    CFDictionaryRef opts = CFDictionaryCreate(
        kCFAllocatorDefault, keys, vals, 2,
        &kCFTypeDictionaryKeyCallBacks,
        &kCFTypeDictionaryValueCallBacks);

    CGImageRef cg = CGImageSourceCreateImageAtIndex(src, 0, opts);

    CFRelease(opts);
    CFRelease(src);

    return renderCGImageToQImage(cg, out);
}
