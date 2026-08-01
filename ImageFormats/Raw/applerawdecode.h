#ifndef APPLERAWDECODE_H
#define APPLERAWDECODE_H

#include <QString>
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

    Configuration (see applerawdecode.mm): Apple's tone boost and sharpening are DISABLED so the
    output stays scene-linear and unsharpened (sharpening before focus-stack alignment is
    harmful) -- the look comes from OutputTransform, not Core Image. Colour NR and a baseline
    luminance NR are LEFT ON: that is the global NR step. Additional LOCAL luminance NR is a
    separate, portable Develop op (see Develop::Denoise), not done here.
*/
namespace AppleRawDecode {

/* Decode path -> scene-linear WorkingImage (sceneReferred = true). Returns false (with err set)
   on any failure; the caller then falls back to the in-house engine, so a false return is a
   soft failure, never fatal. */
bool Decode(const QString &path, WorkingImage &out, QString &err);

} // namespace AppleRawDecode

#endif // APPLERAWDECODE_H
