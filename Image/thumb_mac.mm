// Compiled only on macOS (added to SOURCES in the macx qmake scope).

#include <QImage>
#include <QString>

#include <ImageIO/ImageIO.h>
#include <CoreGraphics/CoreGraphics.h>
#include <CoreFoundation/CoreFoundation.h>

/*
    Fast thumbnail decode via Apple's ImageIO framework.

    For HEIC (and most other formats) ImageIO will return the embedded
    thumbnail if present, or decode at the requested size — both far
    cheaper than QImage::load() of the full-resolution image.

    Returns true on success. The returned QImage has its longest edge
    <= maxPixelSize. EXIF orientation is NOT applied here — Thumb's
    checkOrientation() rotates afterwards using metadata.
*/
bool macImageIOThumbnail(const QString &fPath, int maxPixelSize, QImage &out)
{
    if (maxPixelSize <= 0) return false;

    // Build a CFURL for the file path.
    CFStringRef cfPath = CFStringCreateWithCString(
        kCFAllocatorDefault,
        fPath.toUtf8().constData(),
        kCFStringEncodingUTF8);
    if (!cfPath) return false;

    CFURLRef url = CFURLCreateWithFileSystemPath(
        kCFAllocatorDefault, cfPath, kCFURLPOSIXPathStyle, false);
    CFRelease(cfPath);
    if (!url) return false;

    CGImageSourceRef src = CGImageSourceCreateWithURL(url, nullptr);
    CFRelease(url);
    if (!src) return false;

    // Options dictionary: prefer embedded thumb, decode if missing,
    // apply EXIF transform, cap longest edge at maxPixelSize.
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
        kCFBooleanFalse,    // do NOT apply EXIF transform — Thumb does it
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

    if (!cg) return false;

    const size_t w = CGImageGetWidth(cg);
    const size_t h = CGImageGetHeight(cg);
    if (w == 0 || h == 0) {
        CGImageRelease(cg);
        return false;
    }

    // Render into a QImage-owned ARGB32 buffer via CoreGraphics.
    // Use premultiplied BGRA (little-endian) which matches Qt's
    // Format_ARGB32_Premultiplied byte layout on macOS.
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
