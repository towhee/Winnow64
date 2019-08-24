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
#include "Utilities/utilities.h"
#include "Cache/tshash.h"
#include "Metadata/imagemetadata.h"
#include "xmp.h"
#include "ui_metadatareport.h"

class IFDData
{
public:
    long tagType;
    long tagCount;
    long tagValue;
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
    uint offsetFullJPG;
    uint lengthFullJPG;
    uint offsetThumbJPG;
    uint lengthThumbJPG;
    uint offsetSmallJPG;
    uint lengthSmallJPG;

    uint xmpSegmentOffset;
    uint xmpNextSegmentOffset;
    bool isXmp;
    uint orientationOffset;
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
    QStringList internalXmpFormats;
    QStringList xmpWriteFormats;
    QStringList supportedFormats;
/*
    enum tagDataType {
        "uchar",
        "string",
        "ushort",
        "long",
        "urational",
        "char",
        "bytes",
        "short",
        "long",
        "rational",
        "float4",
        "float8"
    }  */

    void removeImage(QString &imageFileName);
    void setPick(const QString &imageFileName, bool choice);
    void clear();
    void clearMetadata();

    bool writeMetadata(const QString &imageFileName, ImageMetadata m, QByteArray &buffer);

    bool okToReadXmp;
    bool readEssentialMetadata;
    bool readNonEssentialMetadata;
    bool foundTifThumb;

    QString nikonLensCode;

private:
    QFile file;
    QHash<uint, IFDData> ifdDataHash;
    QHash<uint, IFDData>::iterator ifdIter;
    QHash<long, QString> exifHash, ifdHash, gpsHash, segCodeHash,
        nikonMakerHash, sonyMakerHash, fujiMakerHash, canonMakerHash,
        panasonicMakerHash, canonFileInfoHash;
    QHash<QString, long> segmentHash;
//    QHash<QByteArray, QString> nikonLensHash;
    QHash<QString, QString> nikonLensHash;
    QHash<int, QString> orientationDescription;
    QHash<int,int> orientationToDegrees;
    QHash<int,int> orientationFromDegrees;

    // metadata cache
    QMap<QString, ImageMetadata> metaCache;

    bool report;
    QString xmpString;
    long xmpmetaRoom;
    QString reportString;
    QTextStream rpt;
    long order;
//    long order;

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

    uint get1(QByteArray c);
    long get2(QByteArray c);
    long get2(long offset);
    long get4(QByteArray c);
    long get4(long offset);
    double getReal(long offset);
    long findInFile(QString s, long offset, long range);
    bool readXMP(long offset);
    void readIPTC(long offset);
    long readIFD(QString hdr, long offset);
    bool readIRB(long offset);
    void verifyEmbeddedJpg(long &offset, long &length);
    QList<long> getSubIfdOffsets(long subIFDaddr, int count);
//    long getExifOffset(long offsetIfd0);      //update to use ifdDataHash
    QString getString(long offset, long length);
    QByteArray getByteArray(long offset, long length);
    void getSegments(long offset);
    bool getDimensions(long jpgOffset);
    int getNewOrientation(int orientation, int rotation);


    QByteArray nikonDecrypt(QByteArray bData, uint32_t count, uint32_t serial);

    void reportMetadataHeader(QString title);
    void reportIfdDataHash();

    bool formatNikon();
    bool formatCanon();
    bool formatOlympus();
    bool formatSony();
    bool formatFuji();
    bool formatPanasonic();
    bool formatJPG();
    bool formatTIF();
    bool formatDNG();

signals:

public slots:
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
