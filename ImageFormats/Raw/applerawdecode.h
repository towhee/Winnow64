#ifndef APPLERAWDECODE_H
#define APPLERAWDECODE_H

#include <QString>
#include <QList>
#include <QStringList>
#include <functional>
#include "Develop/workingimage.h"

/*
    macOS Core Image (CIRAWFilter) RAW front-end -- engine A.

    Decodes a RAW file DIRECTLY to the shared scene-linear WorkingImage (Apple's GPU demosaic +
    global colour / baseline luminance NR + as-shot white balance), the alternative to the
    in-house UnpackCfa -> Demosaic -> RawColor path (engine B). Both converge on WorkingImage,
    so everything downstream (Develop, local luminance NR, OutputTransform) is engine-agnostic.

    Selected per image by RawRender::Choice (Develop/rawrender.h). macOS-ONLY: this header is included
    unconditionally so the call site needs no #ifdef, but Decode() is only compiled in
    applerawdecode.mm (added to the build on APPLE). Callers therefore reference it only inside a
    Q_OS_MAC guard; off-mac the in-house engine is used unconditionally.

    Configuration (see applerawdecode.mm): Apple's tone boost, local tone map, contrast,
    sharpening and detail enhancement are OFF, so the output is scene-linear and
    unsharpened like engine B's -- the look comes from OutputTransform, sharpening from
    Develop's Detail section. Apple's luminance and colour noise reduction are LEFT at
    Core Image's per-image defaults: that is the reason to choose this engine (much
    improved in decoder 9).
*/
namespace AppleRawDecode {

/* Decode path -> scene-linear WorkingImage (sceneReferred = true). Returns false (with err set)
   on any failure; the caller then falls back to the in-house engine, so a false return is a
   soft failure, never fatal. pin is the image's version choice (0 = newest; see
   VersionPin); -1 = the default (VersionPin()). */
bool Decode(const QString &path, WorkingImage &out, QString &err, int pin = -1);

/* The newest Core Image RAW decoder version the running macOS offers (9 on macOS 27),
   or 0 if none is found. */
int LatestDecoderVersion();

/* Every decoder version the running macOS offers, newest first ({9, 8, 7, 6} on
   macOS 27). */
QList<int> AvailableVersions();

/* The camera models Core Image lists for a decoder version, sorted, as Apple names them
   ("Nikon D850", which is not the EXIF spelling). Per version only on macOS 27+
   (supportedCameraModelsWithVersion); older systems have one list for all versions,
   and *perVersion says which was returned. Apple's per-model list is a guide, not the
   answer for a file: a file can offer fewer versions (measured: the list puts the
   ILCE-9M2 under 8, an uncompressed A9 II file offers only 7). */
QStringList SupportedCameraModels(int version, bool *perVersion = nullptr);

/* THE DEFAULT VERSION CHOICE, for images that have not chosen their own (the
   renderer is per image since 2026-10-14 -- Develop/rawrender.h; this is
   Preferences' default, like G::decodeRawEngine). 0 is the newest version each file
   supports; N is the newest version <= N the file supports, or the oldest it has if N
   is older than all of them. Persisted as "appleRawDecoderVersion" beside
   "decodeRawEngine". */
int VersionPin();
void SetVersionPin(int pin);

/* What a render key folds in: the pin, or LatestDecoderVersion() for "newest", so an OS
   update that adds a version misses the previews the old one rendered. */
int EffectiveVersionPin();
/* The same for a given pin (an image's own choice). */
int EffectiveVersionPin(int pin);

/* THE STATE OF APPLE'S RAW SUPPORT ON THIS MAC, as a short hash, for the render keys.
   The pin does not say which version a FILE decodes with -- "newest" moves from 8 to 9
   when Apple adds the camera to 9 -- and that is not knowable without opening the file.
   It can only change when Apple's support changes, though: an OS update, a RAW
   compatibility update, or a downloaded camera asset. This changes with any of them. */
QString SupportFingerprint();

/* What Decode chose for path, for the Raw panel: the versions the file supports, newest
   first, as numbers ("9.dng" -> 9), and the one it used. False if Decode has not run on
   path this session. */
bool DecodedVersions(const QString &path, QList<int> &newestFirst, int &used);

/* Called on the GUI thread, queued, after each successful Decode, with its path. Set
   once at start-up. */
void SetDecodedCallback(std::function<void(const QString &path)> cb);

/* Find out, without decoding, which versions path supports: opens it with Core Image
   on a pool thread (50-200 ms), records the list for DecodedVersions and posts the
   decoded callback. No-op if path is already known or being asked about. */
void RequestVersions(const QString &path);

} // namespace AppleRawDecode

#endif // APPLERAWDECODE_H
