#ifndef TIFF_H
#define TIFF_H

#include <QtWidgets>
#include <QtConcurrent>
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

#ifdef Q_OS_MAC
#include "ImageFormats/Tiff/libtiff.h"
#include <tiffio.h>    // libtiff
// used on macbookair
// #include "/opt/homebrew/Cellar/libtiff/4.7.0/include/tiffio.h"    // libtiff
// #include "zlib.h"
#endif
#ifdef Q_OS_WIN
#include "Lib/libtiff/libtiff/tiff.h"
#include "Lib/libtiff/libtiff/tiffio.h"
#include "Lib/libtiff/build/libtiff/tiffconf.h"
#include "zlib.h"
#endif

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
    bool parse(ImageMetadata &m, QString fPath);
    bool parseForDecoding(MetadataParameters &p, /*ImageMetadata &m, */IFD *ifd);

    // decode from cache decoder
    bool decode(QString fPath, quint32 offset, QImage &image);

    // decode using unmapped QFile
    bool decode(ImageMetadata &m, QString &fPath, QImage &image,
                bool thumb = false, int maxDim = 0);

    // decode using QFile mapped to memory
    bool decode(MetadataParameters &p, QImage &image, int newSize = 0);

    // add a thumbnail to the tiff for faster thumbnail generation
    // old version
    bool encodeThumbnailOld(MetadataParameters &p, ImageMetadata &m, IFD *ifd);
    // new call from thumb.cpp to ßembed thumbnail in tiff
    bool encodeThumbnail(MetadataParameters &p, ImageMetadata &m, IFD *ifd);
    // new call after image loaded using menu (thumb already exists in datamodel)
    bool encodeThumbnail(QString fPath, QImage &thumb);

    // add a thumbnail jpg in IRB block
    void embedIRBThumbnail(const QString tiffPath, const QImage &thumbnail);

    // // test libtiff
    // // #ifdef Q_OS_MAC
    // QImage readTiffToQImage(const QString &filePath);
    // QImage testLibtiff(QString fPath, int row);
    // void listDirectories(ImageMetadata &m);

    // // from QTiffHandler, adapted for Winnow and using Winnow libtiff, which reads jpg encoding
    bool read(QString fPath, QImage *image, quint32 ifdOffset = 0);
    // // #endif

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

    // Qt enumerations: https://doc.qt.io/qt-6/qimage.html#Format-enum
    enum TiffPhotometric {
        TIFF_PHOTOMETRIC_NONE       = -1,
        TIFF_PHOTOMETRIC_WHITE_IS_ZERO,      /* mono or grayscale, 0 is white */
        TIFF_PHOTOMETRIC_BLACK_IS_ZERO,      /* mono or grayscale, 0 is black */
        TIFF_PHOTOMETRIC_RGB,                /* RGB or RGBA*/
        TIFF_PHOTOMETRIC_PALETTE,            /* Uses a palette */
        TIFF_PHOTOMETRIC_ALPHA_MASK,         /* Transparency mask */
        TIFF_PHOTOMETRIC_SEPARATED,          /* CMYK or some other ink set */
        TIFF_PHOTOMETRIC_YCBCR,              /* YCbCr */
        TIFF_PHOTOMETRIC_CIE_LAB    = 8,     /* 1976 CIE L*a*b* */
        TIFF_PHOTOMETRIC_ICC_LAB,            /* ICC L*a*b* */
        TIFF_PHOTOMETRIC_ITU_LAB,            /* ITU L*a*b* */
        TIFF_PHOTOMETRIC_CFA        = 32803, /* Color Filter Array (TIFF/AP and DNG) */
        TIFF_PHOTOMETRIC_LOG_L      = 32844, /* CIE Log2(L) */
        TIFF_PHOTOMETRIC_LOG_LUV,            /* CIE Log L*u*v* */
        TIFF_PHOTOMETRIC_LINEAR_RAW = 34892, /* Linear Raw (DNG) */
    };


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
    quint32 parseIFDs(MetadataParameters &p, ImageMetadata &m, IFD *ifd,
                   quint32 &nextIFDOffset, int &thumbLongside, QString hdr);

    bool decodeBase(MetadataParameters &p);
    bool decodeLZW(MetadataParameters &p);
    bool decodeZip(MetadataParameters &p);
    bool decodeJpg(MetadataParameters &p);

    void perChannelToInterleave(QImage *im1);
    void toRRGGBBAA(QImage *im);
    void invertEndian16(QImage *im);
    void sample(ImageMetadata &m, int newLongside, int &nth, int &w, int &h);
    TiffType getTiffType();

    // LZW compression
    void lzwDecompress(TiffStrip &t, MetadataParameters &p);  // Rory tweaks
    void lzwDecompressOld(TiffStrip &t, MetadataParameters &p);  // Rory tweaks
    void lzwReset(QHash<quint32,QByteArray> &dictionary,
                  QByteArray &prevString,
                  quint32 &nextCode);
    TiffStrips zipDecompress(TiffStrip &t, MetadataParameters &p);  // ChatGPT created
    bool jpgDecompress(TiffStrip &t, MetadataParameters &p);

    // from QTiffHandler
    QImageIOHandler::Transformations exif2Qt(int exifOrientation);
    int qt2Exif(QImageIOHandler::Transformations transformation);
    void convert32BitOrder(void* buffer, int width);
    void rgb48fixup(QImage *image, bool floatingPoint);
    void rgb96fixup(QImage *image);
    void rgbFixup(QImage *image);
    bool readHeaders(TIFF *tiff, QSize &size, QImage::Format &format, uint16_t &photometric,
                     bool &grayscale, bool &floatingPoint,
                     QImageIOHandler::Transformations transformation);
};

#endif // TIFF_H
