#ifndef STALETHUMB_H
#define STALETHUMB_H

#include <QByteArray>
#include <QFile>
#include <QSize>
#include <QString>

/*
    A STALE EMBEDDED THUMBNAIL is one that no longer shows the image it is embedded in.
    The case that matters: older lossless-rotate tools turned a JPEG's pixels to
    portrait and reset Orientation to 1, but left the EXIF IFD1 thumbnail (and any
    makernote preview) landscape. The grid showed the thumb on its side while the
    loupe, which decodes the real pixels, was upright.

    Which way the pixels were turned is not recorded anywhere, so a stale thumb cannot
    be rotated back into agreement. It is detected by SHAPE -- portrait against
    landscape -- and replaced with one made from the image itself.

    JPEG ONLY. There the dimensions in the metadata are the SOF's: the very pixels the
    loupe decodes. A raw's sensor size and preview size come from different places
    and are not compared.

    Header-only and free of widgets so tests/unit/tst_stalethumb.cpp can include it.
    Used by Thumb::loadThumb (the embedded thumb), Reader::readIcon and
    MW::prefetchIconsFromIndex (a thumb already in ThumbCache), and
    MW::repairStaleThumbs. See "Stale JPEG thumbnails" in notes/Documentation.md.
*/
namespace StaleThumb {

inline bool isChecked(const QString &ext)
{
    const QString e = ext.toLower();
    // the same set the catalog query and schema 24 use (Catalog::liveJpegPaths)
    return e == "jpg" || e == "jpeg" || e == "jpe";
}

/*  Portrait against landscape. A square on either side has no shape to disagree with,
    and an unknown size (0) is never evidence. */
inline bool shapesDisagree(int aW, int aH, int bW, int bH)
{
    if (aW <= 0 || aH <= 0 || bW <= 0 || bH <= 0) return false;
    if (aW == aH || bW == bH) return false;
    return (aW > aH) != (bW > bH);
}

/*  The turn Thumb::checkOrientation gives an icon, mirrored exactly: only orientations
    3, 6 and 8 turn it, and rotationDegrees is added only to those. */
inline int iconTurnDegrees(int orientation, int rotationDegrees)
{
    int base;
    switch (orientation) {
    case 3: base = 180; break;
    case 6: base = 90;  break;
    case 8: base = 270; break;
    default: return 0;
    }
    return ((base + rotationDegrees) % 360 + 360) % 360;
}

/*  The embedded thumb as decoded, BEFORE checkOrientation, against the image's stored
    (SOF) dimensions: both are in stored-pixel space. */
inline bool embeddedIsStale(QSize embedded, int imgW, int imgH)
{
    return shapesDisagree(embedded.width(), embedded.height(), imgW, imgH);
}

/*  A thumb out of ThumbCache, which was stored AFTER checkOrientation, against the
    image's stored dimensions turned the same way. */
inline bool cachedIsStale(QSize cached, int imgW, int imgH,
                          int orientation, int rotationDegrees)
{
    const int turn = iconTurnDegrees(orientation, rotationDegrees);
    if (turn == 90 || turn == 270) return shapesDisagree(cached.width(), cached.height(),
                                                         imgH, imgW);
    return shapesDisagree(cached.width(), cached.height(), imgW, imgH);
}

/*  THE SIZE IN A JPEG FRAME HEADER (any SOFn) of the JPEG stream starting at `start`
    in f, or an invalid QSize. A seek from marker to marker, a few bytes each, whatever
    size the APPn segments are. */
inline QSize frameSizeAt(QIODevice &f, qint64 start)
{
    auto u16 = [](const QByteArray &b, int i) {
        return (static_cast<unsigned char>(b.at(i)) << 8)
               | static_cast<unsigned char>(b.at(i + 1));
    };
    if (start < 0 || !f.seek(start)) return QSize();
    QByteArray b = f.read(2);
    if (b.size() < 2 || u16(b, 0) != 0xFFD8) return QSize();
    for (int guard = 0; guard < 512; ++guard) {
        b = f.read(2);
        if (b.size() < 2 || static_cast<unsigned char>(b.at(0)) != 0xFF) return QSize();
        const int marker = static_cast<unsigned char>(b.at(1));
        if (marker == 0xFF) { f.seek(f.pos() - 1); continue; }     // fill byte
        if (marker == 0x01 || (marker >= 0xD0 && marker <= 0xD7)) continue;
        if (marker == 0xDA || marker == 0xD9) return QSize();      // no frame before scan
        b = f.read(7);                       // length, precision, height, width
        if (b.size() < 7) return QSize();
        const bool isSof = marker >= 0xC0 && marker <= 0xCF
                           && marker != 0xC4 && marker != 0xC8 && marker != 0xCC;
        if (isSof) return QSize(u16(b, 5), u16(b, 3));
        if (!f.seek(f.pos() - 7 + u16(b, 0))) return QSize();
    }
    return QSize();
}

/*  THE PIXELS' SIZE, FROM THE FILE. The confirmation every detection goes through.

    m.width/m.height cannot be trusted alone. Until schema 24's re-read reaches a row,
    the catalog holds the size Jpeg::parse used to take from the Exif PixelX/YDimension
    tags, and 1,241 of the user's JPEGs carry those tags landscape on portrait pixels
    WITH a correct portrait thumbnail -- each one a false "stale" against the catalog.
    The first build reported ~100 of them for repair. Thumb::loadThumb therefore
    judges an embedded thumb against this, and Reader::readIcon uses it to confirm a
    cache-hit candidate. */
inline QSize jpegFrameSize(const QString &fPath)
{
    QFile f(fPath);
    if (!f.open(QIODevice::ReadOnly)) return QSize();
    return frameSizeAt(f, 0);
}

/*  WHERE THE EXIF IFD1 THUMBNAIL STARTS in a JPEG file (absolute offset), or -1.
    Self-contained on purpose -- the catalog does not store decode geometry, and a
    check of 60,000 catalogued files must not run the full metadata parser. Walks to
    APP1 "Exif", then the TIFF header, IFD0's entry count to the next-IFD link, and
    IFD1's JPEGInterchangeFormat (513). */
inline qint64 exifThumbOffset(QIODevice &f)
{
    auto rd = [&](qint64 at, int n) { f.seek(at); return f.read(n); };
    auto be16 = [](const QByteArray &b, int i) {
        return (static_cast<unsigned char>(b.at(i)) << 8)
               | static_cast<unsigned char>(b.at(i + 1));
    };
    // find APP1 "Exif\0\0"
    qint64 pos = 2;
    qint64 tiff = -1;
    for (int guard = 0; guard < 64; ++guard) {
        const QByteArray m = rd(pos, 4);
        if (m.size() < 4 || static_cast<unsigned char>(m.at(0)) != 0xFF) return -1;
        const int marker = static_cast<unsigned char>(m.at(1));
        if (marker == 0xDA || (marker >= 0xC0 && marker <= 0xCF && marker != 0xC4
                               && marker != 0xC8 && marker != 0xCC))
            return -1;                       // past where Exif may be
        const int len = be16(m, 2);
        if (marker == 0xE1 && rd(pos + 4, 6) == QByteArray("Exif\0\0", 6)) {
            tiff = pos + 10;
            break;
        }
        pos += 2 + len;
    }
    if (tiff < 0) return -1;

    const QByteArray hdr = rd(tiff, 8);
    if (hdr.size() < 8) return -1;
    const bool le = hdr.startsWith("II");
    if (!le && !hdr.startsWith("MM")) return -1;
    auto u16 = [&](const QByteArray &b, int i) -> quint32 {
        const quint32 x = static_cast<unsigned char>(b.at(i));
        const quint32 y = static_cast<unsigned char>(b.at(i + 1));
        return le ? (y << 8 | x) : (x << 8 | y);
    };
    auto u32 = [&](const QByteArray &b, int i) -> quint32 {
        return le ? (u16(b, i + 2) << 16 | u16(b, i)) : (u16(b, i) << 16 | u16(b, i + 2));
    };
    const quint32 ifd0 = u32(hdr, 4);
    QByteArray b = rd(tiff + ifd0, 2);
    if (b.size() < 2) return -1;
    const quint32 n0 = u16(b, 0);
    b = rd(tiff + ifd0 + 2 + 12 * qint64(n0), 4);
    if (b.size() < 4) return -1;
    const quint32 ifd1 = u32(b, 0);
    if (!ifd1) return -1;
    b = rd(tiff + ifd1, 2);
    if (b.size() < 2) return -1;
    const quint32 n1 = u16(b, 0);
    if (n1 > 200) return -1;
    b = rd(tiff + ifd1 + 2, int(12 * n1));
    if (b.size() < int(12 * n1)) return -1;
    for (quint32 k = 0; k < n1; ++k) {
        const int e = int(12 * k);
        if (u16(b, e) == 513) return tiff + u32(b, e + 8);
    }
    return -1;
}

/*  THE WHOLE TEST ON A FILE, headers only: is its EXIF thumbnail the wrong shape for
    its pixels? For a JPEG that is not the image just loaded -- a catalog scan
    (CatalogScanner) or the whole-catalog check (MW::checkCatalogStaleThumbs). False
    when anything is missing or unreadable: never stale on a guess. */
inline bool fileIsStale(const QString &fPath)
{
    QFile f(fPath);
    if (!f.open(QIODevice::ReadOnly)) return false;
    const QSize frame = frameSizeAt(f, 0);
    if (!frame.isValid()) return false;
    const QSize thumb = frameSizeAt(f, exifThumbOffset(f));
    return embeddedIsStale(thumb, frame.width(), frame.height());
}

} // namespace StaleThumb

#endif // STALETHUMB_H
