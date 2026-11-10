#ifndef DEVELOPREVISION_H
#define DEVELOPREVISION_H

#include <QString>

/*
    Revision of the develop MATHS, folded into every devPreview / developed thumbnail
    key at the point of use (Metadata::renderKey), exactly as RawDecoder::revision
    (ImageFormats/Raw/decoderrevision.h) is for a sensor decoder.

    The schema version (Develop/developschema.h) and this number are separate on
    purpose. The schema version says what the stored VALUES mean; this says what pixels
    the pipeline makes from them. A renamed key bumps the schema but not this (same
    pixels, no rebuild). A retuned op bumps this, and usually the schema too, with a
    step that converts old values to keep their look.

      kEdited    BUMP when an op's pixels change for a recipe that uses it. Edited
                 images' previews and sidecar thumbnails miss and are rebuilt lazily.
      kIdentity  BUMP only when the render of an UNEDITED image changes (identity
                 params, e.g. a new default or a changed base tone). Every default-
                 render preview is rebuilt, so do not bump it casually.

    0 = never bumped: no key part, so every key that existed before this header is
    unchanged.

    History:
      kEdited 1  2026-11-09  a profile's ProfileToneCurve hands over to the roll-off at
                             0.80, not 0.60 (CameraCurve::kProRawKneeY, fitted to
                             Lightroom). Recipes with Tone mapping = Profile curve.
                             Unedited ProRAW renders are covered by dng revision 3.
      kEdited 2  2026-11-09  ProRAW's embedded profile curve applied per channel, not
                             hue-preserving (OutputLook::curvePerChannel). dng rev 4.
      kEdited 3  2026-11-09  ...and its out-of-gamut colours GamutClip'd, not bled to
                             white (the IMG_1106 pumpkin halo). dng rev 5.
      kEdited 4  2026-11-09  a DCP's ProfileToneCurve back to the 0.60 knee (kEdited 1
                             moved it to 0.80; only ProRAW's curve keeps 0.80).
*/

namespace DevelopRevision {

constexpr int kEdited   = 4;
constexpr int kIdentity = 0;

/* The key part for a base key: identity renders carry the "R" prefix
   (Metadata::defaultRenderKey), recipes do not. Empty when the relevant revision is 0. */
inline QString keyPart(const QString &baseKey)
{
    const int rev = baseKey.startsWith('R') ? kIdentity : kEdited;
    return rev == 0 ? QString() : QString("|dev=%1").arg(rev);
}

} // namespace DevelopRevision

#endif // DEVELOPREVISION_H
