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
    WB), and the CFA data. Fuji sensors are X-Trans (6x6) or Bayer. Decodes both uncompressed RAF
    (16-bit samples) and the Fuji lossless/lossy-compressed RAF (most current cameras), the latter
    via FujiCompressed::decode (see fujicompressed.h). Pairs with the X-Trans path in Demosaic.
    Validated byte-identical to libraw on uncompressed (X-T50) and compressed X-Trans (X-T2) and
    Bayer (GFX 50S II) RAFs. See notes/Documentation.txt.
*/
class FujiRaw : public RawFormat
{
protected:
    bool UnpackCfa(QFile &file, const ImageMetadata &m, RawImage &raw) override;
};

#endif // FUJI_H
