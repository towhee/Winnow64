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
*/
public:
    int row = 0;                            // datamodel row
    QString fPath = "";
    QString fName = "";
    bool refine = false;
    bool pick = false;
    bool isPicked = false;                  //rgh required?
    bool ingested = false;
    bool isSearch = false;
    QString rating = "";
    QString label = "";
    QString type = "";
    int size = 0;
    int width = 0;                          // width of raw image
    int height = 0;                         // height of raw image
    QString dimensions = "";
    QDateTime createdDate;
    QDateTime modifiedDate;
    int year = 0;
    int month = 0;
    int day = 0;
    float megapixels = 0;
    quint32 orientationOffset = 0;
    int orientation = 0;
    int rotationDegrees = 0;                // additional rotation from edit
    QString aperture = "";
    double apertureNum = 0.0;
    QString exposureTime = "";
    double exposureTimeNum = 0.0;
    QString ISO = "";
    int ISONum = 0;
    double exposureCompensationNum = 0.0;
    QString exposureCompensation = "0";
    QString make = "";
    QString model = "";
    QString lens = "";
    QString focalLength = "";
    int focalLengthNum = 0;
    QString shootingInfo = "";
    QString title = "";
    QString creator = "";
    QString copyright = "";
    QString email = "";
    QString url = "";
    QString cameraSN = "";
    QString lensSN = "";
    ulong shutterCount = 0;
    QString copyFileNamePrefix = "";        // rgh do we need?

    QString _rating = "";                   // original value
    QString _label = "";                    // original value
    QString _title = "";                    // original value
    QString _creator = "";                  // original value
    QString _copyright = "";                // original value
    QString _email = "";                    // original value
    QString _url = "";                      // original value
    int _orientation = 0;                   // original value
    int _rotationDegrees = 0;               // original value

    bool metadataLoaded = false;            // all metadata except thumb
    bool isThumbLoaded = false;             // refers to thumb only
    bool thumbUnavailable = false;          // no embedded thumb
    bool imageUnavailable = false;          // no embedded preview
    QStringList err;
    QString searchStr = "";
    QByteArray nikonLensCode = nullptr;

    quint32 offsetFull = 0;
    quint32 lengthFull = 0;
    int widthFull = 0;                      // width of preview (ImageView image width)
    int heightFull = 0;                     // height of preview (ImageView image height)
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
    quint32 iccSegmentOffset;
    quint32 iccSegmentLength;
    QByteArray iccBuf;
    QString iccSpace;
    QString parseSource;
    int loadMsecPerMp;
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
