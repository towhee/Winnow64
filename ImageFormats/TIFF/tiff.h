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
    bool parseForDecoding(MetadataParameters &p, ImageMetadata &m, IFD *ifd);
    bool decode(ImageMetadata &m, QString &fPath, QImage &image, int maxDim = 0);

private:
    int bitsPerSample = 0;
    int photoInterp = 0;
    int samplesPerPixel = 0;
    int rowsPerStrip = 0;
    int compression = 0;
    QVector<quint32> stripOffsets;
    QVector<quint32> stripByteCounts;
    int planarConfiguration = 1;

    enum TiffType {
        unknown,
        tiff8bit,
        tiff16bit
    };

    void toRRGGBBAA(QImage *im);
    void invertEndian16(QImage *im);
};

#endif // TIFF_H
