#ifndef METADATA_H
#define METADATA_H

#include <QtWidgets>
#include <QtCore>
//#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
//#include <QtXmlPatterns>
//#endif
//#include <iostream>
//#include <iomanip>
//#include "Cache/tshash.h"
#include "Metadata/exif.h"
#include "Metadata/ifd.h"
#include "Metadata/irb.h"
#include "Metadata/iptc.h"
#include "Metadata/gps.h"
#include "Metadata/metareport.h"
#include "Utilities/utilities.h"
#include "Metadata/imagemetadata.h"
#include "Metadata/ExifTool.h"
#include "xmp.h"

#include "ui_metadatareport.h"

//#ifdef Q_OS_WIN
// rgh remove heic
#include "ImageFormats/Heic/heic.h"
//#endif

#include "ImageFormats/Jpeg/jpeg.h"
#ifdef Q_OS_MAC
#include "ImageFormats/Jpeg/jpeg2.h"
#include "ImageFormats/Jpeg/jpgdecoder.h"
#include "ImageFormats/Jpeg/jpegturbo.h"
#endif
#include "ImageFormats/Canon/canon.h"
#include "ImageFormats/Canon/canoncr3.h"
#include "ImageFormats/Dng/dng.h"
#include "ImageFormats/Fuji/fuji.h"
#include "ImageFormats/Nikon/nikon.h"
#include "ImageFormats/Olympus/olympus.h"
#include "ImageFormats/Panasonic/panasonic.h"
#include "ImageFormats/Sony/sony.h"
#include "ImageFormats/Tiff/tiff.h"
#include "ImageFormats/Video/mov.h"
#include "ImageFormats/Png/png.h"
#include "ImageFormats/Video/mp4.h"

class Metadata : public QObject
{
    Q_OBJECT
public:
    Metadata(QObject *parent = nullptr);
    bool readMetadata(bool report, const QString &path, QString source);
    QString readExifToolTag(QString fPath, QString tag);
//    bool writeXMP(QStringList &paths, const QString tag, const QString tagValue);
    QString diagnostics(QString fPath);
    void reportMetadata();
    void testNewFileFormat(const QString &path);
    bool parseSidecar();
    QString sidecarPath(QString fPath);
    inline QString b(bool x)
    {
        if (x == true)  return "true";
        else return "false";
    }
    inline QString d(QDateTime x){return x.toString("yyyy-MM-dd hh:mm:ss");}

    /* The ImageMetadata class struct has all the fields that are stored in the
       datamodel.  The various file parse routines populate ImageMetadata.
       ie: m.aperture = (a value from the raw file).  The DataModel class adds this
       information from each file to the model.  The ImageMetadata struct is defined
       in imagemetadata.h.
    */
    ImageMetadata m;

    /* The MetadataParameters class struct has the information that is passed from this
       function to the parsing class for the file type: file, buffer, offset, lookup hash,
       report, hdr, xmpString, inclNonEssential.  This condenses all this info so the
       funcion pass param is neater.  The MetadataParameters class struct is defined
       in imagemetadata.h.
    */
    MetadataParameters p;

//    bool thumbUnavailable;                  // no embedded thumb
//    bool imageUnavailable;                  // no embedded preview

    QStringList hasJpg;
    QStringList hasHeic;
    QStringList sidecarFormats;
    QStringList canEmbedThumb;
    QStringList hasMetadataFormats;
    QStringList rotateFormats;
    QStringList embeddedICCFormats;
    QStringList internalXmpFormats;
    QStringList xmpWriteFormats;
    QStringList iccFormats;
    QStringList noMetadataFormats;
    QStringList supportedFormats;
    QStringList videoFormats;
    QStringList earlyNikons;

    QStringList ratings;
    QStringList labels;

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

    // load failure status
    enum LoadStatus {
        Success,
        InstanceClash,
        NullPath,
        DoesNotExist,
        AlreadyOpen,
        ParseFailed
    };
    QStringList loadStatusText {
        "Success",
        "Instance clash",
        "Null file path",
        "File does not exist",
        "File is already open",
        "Metadata parsing failure"
    };

    void clearMetadata();
    bool writeXMP(const QString &imageFileName, QString src = "Undefined");
    static void writeOrientation(QString fPath, QString orientationNumber);


    QFile file;
    bool okToReadXmp;
    bool readEssentialMetadata;
    bool readNonEssentialMetadata;
    bool foundTifThumb;

    QString nikonLensCode;

    void missingThumbnailWarning();
    QString fileCategory(QString &path, QStringList &paths);

private:
    QMutex mutex;
    // Exif
    Exif *exif = nullptr;
    GPS *gps = nullptr;
    IFD *ifd = nullptr;
    IRB *irb = nullptr;
    IPTC *iptc = nullptr;

    // formats
    Jpeg *jpeg = nullptr;
    Canon *canon = nullptr;
    CanonCR3 *canonCR3 = nullptr;
    DNG *dng = nullptr;
    Fuji *fuji = nullptr;
    Nikon *nikon = nullptr;
    Olympus *olympus = nullptr;
    Panasonic *panasonic = nullptr;
    Sony *sony = nullptr;
    Tiff *tiff = nullptr;
//#ifdef Q_OS_WIN
    // rgh remove heic
    Heic *heic = nullptr;
//#endif

    // hash
    QHash<uint, IFDData> ifdDataHash;
    QHash<uint, IFDData>::iterator ifdIter;
    QHash<quint32, QString> exifHash, ifdHash, gpsHash, segCodeHash,
        nikonMakerHash, sonyMakerHash, fujiMakerHash, canonMakerHash,
        panasonicMakerHash, canonFileInfoHash;
    QHash<QString, quint32> segmentHash;
    QHash<QString, QString> nikonLensHash;
    QHash<int, QString> sonyAFAreaModeHash;
    QHash<int, QString> orientationDescription;
    QHash<int,int> orientationToDegrees;
    QHash<int,int> orientationFromDegrees;

    QString exifToolPath;
    QString reportString;
    quint32 order;

    void initSupportedLabelsRatings();
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

    quint32 findInFile(QString s, quint32 offset, quint32 range);
    void verifyEmbeddedJpg(quint32 &offset, quint32 &length);
    int getNewOrientation(int orientation, int rotation);

    bool parseCanon();
    bool parseCanonCR3();
    bool parseDNG();
    bool parseFuji();
    bool parseHEIF();
    bool parseJPG(quint32 startOffset);
    bool parseNikon();
    bool parseOlympus();
    bool parsePanasonic();
    bool parseSony();
    bool parseTIF();
//    bool parseSidecar();

signals:

public slots:
    bool loadImageMetadata(const QFileInfo &fileInfo,
                           int instance,
                           bool essential = true,
                           bool nonEssential = true,
                           bool isReport = false,
                           bool isLoadXmp = false,
                           QString source = "",
                           bool isRemote = false
            );

};

#endif // METADATA_H
