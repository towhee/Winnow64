#ifndef FUJI_H
#define FUJI_H

#include <QtWidgets>
#include "Utilities/utilities.h"
#include "Metadata/imagemetadata.h"
#include "Metadata/exif.h"
#include "Metadata/ifd.h"
#include "Metadata/xmp.h"
#include "ImageFormats/Jpeg/jpeg.h"
#include "ImageFormats/Raw/rawformat.h"

class Fuji
{
public:
    Fuji();
    bool parse(MetadataParameters &p,
               ImageMetadata &m,
               IFD *ifd,
               Exif *exif,
               Jpeg *jpeg);
private:
    QHash<quint32, QString> fujiMakerHash;
    Utilities u;
};

/*
    Sensor unpack for Fujifilm RAF (the per-format override of RawFormat). RAF is a Fuji container
    ("FUJIFILMCCD-RAW "): a header with offsets to the embedded JPEG, the CFA metadata directory
    (raw dimensions, the X-Trans 6x6 pattern in tag 0x131, and a 0xC000 RAFData block holding the
    WB), and the CFA data. Fuji sensors are X-Trans (6x6) or Bayer. Decodes UNCOMPRESSED RAF only
    (16-bit samples); the Fuji lossless-compressed RAF (most current cameras) is not handled yet
    and falls back to the embedded JPEG. Pairs with the X-Trans path in Demosaic. Validated
    byte-identical to libraw on an uncompressed X-Trans RAF (X-T50). See notes/Documentation.txt.
*/
class FujiRaw : public RawFormat
{
protected:
    bool UnpackCfa(QFile &file, const ImageMetadata &m, RawImage &raw) override;
};

#endif // FUJI_H
