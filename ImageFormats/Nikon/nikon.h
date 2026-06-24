#ifndef NIKON_H
#define NIKON_H

#include <QtWidgets>
#include "Utilities/utilities.h"
#include "Metadata/imagemetadata.h"
#include "Metadata/exif.h"
#include "Metadata/ifd.h"
#include "Metadata/xmp.h"
#include "Metadata/gps.h"
#include "ImageFormats/Jpeg/jpeg.h"
#include "ImageFormats/Raw/rawformat.h"

class Nikon : public QObject
{
    Q_OBJECT

public:
    Nikon();
    bool parse(MetadataParameters &p,
               ImageMetadata &m,
               IFD *ifd,
               Exif *exif,
               Jpeg *jpeg,
               GPS *gps);
private:
    QByteArray nikonDecrypt(QByteArray bData, uint32_t count, uint32_t serial);
    QHash<quint32, QString> nikonMakerHash;
    QHash<QString, QString> nikonLensHash;
    Utilities u;
};

/*
    Sensor unpack for Nikon NEF (the per-format override of RawFormat). Self-contained: walks the
    NEF TIFF IFDs (via TiffWalk) to find the CFA SubIFD, reads the linearization/Huffman metadata
    from the Nikon type-3 MakerNote (tag 0x96), and decodes Nikon's compressed raw -- Huffman-
    coded differences against dual (vertical + horizontal) predictors, mapped through a
    linearization curve. Ported from dcraw's nikon_load_raw and validated byte-identical to
    libraw on 12-bit and 14-bit lossless NEF.

    Colour matrix comes from the shared per-model table (cameramatrix.cpp). The as-shot white
    balance is read from the UNENCRYPTED MakerNote tag 0x0C (WhiteBalanceRBLevels) -- the
    encrypted ColorBalance (0x97) carries the same values the hard way and is not needed. See
    notes/Documentation.txt "RAW DECODING".
*/
class NikonRaw : public RawFormat
{
protected:
    bool UnpackCfa(QFile &file, const ImageMetadata &m, RawImage &raw) override;
};

#endif // NIKON_H
