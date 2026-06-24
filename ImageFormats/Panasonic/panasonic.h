#ifndef PANASONIC_H
#define PANASONIC_H

#include <QtWidgets>
#include "Utilities/utilities.h"
#include "Metadata/imagemetadata.h"
#include "Metadata/exif.h"
#include "Metadata/ifd.h"
#include "Metadata/xmp.h"
#include "ImageFormats/Jpeg/jpeg.h"
#include "ImageFormats/Raw/rawformat.h"

class Panasonic : public QObject
{
    Q_OBJECT

public:
    Panasonic();
    bool parse(MetadataParameters &p,
               ImageMetadata &m,
               IFD *ifd,
               Exif *exif,
               Jpeg *jpeg);
private:
    QHash<quint32, QString> panasonicMakerHash;
    Utilities u;
};

/*
    Sensor unpack for Panasonic RW2 (the per-format override of RawFormat). RW2 is a TIFF-like
    container (magic 0x55) whose IFD0 carries Panasonic raw tags: SensorWidth/Height (0x02/0x03),
    RawFormat version (0x2D), RawDataOffset (0x118), per-channel WB (0x24/0x25/0x26) and black
    (0x1C..0x1E). Decodes the classic RawFormat-4 compression (dcraw's panasonic_load_raw): a
    14-column group code with a per-pair nibble predictor read through Panasonic's reversed
    16 KB "pana_bits" buffer (load_flags rotation). Ported from dcraw and validated byte-identical
    to libraw on a DC-GX9 RW2. RawFormat 5/6/7 (newer cameras) are not handled yet.
*/
class PanasonicRaw : public RawFormat
{
protected:
    bool UnpackCfa(QFile &file, const ImageMetadata &m, RawImage &raw) override;
};

#endif // PANASONIC_H
