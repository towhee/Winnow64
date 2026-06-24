#ifndef OLYMPUS_H
#define OLYMPUS_H

#include <QtWidgets>
#include "Utilities/utilities.h"
#include "Metadata/imagemetadata.h"
#include "Metadata/exif.h"
#include "Metadata/ifd.h"
#include "Metadata/xmp.h"
#include "Metadata/gps.h"
#include "ImageFormats/Jpeg/jpeg.h"
#include "ImageFormats/Raw/rawformat.h"

class Olympus : public QObject
{
    Q_OBJECT

public:
    Olympus();
    bool parse(MetadataParameters &p,
               ImageMetadata &m,
               IFD *ifd,
               Exif *exif,
               Jpeg *jpeg,
               GPS *gps);
private:
    QHash<quint32, QString> olympusMakerHash,
                            olympusCameraSettingsHash,
                            olympusEquipmentHash;
    Utilities u;
};

/*
    Sensor unpack for Olympus ORF (the per-format override of RawFormat). The ORF TIFF lies
    (it tags the raw strip Compression 1 / BitsPerSample 16) but the strip is actually Olympus's
    proprietary 12-bit compression: a Huffman magnitude code + adaptive bit length + a median
    predictor with a 2-column running carry. Ported from dcraw's olympus_load_raw and validated
    byte-identical to libraw on an E-M1 ORF.

    Colour matrix comes from the shared per-model table (cameramatrix.cpp). The as-shot white
    balance IS read (Olympus MakerNote ImageProcessing 0x2040 -> 0x0100 WB_RBLevels) -- essential,
    since matrix-neutral renders badly green for Olympus. FIRST CUT: white 4095 (12-bit scheme),
    and E-M1-validated black (255) and BGGR pattern (Olympus omits CFAPattern); per-model black/
    pattern/bit-depth are a refinement. See notes/Documentation.txt "RAW DECODING".
*/
class OlympusRaw : public RawFormat
{
protected:
    bool UnpackCfa(QFile &file, const ImageMetadata &m, RawImage &raw) override;
};

#endif // OLYMPUS_H
