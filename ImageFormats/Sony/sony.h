#ifndef SONY_H
#define SONY_H

#include <QtWidgets>
#include "Utilities/utilities.h"
#include "Metadata/imagemetadata.h"
#include "Metadata/exif.h"
#include "Metadata/ifd.h"
#include "Metadata/xmp.h"
#include "Metadata/gps.h"
#include "ImageFormats/Jpeg/jpeg.h"
#include "ImageFormats/Raw/rawformat.h"

class Sony : public QObject
{
    Q_OBJECT

public:
    Sony();
    bool parse(MetadataParameters &p,
               ImageMetadata &m,
               IFD *ifd,
               Exif *exif,
               Jpeg *jpeg,
               GPS *gps);
private:
    QHash<quint32, QString> sonyMakerHash;
    QHash<int, int> sonyCypherHash;
    Utilities u;
};

/*
    Sensor unpack for Sony ARW (the per-format override of RawFormat). Kept separate from the
    Sony metadata parser above: different lifetime (one per decode, on a decoder thread) and
    no shared state. See notes/Documentation.txt "RAW DECODING".

    SCOPE: UNCOMPRESSED ARW (e.g. Sony A9 II: 14-bit samples stored as little-endian uint16,
    Bayer) and lossy "ARW Compressed" (Compression 32767). The newer "Lossless Compressed RAW"
    (Compression JPEG, e.g. A1 / A7R5) is not handled yet, so ImageDecoder falls back to the
    embedded JPG for those. Either way the sensor levels come from the ENCRYPTED SR2Private
    block, not from any plaintext IFD -- see applySr2Levels in sony.cpp. The unpack can walk
    the file's TIFF/EP IFDs itself rather than relying on ImageMetadata.
*/
class SonyRaw : public RawFormat
{
protected:
    bool UnpackCfa(QFile &file, const ImageMetadata &m, RawImage &raw) override;
};

#endif // SONY_H
