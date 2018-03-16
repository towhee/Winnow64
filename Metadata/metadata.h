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
#include "xmp.h"
#include "ui_metadatareport.h"

class IFDData
{
public:
    ulong tagType;
    ulong tagCount;
    ulong tagValue;
    short tagReal;
};

class ImageMetadata
{
/*
This class structure of all the fields in metadata is used to insert a new item
into "QMap<QString, ImageMetadata> metaCache" in Metadata::loadImageMetadata.
*/
public:
    bool isPicked;          //rgh required?
    ulong offsetFullJPG;
    ulong lengthFullJPG;
    ulong offsetThumbJPG;
    ulong lengthThumbJPG;
    ulong offsetSmallJPG;
    ulong lengthSmallJPG;

    ulong xmpSegmentOffset;
    ulong xmpNextSegmentOffset;
    bool isXmp;
    ulong orientationOffset;
    int orientation;
    int rotationDegrees;            // additional rotation from edit
    ulong width;
    ulong height;
    QString dimensions;
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
    QString _title;                     // original value
    QString rating;
    QString _rating;                    // original value
    QString label;
    QString _label;                     // original value
    QString lens;
    QString creator;
    QString _creator;                   // original value
    QString copyright;
    QString _copyright;                 // original value
    QString email;
    QString _email;                     // original value
    QString url;
    QString _url;                       // original value
    QString cameraSN;
    QString lensSN;
    ulong shutterCount;
    int year;
    int month;
    int day;
    QString copyFileNamePrefix;
    bool metadataLoaded = false;    // all metadata except thumb
    bool isThumbLoaded;             // refers to thumb only
    bool thumbUnavailable = false;  // no embedded thumb
    bool imageUnavailable = false;  // no embedded preview
    QString err = "";
};

class Metadata : public QObject
{
    Q_OBJECT
public:
    Metadata(QObject *parent = 0);
    bool readMetadata(bool report, const QString &path);
    void reportMetadataAllFiles();
    void reportMetadata();

    // variables used to hold data before insertion into QMap metaCache
    bool isPicked;
    ulong offsetFullJPG;
    ulong lengthFullJPG;
    ulong offsetThumbJPG;
    ulong lengthThumbJPG;
    ulong offsetSmallJPG;
    ulong lengthSmallJPG;

    ulong xmpSegmentOffset;
    ulong xmpNextSegmentOffset;
    bool isXmp;
    ulong orientationOffset;
    int orientation;
    int rotationDegrees;            // additional rotation from edit
    ulong width;
    ulong height;
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
    ulong shutterCount;
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
        "ulong",
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
    bool isLoaded(const QString &imageFullPath);
    bool isThumbLoaded(const QString &imageFullPath);
    ulong getOffsetFullJPG(const QString &imageFullPath);
    ulong getLengthFullJPG(const QString &imageFullPath);
    ulong getOffsetThumbJPG(const QString &imageFullPath);
    ulong getLengthThumbJPG(const QString &imageFullPath);
    ulong getOffsetSmallJPG(const QString &imageFullPath);
    ulong getLengthSmallJPG(const QString &imageFullPath);
    ulong getWidth(const QString &imageFullPath);
    ulong getHeight(const QString &imageFullPath);
    QString getDimensions(const QString &imageFullPath);
    int getOrientation(QString &imageFileName);
    int getRotation(QString &imageFileName);
    void setRotation(const QString &imageFileName, const int rotationDegrees);
    bool getPick(const QString &imageFileName);
//    QString getCreated(const QString &imageFileName);
    QString getMake(const QString &imageFileName);
    QString getModel(const QString &imageFileName);
    QString getExposureTime( const QString &imageFileName);
    float getExposureTimeNum( const QString &imageFileName);
    QString getAperture(const QString &imageFileName);
    qreal getApertureNum(const QString &imageFileName);
    QString getISO(const QString &imageFileName);
    int getISONum(const QString &imageFileName);
    QString getFocalLength(const QString &imageFileName);
    int getFocalLengthNum(const QString &imageFileName);
    QString getShootingInfo(const QString &imageFileName);
    QString getTitle(const QString &imageFileName);
    void setTitle(const QString &imageFileName, const QString &title);
    QString getRating(const QString &imageFileName);
    void setRating(const QString &imageFileName, const QString &rating);
    QString getLabel(const QString &imageFileName);
    void setLabel(const QString &imageFileName, const QString &label);
    QString getLens(const QString &imageFileName);
    QString getCreator(const QString &imageFileName);
    void setCreator(const QString &imageFileName, const QString &creator);
    QString getCopyright(const QString &imageFileName);
    void setCopyright(const QString &imageFileName, const QString &copyright);
    QString getEmail(const QString &imageFileName);
    void setEmail(const QString &imageFileName, const QString &email);
    QString getUrl(const QString &imageFileName);
    void setUrl(const QString &imageFileName, const QString &url);
    QDateTime getCreatedDate(const QString &imageFileName);
    int getYear(const QString &imageFileName);
    int getMonth(const QString &imageFileName);
    int getDay(const QString &imageFileName);
    bool getImageUnavailable(const QString &imageFileName);
    bool getThumbUnavailable(const QString &imageFileName);
    QString getErr(const QString &imageFileName);
    void setErr(const QString &imageFileName, const QString &err);
    QString getCopyFileNamePrefix(const QString &imageFileName);

    bool writeMetadata(const QString &imageFileName, QByteArray &buffer);
    void setMetadata(const QString &imageFileName);

    bool okToReadXmp;
    bool readEssentialMetadata;
    bool readNonEssentialMetadata;
    bool foundTifThumb;

    QString nikonLensCode;

private:
    QFile file;
    QHash<uint, IFDData> ifdDataHash;
    QHash<uint, IFDData>::iterator ifdIter;
    QHash<ulong, QString> exifHash, ifdHash, gpsHash, segCodeHash,
        nikonMakerHash, sonyMakerHash, fujiMakerHash, canonMakerHash,
        canonFileInfoHash;
    QHash<QString, ulong> segmentHash;
//    QHash<QByteArray, QString> nikonLensHash;
    QHash<QString, QString> nikonLensHash;
    QHash<int, QString> orientationDescription;
    QHash<int,int> orientationToDegrees;
    QHash<int,int> orientationFromDegrees;

    // metadata cache
    QMap<QString, ImageMetadata> metaCache;

    bool report;
    QString xmpString;
    ulong xmpmetaRoom;
    QString reportString;
    QTextStream rpt;
    long order;

    void initSupportedFiles();
    void initSegCodeHash();
    void initExifHash();
    void initIfdHash();
    void initOrientationHash();
    void initNikonMakerHash();
    void initCanonMakerHash();
    void initSonyMakerHash();
    void initFujiMakerHash();
    void initNikonLensHash();

    uint get1(QByteArray c);
    ulong get2(QByteArray c);
    ulong get2(long offset);
    ulong get4(QByteArray c);
    ulong get4(long offset);
    float getReal(long offset);
    ulong findInFile(QString s, ulong offset, ulong range);
    bool readXMP(ulong offset);
    void readIPTC(ulong offset);
    ulong readIFD(QString hdr, ulong offset);
    bool readIRB(ulong offset);
    void verifyEmbeddedJpg(ulong &offset, ulong &length);
    QList<ulong> getSubIfdOffsets(ulong subIFDaddr, int count);
//    ulong getExifOffset(ulong offsetIfd0);      //update to use ifdDataHash
    QString getString(ulong offset, ulong length);
    QByteArray getByteArray(ulong offset, ulong length);
    void getSegments(ulong offset);
    bool getDimensions(ulong jpgOffset);
    int getNewOrientation(int orientation, int rotation);


    QByteArray nikonDecrypt(QByteArray bData, uint32_t count, uint32_t serial);

    void reportMetadataHeader(QString title);
    void reportIfdDataHash();

    void formatNikon();
    void formatCanon();
    void formatOlympus();
    void formatSony();
    bool formatFuji();
    bool formatJPG();
    bool formatTIF();

signals:

public slots:
    void loadFromThread(QFileInfo &fileInfo);
    bool loadImageMetadata(const QFileInfo &fileInfo,
                           bool essential = true,
                           bool nonEssential = true,
                           bool isReport = false,
                           bool isLoadXmp = false);

private:
    void track(QString fPath, QString msg);
};

#endif // METADATA_H
