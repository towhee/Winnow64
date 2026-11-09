#ifndef DECODERREVISION_H
#define DECODERREVISION_H

#include <QString>

namespace RawDecoder {

/*
    Per-format revision of the sensor decoder, folded into every devPreview / developed
    thumbnail key (Metadata::renderKey). BUMP A FORMAT'S NUMBER when a decoder fix
    changes its pixels -- black level, white level, as-shot WB, matrix, crop -- so the
    previews and sidecar thumbnails rendered by the old decoder MISS and are rebuilt,
    while every other format keeps its keys and its cache.

    0 means "never bumped" and leaves the key exactly as it was before this table existed,
    so adding a format here costs nothing until it is bumped. Header-only so Metadata (and
    the unit tests that link it) need not pull in rawformat.cpp.

    History:
      cr2 1, cr3 1  2026-10-10  masked-border black level averaged lit columns
                                (7D Mark II black 2130 vs 2047): green cast. See
                                canon.cpp UnpackCfa.
      dng 1         2026-11-09  AnalogBalance folded into the matrix (iPhone ProRAW
                                rendered off-white); the file's own BaselineExposure
                                and ProfileGainTableMap applied. See dng.cpp RenderInfo.
      dng 2         2026-11-09  AsShotNeutral (1, 1, 1) honoured, not read as "none"
                                (ProRAW rendered at D65: yellow indoors). See
                                RawSensorInfo::asShotStated.
      dng 3         2026-11-09  not a decoder change: an unedited ProRAW renders with
                                its profile curve (Auto), whose knee moved to 0.80
                                (CameraCurve::kProfileKneeY). Bumped here, not
                                DevelopRevision::kIdentity, to rebuild only DNGs.
      dng 4         2026-11-09  likewise: ProRAW's embedded curve now per channel
                                (OutputLook::curvePerChannel).
      dng 5         2026-11-09  likewise: GamutClip in place of HighlightRolloff for it.
*/
inline int revision(const QString &ext)
{
    const QString e = ext.toLower();
    if (e == "cr2" || e == "cr3") return 1;
    if (e == "dng") return 5;
    return 0;
}

/*
    Revision of the raw DENOISER (PMRID: its noise model, tiling, blend), folded into the
    devPreview / developed-thumbnail key only for renders that run it -- the Winnow
    engine with the denoise wanted (Metadata::denoiserKeyPart). BUMP when a PMRID change
    alters its output, so previews built by the old denoiser miss and are rebuilt while
    Apple-engine and un-denoised previews keep their keys. 0 = never bumped (no key part).

    History:
      1  2026-10-16  blind noise estimate fitted k ~3000x too high on some uncalibrated
                     bodies (LX3 at ISO 80): magenta render. See pmrid.cpp estimateKB.
*/
inline constexpr int kDenoiserRevision = 1;

} // namespace RawDecoder

#endif // DECODERREVISION_H
