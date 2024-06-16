#ifndef IMAGEMETADATA_H
#define IMAGEMETADATA_H

#include <QtCore>
#include "Main/global.h"

class ImageMetadata
{
/*
    This structure, of all the fields in metadata, is used to insert a new item
    into the datamodel dm->addMetadataForItem(ImageMetadata m) and to pass metadata
    to other classes such as the image parsing routines.

    The order = order set in datamodel
*/
public:
    int row = 0;                            // datamodel row
    int instance = 0;                       // incremented each time datamodel reloaded for new folder
    QString fPath = "";
    // roles for G::PathColumn in datamodel
    QString toolTipRole = "";
    bool cacheRole = false;
    bool dupHideRawRole = false;

    QString currRootFolder = "";
    QString fName = "";
    QDateTime createdDate;
    QDateTime modifiedDate;
    QString year = "";
    QString day = "";
    bool pick = false;
    bool ingested = false;
    bool video = false;
    bool metadataAttempted = false;         // all metadata except thumb
    bool metadataLoaded = false;            // all metadata except thumb
    bool isEmbeddedThumbMissing = false;
    bool isThumbLoaded = false;             // refers to thumb only

    QStringList err;

    bool isSearch = false;
    QString type = "";
    QString ext = "";
    // end of file system info, rest requires pulling metadata from image file
    int size = 0;
    uint permissions = 0;                   // file permissions (QFileDevice::Permissions)
    bool isReadWrite = true;                // Read/Write permission
    int width = 0;                          // width of raw image
    int height = 0;                         // height of raw image
    QString dimensions = "";
    int widthPreview = 0;                   // width of preview (ImageView image width)
    int heightPreview = 0;                  // height of preview (ImageView image height)
    float megapixels = 0;
    int loadMsecPerMp;
    float aspectRatio = 1;
    int orientation = 0;
    quint32 orientationOffset = 0;
    int rotationDegrees = 0;                // additional rotation from edit

    int _orientation = 0;                   // original value
    int _rotationDegrees = 0;               // original value

    QString aperture = "";
    double apertureNum = 0.0;
    QString exposureTime = "";
    double exposureTimeNum = 0.0;
    QString ISO = "";
    int ISONum = 0;
    double exposureCompensationNum = 0.0;
    QString exposureCompensation = "";
    QString make = "";
    QString model = "";
    QString lens = "";
    QString focalLength = "";
    int focalLengthNum = 0;
    QString duration = "";
    QString cameraSN = "";
    QString lensSN = "";
    int focusX = 0;
    int focusY = 0;
    int shutterCount = 0;
    QString gpsCoord;
    QString shootingInfo = "";
    QStringList keywords;

    QString title = "";
    QString creator = "";
    QString copyright = "";
    QString email = "";
    QString url = "";
    QString rating = "";
    QString label = "";

    QString _title = "";                    // original value
    QString _creator = "";                  // original value
    QString _copyright = "";                // original value
    QString _email = "";                    // original value
    QString _url = "";                      // original value
    QString _rating = "";                   // original value
    QString _label = "";                    // original value

    QByteArray nikonLensCode = nullptr;

    quint32 offsetFull = 0;
    quint32 lengthFull = 0;
    quint32 offsetThumb = 0;
    quint32 lengthThumb = 0;
    // reqd for TIFF
    int thumbFormat = G::ImageFormat::Tif;  // G::ImageFormat::Jpg if in IRB
    int widthThumb = 0;                     // width of thumbnail
    int heightThumb = 0;                    // height of thumbnail
    int samplesPerPixel = 0;

    bool isBigEnd = false;
    quint32 ifd0Offset = 0;
    quint32 xmpSegmentOffset = 0;
    quint32 xmpSegmentLength = 0;
    bool isXmp = false;
    quint32 iccSegmentOffset = 0;
    quint32 iccSegmentLength = 0;
    QByteArray iccBuf;
    QString iccSpace;
    QString parseSource;
    QString parseFailure;
    enum Parse {
        Success,
        BadStartToFile,
        MissingEXIF,
        EndianOrderNotFound
    } parseStatus;
    QString searchStr = "";
    bool compare = false;
};
Q_DECLARE_METATYPE(ImageMetadata)

class MetadataParameters
{
/*
    This structure holds the parameters that are passed from Metadata to each of the
    image file parsing classes ie parseSony.
*/
public:
    QString fPath;
    QFile file;
    // int row;
    int instance;
    QBuffer buf;
    quint32 offset;
    QHash<quint32, QString> *hash;
    bool report;
    QTextStream rpt;
    QString hdr;
    QString xmpString;
    bool inclNonEssential;
};
//Q_DECLARE_METATYPE(MetadataParameters)

#endif // IMAGEMETADATA_H
