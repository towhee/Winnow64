#ifndef TIFF_H
#define TIFF_H

#include <QtWidgets>
#include "Main/global.h"
#include "Utilities/utilities.h"
#include "Metadata/imagemetadata.h"
#include "Metadata/exif.h"
#include "Metadata/ifd.h"
#include "Metadata/irb.h"
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
               IRB *irb,
               IPTC *iptc,
               Exif *exif,
               Jpeg *jpeg);
    bool parseForDecoding(MetadataParameters &p, ImageMetadata &m, IFD *ifd);
    void reportDecodingParamters(MetadataParameters &p);
    // decode using unmapped QFile
    bool decode(ImageMetadata &m, QString &fPath, QImage &image,
                bool thumb = false, int maxDim = 0);
    // decode using QFile mapped to memory
//    bool decode(ImageMetadata &m, QFile &file, QImage &image, int maxDim = 0);
    // decoding happens here
    bool decode(ImageMetadata &m, MetadataParameters &p, QImage &image, int maxDim = 0);
    bool encodeThumbnail(MetadataParameters &p, ImageMetadata &m, IFD *ifd);

private:
    quint32 lastIFDOffset = 0;      // used to add thumbnail IFD to IFD chain
    quint32 thumbIFDOffset = 0;

    // from tiff ifd
    int width;
    int height;
    int bitsPerSample = 0;
    int photoInterp = 0;
    int samplesPerPixel = 0;
    int rowsPerStrip = 0;
    int compression = 0;
    QVector<quint32> stripOffsets;
    QVector<quint32> stripByteCounts;
    int planarConfiguration = 1;

    // used to decode
    int bytesPerPixel;
    int bytesPerLine;

    enum TiffType {
        unknown,
        tiff8bit,
        tiff16bit
    };
//    typedef TiffType;

    void toRRGGBBAA(QImage *im);
    void invertEndian16(QImage *im);
    void sample(ImageMetadata &m, int newLongside, int &nth, int &w, int &h);
    bool unableToDecode();
    TiffType getTiffType();
};

#endif // TIFF_H
