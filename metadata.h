#ifndef METADATA_H
#define METADATA_H

#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QDebug>
#include <QHash>
#include <QList>
#include <iostream>
#include <iomanip>
#include <QElapsedTimer>
#include <QThread>

//#include "global.h"

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

public:
    bool isPicked;          //rgh
    ulong offsetFullJPG;
    ulong lengthFullJPG;
    ulong offsetThumbJPG;
    ulong lengthThumbJPG;
    ulong offsetSmallJPG;
    ulong lengthSmallJPG;
    int orientation;
    ulong width;
    ulong height;
    QString dateTime;
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
    int year;
    int month;
    int day;
    QString copyFileNamePrefix;
    bool isLoaded = false;
    QString err = "";

};

class Metadata
{
public:
    Metadata();
    bool readMetadata(bool report, const QString &imageFullPath);

    ulong offsetFullJPG;
    ulong lengthFullJPG;
    ulong offsetThumbJPG;
    ulong lengthThumbJPG;
    ulong offsetSmallJPG;
    ulong lengthSmallJPG;
    int orientation;
    ulong width;
    ulong height;
    QString dateTime;
    QString model;
    QString exposureTime;
    float exposureTimeNum;
    QString aperture;
    float apertureNum;
    QString ISO;
    int ISONum;
    QString focalLength;
    int focalLengthNum;
    QString title;
    QString err;

    QStringList rawFormats;
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
    bool loadImageMetadata(const QFileInfo &fileInfo);
    bool isLoaded(const QString &imageFullPath);
    ulong getOffsetFullJPG(const QString &imageFullPath);
    ulong getLengthFullJPG(const QString &imageFullPath);
    ulong getOffsetThumbJPG(const QString &imageFullPath);
    ulong getLengthThumbJPG(const QString &imageFullPath);
    ulong getOffsetSmallJPG(const QString &imageFullPath);
    ulong getLengthSmallJPG(const QString &imageFullPath);
    ulong getWidth(const QString &imageFullPath);
    ulong getHeight(const QString &imageFullPath);
    QString rpt;

    int getImageOrientation(QString &imageFileName);
    bool getPick(const QString &imageFileName);
    QString getDateTime(const QString &imageFileName);
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
    int getYear(const QString &imageFileName);
    int getMonth(const QString &imageFileName);
    int getDay(const QString &imageFileName);
    QString getErr(const QString &imageFileName);
    void setErr(const QString &imageFileName, const QString &err);
    QString getCopyFileNamePrefix(const QString &imageFileName);
//    bool isMetadata(const QString &imageFileName);

private:
    QFile file;
    QHash<uint, IFDData> ifdDataHash;
    QHash<uint, IFDData>::iterator ifdIter;
    QHash<ulong, QString> exifHash, ifdHash, gpsHash, segCodeHash;
    QHash<QString, ulong> segmentHash;

    // was metadata
    QMap<QString, ImageMetadata> metaCache;
//    Metadata metadata;

    bool report;
    long order;

    void initSupportedFiles();
    void initSegCodeHash();
    void initExifHash();
    void initIfdHash();

    uint get1(QByteArray c);
    ulong get2(QByteArray c);
    ulong get4(QByteArray c);
    float getReal(long offset);
    void readIPTC(ulong offset);
    ulong readIFD(QString hdr, ulong offset);
    QList<ulong> getSubIfdOffsets(ulong subIFDaddr, int count);
//    ulong getExifOffset(ulong offsetIfd0);      //update to use ifdDataHash
    QString getString(ulong offset, ulong length);
    void getSegments(ulong offset);
    bool getDimensions(ulong jpgOffset);

    void reportMetadata();
    void reportIfdDataHash();
    void clearMetadata();

    void formatNikon();
    void formatCanon();
    void formatOlympus();
    void formatSony();
    void formatFuji();
    bool formatJPG();

signals:

public slots:
    void loadFromThread(QFileInfo fileInfo);

private:
    void track(QString fPath, QString msg);
};

#endif // METADATA_H
