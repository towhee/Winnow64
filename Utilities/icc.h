/*
    WHEN COPY NEW C CODE FROM LCMS2 AT https://github.com/mm2/Little-CMS

    1. Copy .c .h files from src and include to Lcms2 in Winnow64

    2. In lcms2.h uncomment as per:

        // Uncomment this to remove the "register" storage class
        // #define CMS_NO_REGISTER_KEYWORD 1

*/

#ifndef ICC_H
#define ICC_H

#include <QtWidgets>
#include "Lcms2/lcms2.h"                // little color management system

/*
    Colour management of decoded images for the screen: source profile (the file's
    embedded ICC, empty = sRGB) -> the display profile (setOutProfile).

    Built for riffling, where every decoded image passes through here (2026-10-26, see
    "15.3 The Display Transform Cache and Fast Kernel" in notes/Documentation.md):

      - transforms are CACHED per source profile, not created per image (~3 ms each);
      - a source the display already matches (an sRGB file on an sRGB display) is a no-op;
      - a matrix/TRC pair -- every display and camera profile measured -- runs a folded
        lookup kernel, ~2.3x faster than lcms and within one level of it; anything else
        (CLUT profiles) runs the cached lcms transform;
      - both run in parallel strips.
*/
namespace ICC
{
    extern bool setOutProfile();
    /* image must be 32 bits per pixel (BGRA); anything else is converted first. */
    extern void transform(const QByteArray &buf, QImage &image);
    extern void err(cmsContext contextID, cmsUInt32Number errorCode, const char *text);

    /* For tests and diagnostics: how transform() would treat this source profile. */
    enum class Path { None, Identity, Fast, Lcms };
    extern Path pathFor(const QByteArray &buf);
}

#endif // ICC_H
