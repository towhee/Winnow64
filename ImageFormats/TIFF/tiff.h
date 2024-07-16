#ifndef TIFF_H
#define TIFF_H

#include <QtWidgets>
#include <QtConcurrent>
#include "Main/global.h"
#include "Utilities/utilities.h"
#include "Metadata/imagemetadata.h"
#include "Metadata/exif.h"
#include "Metadata/ifd.h"
#include "Metadata/irb.h"
#include "Metadata/xmp.h"
#include "Metadata/gps.h"
#include "ImageFormats/Jpeg/jpeg.h"
#include "Metadata/metareport.h"
#include <vector>
#include <cstring>

#include "zlib.h"
#include <tiffio.h>     // libtiff

extern bool showDebug;

class Tiff : public QObject
{
    Q_OBJECT

public:
    explicit Tiff(QString src = "");
    bool parse(MetadataParameters &p,
               ImageMetadata &m,
               IFD *ifd,
               IRB *irb,
               IPTC *iptc,
               Exif *exif,
               GPS *gps);
    bool parseForDecoding(MetadataParameters &p, /*ImageMetadata &m, */IFD *ifd);

    // decode from cache decoder
    bool decode(QString fPath, quint32 offset, QImage &image);

    // decode using unmapped QFile
    bool decode(ImageMetadata &m, QString &fPath, QImage &image,
                bool thumb = false, int maxDim = 0);

    // decode using QFile mapped to memory
    bool decode(MetadataParameters &p, QImage &image, int newSize = 0);

    // add a thumbnail to the tiff for faster thumbnail generation
    bool encodeThumbnail(MetadataParameters &p, ImageMetadata &m, IFD *ifd);

    // test libtiff
    QImage readTiffToQImage(const QString &filePath);
    void testLibtiff(QString fPath);

private:
    Utilities u;
    QImage *im;

    quint32 lastIFDOffsetPosition = 0;      // used to add thumbnail IFD to IFD chain
    quint32 thumbIFDOffset = 0;

    // from tiff ifd
    int width;
    int height;
    bool isBigEnd;
    int bitsPerSample = 0;
    int photoInterp = 0;
    int samplesPerPixel = 0;
    int rowsPerStrip = 0;
    int compression = 0;
    int predictor = 0;
    int planarConfiguration = 1;
    QVector<quint32> stripOffsets;
    QVector<quint32> stripByteCounts;

    // used to decode
    int bytesPerPixel;
    uint bytesPerRow;
    quint32 scanBytesAvail;                 // Maximum bytes in QImage im

    enum TiffType {
        unknown,
        tiff8bit,
        tiff16bit
    };

    enum {
        NoCompression,
        LzwCompression,
        LzwPredictorCompression,
        ZipCompression,
        JpgCompression
    };
    int compressionType;
    QStringList compressionString {"None", "LZW", "LZW Predictor", "Zip", "Jpg"};
    quint8 rgb[3];  // being used?

    struct TiffStrip {
        int strip;
        char* in;
        quint32 incoming;
        uchar* out;
        int bitsPerSample;
        uint bytesPerRow;
        quint32 stripBytes;
        bool predictor = false;
        /* for debugging
        QString fName;
        int rowsPerStrip;
        */
    };
    typedef QVector<TiffStrip> TiffStrips;
    QVector<QByteArray> inBa;

    QString err;
    bool isBigEndian(MetadataParameters &p);

    QString source;
    QString filePath;  // for debugging
    bool isDebug;

    void parseJpgTables(MetadataParameters &p, ImageMetadata &m);

    bool decodeBase(MetadataParameters &p);
    bool decodeLZW(MetadataParameters &p);
    bool decodeZip(MetadataParameters &p);
    bool decodeJpg(MetadataParameters &p);

    void perChannelToInterleave(QImage *im1);
    void toRRGGBBAA(QImage *im);
    void invertEndian16(QImage *im);
    void sample(ImageMetadata &m, int newLongside, int &nth, int &w, int &h);
    void scaleFromQImage(QImage &im, QByteArray &bas, int newLongSide);
    TiffType getTiffType();

    // LZW compression
    void lzwDecompress(TiffStrip &t, MetadataParameters &p);  // Rory tweaks
    void lzwDecompressOld(TiffStrip &t, MetadataParameters &p);  // Rory tweaks
    void lzwReset(QHash<quint32,QByteArray> &dictionary,
                  QByteArray &prevString,
                  quint32 &nextCode);
    TiffStrips zipDecompress(TiffStrip &t, MetadataParameters &p);  // ChatGPT created
    bool jpgDecompress(TiffStrip &t, MetadataParameters &p);

    // try libtiff
    int add_jpeg_thumbnail(TIFF* tif, uint32 width, uint32 height, uint8* thumb_data, uint32 thumb_size);
};

#endif // TIFF_H
