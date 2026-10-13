#ifndef APPLERAWDECODE_H
#define APPLERAWDECODE_H

#include <QString>
#include <QList>
#include <functional>
#include "Develop/workingimage.h"

/*
    macOS Core Image (CIRAWFilter) RAW front-end -- engine A.

    Decodes a RAW file DIRECTLY to the shared scene-linear WorkingImage (Apple's GPU demosaic +
    global colour / baseline luminance NR + as-shot white balance), the alternative to the
    in-house UnpackCfa -> Demosaic -> RawColor path (engine B). Both converge on WorkingImage,
    so everything downstream (Develop, local luminance NR, OutputTransform) is engine-agnostic.

    Selected by G::decodeRawEngine == appleDecodeRawEngine. macOS-ONLY: this header is included
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
   soft failure, never fatal. */
bool Decode(const QString &path, WorkingImage &out, QString &err);

/* The newest Core Image RAW decoder version the running macOS offers (9 on macOS 27),
   or 0 if none is found. */
int LatestDecoderVersion();

/* THE USER'S VERSION CHOICE, global like the engine (Raw panel "Apple Raw N"). 0, the
   default, is the newest version each file supports; N is the newest version <= N the
   file supports, or the oldest it has if N is older than all of them. Persisted as
   "appleRawDecoderVersion" beside "decodeRawEngine". Changing it must clear the
   WorkingImageCache, as an engine switch does. */
int VersionPin();
void SetVersionPin(int pin);

/* What a render key folds in: the pin, or LatestDecoderVersion() for "newest", so an OS
   update that adds a version misses the previews the old one rendered. */
int EffectiveVersionPin();

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
