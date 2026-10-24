#ifndef STALETHUMB_H
#define STALETHUMB_H

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
    return e == "jpg" || e == "jpeg";
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

} // namespace StaleThumb

#endif // STALETHUMB_H
