#ifndef METADATA_H
#define METADATA_H

#include <QtWidgets>
#include <QtCore>
#include <QtXmlPatterns>
#include <iostream>
#include <iomanip>
//#include <QElapsedTimer>
//#include <QThread>

#include "Main/global.h"
#include "Metadata/exif.h"
#include "Metadata/ifd.h"
#include "Metadata/iptc.h"
#include "Metadata/metareport.h"
#include "Utilities/utilities.h"
#include "Cache/tshash.h"
#include "Metadata/imagemetadata.h"
#include "xmp.h"
#include "ui_metadatareport.h"

#include "ImageFormats/Heic/heic.h"
#include "ImageFormats/Jpeg/jpeg.h"
#include "ImageFormats/Canon/canon.h"
#include "ImageFormats/Dng/dng.h"
#include "ImageFormats/Fuji/fuji.h"
#include "ImageFormats/Nikon/nikon.h"
#include "ImageFormats/Olympus/olympus.h"
#include "ImageFormats/Panasonic/panasonic.h"

class IFDData
{
public:
    quint32 tagType;
    quint32 tagCount;
    quint32 tagValue;
    short tagReal;
};

//Q_DECLARE_METATYPE(ImageMetadata)
typedef TSHash<int, ImageMetadata> MetaHash;

class Metadata : public QObject
{
    Q_OBJECT
public:
    Metadata(QObject *parent = nullptr);
    bool readMetadata(bool report, const QString &path);
    QString diagnostics(QString fPath);
    void reportMetadata();
    void testNewFileFormat(const QString &path);

    // variables used to hold data before insertion into QMap metaCache
    int row;                // datamodel row
    bool isPicked;
    quint32 offsetFullJPG;
    quint32 lengthFullJPG;
    quint32 offsetThumbJPG;
    quint32 lengthThumbJPG;
    quint32 offsetSmallJPG;
    quint32 lengthSmallJPG;
    quint32 xmpSegmentOffset;
    quint32 xmpNextSegmentOffset;
    bool isXmp;
    quint32 iccSegmentOffset;
    quint32 iccSegmentLength;
    QByteArray iccBuf;
    QString iccSpace;
    quint32 orientationOffset;
    int orientation;
    int rotationDegrees;            // additional rotation from edit
    uint width;
    uint height;
    QString dimensions;
//    QString created;
    QDateTime createdDate;
    QString make;
    QString model;
    QString exposureTime;
    float exposureTimeNum;
    QString aperture;
    float apertureNum;
    QString ISO;
    int ISONum;
    QString focalLength;
    int focalLengthNum;
    QString shootingInfo;
    QString title;
    QString _title;                         // original value
    QString rating;
    QString _rating;                        // original value
    QString label;
    QString _label;                         // original value
    QString lens;
    QString creator;
    QString _creator;                       // original value
    QString copyright;
    QString _copyright;                     // original value
    QString email;
    QString _email;                         // original value
    QString url;
    QString _url;                           // original value
    QString cameraSN;
    QString lensSN;
    uint shutterCount;
    bool metadataLoaded;

    ImageMetadata imageMetadata;            // agregate for mdCacher
    // end variables used to hold data

    QString fPath;
    QString err;
    bool thumbUnavailable;                  // no embedded thumb
    bool imageUnavailable;                  // no embedded preview

//    QMap<QString, QString> exampleMap;

    QStringList rawFormats;
    QStringList sidecarFormats;
    QStringList getMetadataFormats;
    QStringList embeddedICCFormats;
    QStringList internalXmpFormats;
    QStringList xmpWriteFormats;
    QStringList supportedFormats;
/*
    enum tagDataType {
        "uchar",
        "string",
        "ushort",
        "quint32",
        "urational",
        "char",
        "bytes",
        "short",
        "quint32",
        "rational",
        "float4",
        "float8"
    }  */

    void removeImage(QString &imageFileName);
    void setPick(const QString &imageFileName, bool choice);
    void clear();
    void clearMetadata();

    bool writeMetadata(const QString &imageFileName, ImageMetadata m, QByteArray &buffer);

    QFile file;
    bool okToReadXmp;
    bool readEssentialMetadata;
    bool readNonEssentialMetadata;
    bool foundTifThumb;

    QString nikonLensCode;

private:
    // Exif
    Exif *exif = nullptr;
    IFD *ifd = nullptr;
    IPTC *iptc = nullptr;

    // formats
//    Heic *heic = nullptr;
    Jpeg *jpeg = nullptr;
    Canon *canon = nullptr;
    DNG *dng = nullptr;
    Fuji *fuji = nullptr;
    Nikon *nikon = nullptr;
    Olympus *olympus = nullptr;
    Panasonic *panasonic = nullptr;

    // hash
    QHash<uint, IFDData> ifdDataHash;
    QHash<uint, IFDData>::iterator ifdIter;
    QHash<quint32, QString> exifHash, ifdHash, gpsHash, segCodeHash,
        nikonMakerHash, sonyMakerHash, fujiMakerHash, canonMakerHash,
        panasonicMakerHash, canonFileInfoHash;
    QHash<QString, quint32> segmentHash;
//    QHash<QByteArray, QString> nikonLensHash;
    QHash<QString, QString> nikonLensHash;
    QHash<int, QString> sonyAFAreaModeHash;
    QHash<int, QString> orientationDescription;
    QHash<int,int> orientationToDegrees;
    QHash<int,int> orientationFromDegrees;

    // metadata cache
    QMap<QString, ImageMetadata> metaCache;

    bool report;
    QString xmpString;
    quint32 xmpmetaRoom;
    QString reportString;
    QTextStream rpt;
    quint32 order;

    void initSupportedFiles();
    void initSegCodeHash();
    void initExifHash();
    void initIfdHash();
    void initOrientationHash();
    void initNikonMakerHash();
    void initCanonMakerHash();
    void initSonyMakerHash();
    void initFujiMakerHash();
    void initPanasonicMakerHash();
    void initNikonLensHash();
    void initSonyAFAreaModeHash();

    int get4_1st(QByteArray c);
    int get4_2nd(QByteArray c);
    uint get8(QByteArray c);
    quint16 get16(QByteArray c);
    quint16 get16(quint32 offset);
    quint32 get32(QByteArray c);
    quint32 get32(quint32 offset);
    LONGLONG get64(QByteArray c);
    double getReal(quint32 offset);
    quint32 findInFile(QString s, quint32 offset, quint32 range);
    bool readXMP(quint32 offset);
    void readIPTC(quint32 offset);
    quint32 readIFD(QString hdr, quint32 offset);
    bool readIRB(quint32 offset);
    void verifyEmbeddedJpg(quint32 &offset, quint32 &length);
    QList<quint32> getSubIfdOffsets(quint32 subIFDaddr, int count);
//    quint32 getExifOffset(quint32 offsetIfd0);      //update to use ifdDataHash
    QString getString(quint32 offset, quint32 length);
    QString getCString();
    QString getCString(quint32 offset);
    QByteArray getByteArray(quint32 offset, quint32 length);
    void getJpgSegments(qint64 offset);
    void getJpgICC(qint64 offset);
    bool getDimensions(quint32 jpgOffset);
    int getNewOrientation(int orientation, int rotation);

    bool nextHeifBox(quint32 &length, QString &type);
    bool getHeifBox(QString &type, quint32 &offset, quint32 &length);
    bool ftypBox(quint32 &offset, quint32 &length);
    bool metaBox(quint32 &offset, quint32 &length);
    bool hdlrBox(quint32 &offset, quint32 &length);
    bool pitmBox(quint32 &offset, quint32 &length);
    bool ilocBox(quint32 &offset, quint32 &length);  //
    bool iinfBox(quint32 &offset, quint32 &length);  // Item Information Box
    bool infeBox(quint32 &offset, quint32 &length);  // Item Info Entry
    bool irefBox(quint32 &offset, quint32 &length);
    bool sitrBox(quint32 &offset, quint32 &length);  // Single Item Type Reference Box
    bool sitrBoxL(quint32 &offset, quint32 &length); // Single Item Type Reference Box Large
    bool iprpBox(quint32 &offset, quint32 &length);
    bool hvcCBox(quint32 &offset, quint32 &length);
    bool ispeBox(quint32 &offset, quint32 &length);
    bool ipmaBox(quint32 &offset, quint32 &length);
    bool mdatBox(quint32 &offset, quint32 &length);

    QByteArray nikonDecrypt(QByteArray bData, uint32_t count, uint32_t serial);

    void reportMetadataHeader(QString title);
    void reportIfdDataHash();

    bool formatNikon();
    bool formatCanon();
    bool formatOlympus();
    bool formatSony();
    bool formatFuji();
    bool formatPanasonic();
    bool formatJPG(quint32 startOffset);
    bool formatTIF();
    bool formatDNG();
//    bool formatHEIF();

//    // heif variables
//    struct Heif {
//        quint32 metaOffset;
//        quint32 metaLength;
//        quint16 pitmId;
//        int ilocOffsetSize;
//        int ilocLengthSize;
//        int ilocBaseOffsetSize;
//        int ilocItemCount;
//        int ilocExtentCount;
//        quint32 irefOffset;
//        quint32 irefLength;
//    } heif;

signals:

public slots:
    bool formatHEIF();
    void loadFromThread(QFileInfo &fileInfo);
    bool loadImageMetadata(const QFileInfo &fileInfo,
                           bool essential = true,
                           bool nonEssential = true,
                           bool isReport = false,
                           bool isLoadXmp = false,
                           QString source = "");

private:
    void track(QString fPath, QString msg);
};

#endif // METADATA_H
