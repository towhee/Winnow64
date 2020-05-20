#ifndef TIFF_H
#define TIFF_H

#include <QtWidgets>
#include "Main/global.h"
#include "Utilities/utilities.h"
#include "Metadata/imagemetadata.h"
#include "Metadata/exif.h"
#include "Metadata/ifd.h"
#include "Metadata/xmp.h"
#include "ImageFormats/Jpeg/jpeg.h"
#include "Metadata/metareport.h"
//#include "Datamodel/datamodel.h"

class Tiff : public QObject
{
    Q_OBJECT

public:
    explicit Tiff();
    bool parse(MetadataParameters &p,
               ImageMetadata &m,
               IFD *ifd,
               IPTC *iptc,
               Exif *exif,
               Jpeg *jpeg);

    bool decode(ImageMetadata &m, QFile &file, QImage &image, bool mainImage);
private:
//    DataModel *dm;
    int w = 0;
    int h = 0;
    int bitsPerSample = 0;
    int photoInterp = 0;
    int samplesPerPixel = 0;
    quint32 stripByteCounts = 0;
    int compression = 0;
    quint32 offset = 0;
    quint32 length = 0;

    enum TiffType {
        unknown = 0,
        tiff16bit_II = 1,
        tiff16bit_MM = 2
    };
};

#endif // TIFF_H
