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
#include "global.h"

class IFDData
{
public:
    ulong tagType;
    ulong tagCount;
    ulong tagValue;
    short tagReal;
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
    QString aperture;
    QString ISO;
    QString focalLength;
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

private:
    QFile file;
    QHash<uint, IFDData> ifdDataHash;
    QHash<uint, IFDData>::iterator ifdIter;
    QHash<ulong, QString> exifHash, ifdHash, gpsHash;

    bool report;
    short order;

    void initExifHash();
    void initIfdHash();

    ulong get2(QByteArray c);
    ulong get4(QByteArray c);
    float getReal(long offset);
    ulong readIFD(QString hdr, ulong offset);
    QList<ulong> getSubIfdOffsets(ulong subIFDaddr);
    ulong getExifOffset(ulong offsetIfd0);      //update to use ifdDataHash
    QString getString(ulong offset, ulong length);

    void reportMetadata();
    void reportIfdDataHash();
    void clearMetadata();

    void nikon();
    void canon();
    bool jpeg();

signals:

public slots:
};

#endif // METADATA_H
