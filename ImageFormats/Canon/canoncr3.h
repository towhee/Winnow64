#ifndef CANONCR3_H
#define CANONCR3_H

#include <QObject>
#include <QtWidgets>
#include <QtCore>
#include <QtXmlPatterns>
#include <iostream>
#include <iomanip>

#include "Main/global.h"
#include "Utilities/utilities.h"
#include "Metadata/imagemetadata.h"
#include "Metadata/exif.h"
#include "Metadata/ifd.h"
#include "Metadata/gps.h"
#include "Metadata/xmp.h"
#include "ImageFormats/Jpeg/jpeg.h"

class CanonCR3
{
public:
    CanonCR3(MetadataParameters &p,
             ImageMetadata &m,
             IFD *ifd,
             Exif *exif,
             Jpeg *jpeg);
//    CanonCR3();
    bool parse();

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
    bool urnBox (quint32 &offset, quint32 &length);  // Data Entry Urn Box
    bool colrBox(quint32 &offset, quint32 &length);  // Color Information Box
    bool irotBox(quint32 &offset, quint32 &length);  // Color Information Box
    bool pixiBox(quint32 &offset, quint32 &length);  // Color Information Box
    bool freeBox(quint32 &offset, quint32 &length);  // Free space to ignore
    bool thmbBox(quint32 &offset, quint32 &length);  // thumbnail jpg
    bool moovBox(quint32 &offset, quint32 &length);  // container
    bool uuidBox(quint32 &offset, quint32 &length);  // container
    bool cmt1Box(quint32 &offset, quint32 &length);  // IFD0
    bool cmt2Box(quint32 &offset, quint32 &length);  // ExifIFD
    bool cmt3Box(quint32 &offset, quint32 &length);  // ExifIFD
    bool cmt4Box(quint32 &offset, quint32 &length);  // ExifIFD
    bool trakBox(quint32 &offset, quint32 &length);  // Track
    bool tkhdBox(quint32 &offset, quint32 &length);  // Track header
    bool mdiaBox(quint32 &offset, quint32 &length);  // Media info for track
    bool mdhdBox(quint32 &offset, quint32 &length);  // Media header
    bool minfBox(quint32 &offset, quint32 &length);  //
    bool vmhdBox(quint32 &offset, quint32 &length);  //
    bool stblBox(quint32 &offset, quint32 &length);  //
    bool stsdBox(quint32 &offset, quint32 &length);  //
    bool sttsBox(quint32 &offset, quint32 &length);  //
    bool stscBox(quint32 &offset, quint32 &length);  //
    bool stszBox(quint32 &offset, quint32 &length);  //
    bool co64Box(quint32 &offset, quint32 &length);  // trak - embedded jpg offset

    void parseIfd0();
    void parseExif();


    MetadataParameters &p;
    ImageMetadata &m;
    IFD *ifd;
    Exif *exif;
    Jpeg *jpeg;

    qint64 eof;
    quint32 ifd0Offset;
    quint32 exifOffset;
    int trakCount;
};

#endif // CANONCR3_H
