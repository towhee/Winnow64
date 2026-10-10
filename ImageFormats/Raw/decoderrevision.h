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
*/
inline int revision(const QString &ext)
{
    const QString e = ext.toLower();
    if (e == "cr2" || e == "cr3") return 1;
    return 0;
}

} // namespace RawDecoder

#endif // DECODERREVISION_H
