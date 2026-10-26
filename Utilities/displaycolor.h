#ifndef DISPLAYCOLOR_H
#define DISPLAYCOLOR_H

#include <QImage>

/*
    The sRGB -> display conversion for pixels Winnow renders ITSELF and puts on screen
    (the Develop loupe, the Develop placeholder, the Detail 1:1 patch).

    Every rendered image is sRGB (OutputTransform), and on a wide-gamut display sRGB
    numbers shown unconverted come out oversaturated -- reds most visibly. The Develop
    loupe was shown that way while Browse converted (ImageDecoder::colorManage), so the
    same picture changed colour on the first slider tick, and a face measured ~25% redder
    than Lightroom's with the same profile (see "15.2 Display Colour Management of
    Rendered Frames" in notes/Documentation.md).

    WHY AT THE DISPLAY BOUNDARY and not by rendering into the display's space: the
    rendered frame is also the developed preview (sRGB JPEG), the scopes' input and the
    sharpening preview's source. Those stay sRGB; only the copy handed to the screen is
    converted.

    Same lcms engine and intent as ICC::transform, so Browse and Develop agree. The
    transforms are built ONCE per display (Rebuild) rather than per image, and applied
    in parallel strips: ~3-5 ms for a 6 MP proxy on 16 threads (measured 2026-10-26).

    Inactive -- ToDisplay returns its input untouched, no copy -- when colour management
    is off, no display profile is known (Linux has none), or the display is effectively
    sRGB.
*/
namespace DisplayColor
{
    /* GUI thread: re-read the display profile and G::colorManage. Call after
       ICC::setOutProfile and when colour management is toggled. */
    void Rebuild();

    /* Thread-safe. srgb converted for the current display, or srgb itself. */
    QImage ToDisplay(const QImage &srgb);

    bool IsActive();

    /* The display profile is not effectively sRGB (a wide-gamut panel), whether or not
       colour management is on. With it off, everything on screen is oversaturated --
       the Develop dock's alert row says so (MW::updateDevelopSelectionWarning). */
    bool DisplayDiffersFromSrgb();
}

#endif // DISPLAYCOLOR_H
