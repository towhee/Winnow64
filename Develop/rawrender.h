#ifndef RAWRENDER_H
#define RAWRENDER_H

#include <QString>

struct EditParams;

/*
    WHICH DECODER RENDERS AN IMAGE'S RAW FILE -- per image since 2026-10-14.

    "Render using" (the Raw panel) used to be one global switch, so choosing Apple Raw 9
    on a version changed its master and every other raw too. Now the choice lives in the
    image's recipe (EditParams::rawEngine / appleRawVersion, Global scope), and an image
    that never chose follows the app-wide DEFAULT (G::decodeRawEngine and
    AppleRawDecode::VersionPin, set in Preferences).

    Every site that used to read G::decodeRawEngine asks here instead, with the recipe
    it is rendering:
      o the decode (RawFormat::Decode) -- which engine, which Apple version;
      o WorkingImageCache -- the pre-develop base is per renderer (tag()), so a master
        and its version decoded differently do not hand each other their pixels;
      o the render keys (Metadata::devPreviewKey) -- engineKey() and appleKeyPart()
        reproduce, for an image with no choice of its own, exactly the strings the
        global setting produced, so no existing preview is re-keyed;
      o the Winnow-only features (PMRID "Denoise raw", camera profiles).

    Off macOS Apple is never chosen: a recipe naming it renders with Winnow.
*/
namespace RawRender {

struct Choice
{
    bool apple = false;
    int  applePin = 0;          // with apple: 0 = newest the file supports, N = Apple Raw N

    bool operator==(const Choice &o) const {
        return apple == o.apple && (!apple || applePin == o.applePin);
    }
    bool operator!=(const Choice &o) const { return !(*this == o); }

    /* WorkingImageCache's tag: "w", or "a" + the pin ("a0" = newest). */
    QString tag() const { return apple ? "a" + QString::number(applePin) : "w"; }
    /* For the panel's history row and diagnostics: "Winnow", "Apple Raw (newest)",
       "Apple Raw 9". */
    QString label() const;
};

/* The app-wide default: what an image with no choice of its own renders with. */
Choice defaultChoice();
/* The recipe's Global-scope params -> the renderer, falling back to the default. */
Choice resolve(const EditParams &global);
/* A stored recipe (Metadata::readDevelopSidecar's base64 EditStack) -> the renderer.
   Cheap for the common case: a recipe that never chose is not parsed. */
Choice fromBlob(const QString &blob);
/* Write c into params as an EXPLICIT choice. */
void apply(EditParams &global, const Choice &c);

/* Render-key parts. engineKey is the integer the keys have always carried (the
   G::DecodeRawEngine value); appleKeyPart is "" for Winnow, else the Apple version and
   SupportFingerprint (see Metadata.cpp "THE APPLE DECODER VERSION"). */
int engineKey(const Choice &c);
QString appleKeyPart(const Choice &c);

} // namespace RawRender

#endif // RAWRENDER_H
