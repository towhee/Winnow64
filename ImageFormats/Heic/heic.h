#ifndef HEIC_H
#define HEIC_H

#include <QtWidgets>
#include <QtCore>
//#include <QtXmlPatterns>
#include <iostream>
#include <iomanip>

#include "Utilities/utilities.h"
#include "Metadata/imagemetadata.h"
#include "Metadata/exif.h"
#include "Metadata/ifd.h"
#include "Metadata/gps.h"
//#include "Metadata/xmp.h"
#ifdef Q_OS_WIN
#include "Lib/libde265/include/de265.h"
#include "Lib/libheif/include/heif.h"
#endif

class Heic : public QObject
{
    Q_OBJECT

public:
    Heic();
    bool parseLibHeif(MetadataParameters &p, ImageMetadata &m, IFD *ifd, Exif *exif, GPS *gps);
    bool parseHeic(MetadataParameters &p, ImageMetadata &m, IFD *ifd, Exif *exif, GPS *gps);
    bool parseExif(MetadataParameters &p, ImageMetadata &m, IFD *ifd, Exif *exif, GPS *gps);
    bool decodePrimaryImage(QString &fPath, QImage &image);
    bool decodeThumbnail(QString &fPath, QImage &image);

    // the numeric values map directly to the values of chroma_format_idc in the h.265 bitstream
    enum de265_chroma {
      de265_chroma_mono = 0,
      de265_chroma_420  = 1,
      de265_chroma_422  = 2,
      de265_chroma_444  = 3
    };

    enum de265_colorspace {
      de265_colorspace_YCbCr= 0,
      de265_colorspace_GBR  = 1
    };

    struct Context {
        bool is_primary;
        int w;
        int h;
        de265_chroma chroma;
        int bitDepth_luma;
        int bitDepth_chroma;
        quint64 pts;
    };

    quint32 metaOffset;
    quint32 metaLength;
    quint16 pitmId;
    int ilocOffsetSize;
    int ilocLengthSize;
    int ilocBaseOffsetSize;
    int ilocItemCount;
    int ilocExtentCount;
    quint32 irefOffset;
    quint32 irefLength;

private:
    bool nextHeifBox(quint32 &length, QString &type);
    bool getHeifBox(QString &type, quint32 &offset, quint32 &length);

    bool dinfBox(quint32 &offset, quint32 &length);  // Data Information Box (Container)
    bool drefBox(quint32 &offset, quint32 &length);  // Data Reference Box
    bool ftypBox(quint32 &offset, quint32 &length);  // File Type Box
    bool hdlrBox(quint32 &offset, quint32 &length);  // Metadata Handler Box
    bool hvcCBox(quint32 &offset, quint32 &length);  // HEVC Configuration Item Property Box
    bool ilocBox(quint32 &offset, quint32 &length);  // Item Location Box
    bool iinfBox(quint32 &offset, quint32 &length);  // Item Information Box
    bool infeBox(quint32 &offset, quint32 &length);  // Item Info Entry
    bool ipmaBox(quint32 &offset, quint32 &length);  // Image Property Association Box
    bool iprpBox(quint32 &offset, quint32 &length);  // Item Properties Box
    bool irefBox(quint32 &offset, quint32 &length);  // Item Reference Box
    bool ispeBox(quint32 &offset, quint32 &length);  // Image Spacial Extent Box
    bool mdatBox(quint32 &offset, quint32 &length);  // Media Data Box
    bool idatBox(quint32 &offset, quint32 &length);  // Media Data Box
    bool metaBox(quint32 &offset, quint32 &length);  // Metadata Box (Container)
    bool pitmBox(quint32 &offset, quint32 &length);  // Primary Item Box
    bool sitrBox(quint32 &offset, quint32 &length);  // Single Item Type Reference Box
    bool sitrBoxL(quint32 &offset, quint32 &length); // Single Item Type Reference Box Large
    bool urlBox(quint32 &offset, quint32 &length);   // Data Entry Url Box
    bool urnBox(quint32 &offset, quint32 &length);   // Data Entry Urn Box
    bool colrBox(quint32 &offset, quint32 &length);  // Color Information Box
    bool irotBox(quint32 &offset, quint32 &length);  // Color Information Box
    bool pixiBox(quint32 &offset, quint32 &length);  // Color Information Box

    QFile *file;        // temp until decide what to do with constructor
    ImageMetadata *m;
    QString fPath;
    qint64 eof;
    int exifItemID;
    quint32 exifOffset;
    quint32 exifLength;
    bool isDebug;
};

#endif // HEIC_H

/*
Image data appears to be stored in image.h  class decoder_context;
    template <class DataUnit> class MetaDataArray
*/
