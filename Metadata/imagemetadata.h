#ifndef IMAGEMETADATA_H
#define IMAGEMETADATA_H

#include <QtCore>

class ImageMetadata
{
/*
This structure of all the fields in metadata is used to insert a new item
into the datamodel dm->addMetadataForItem(ImageMetadata m).
*/
public:
    int row = 0;                            // datamodel row
    bool isBigEnd = false;
    bool isPicked = false;                  //rgh required?
    bool isSearch = false;
    quint32 offsetFull = 0;
    quint32 lengthFull = 0;
    int widthFull = 0;
    int heightFull = 0;
    quint32 offsetThumb = 0;
    quint32 lengthThumb = 0;
//    quint32 offsetSmall = 0;
//    quint32 lengthSmall = 0;

    // need for TIFF
//    int bitsPerSample = 0;
//    int photoInterp = 0;
    int samplesPerPixel = 0;
//    int compression = 0;
//    quint32 stripByteCounts = 0;
//    int planarConfiguration = 1;

    quint32 ifd0Offset = 0;
    quint32 xmpSegmentOffset = 0;
    quint32 xmpNextSegmentOffset = 0;
    bool isXmp = false;
    quint32 iccSegmentOffset;
    quint32 iccSegmentLength;
    QByteArray iccBuf;
    QString iccSpace;
    quint32 orientationOffset = 0;
    int orientation = 0;
    int rotationDegrees = 0;                // additional rotation from edit
    int width = 0;
    int height = 0;

    QString dimensions = "";
    QDateTime createdDate;
    QString make = "";
    QString model = "";
    QString exposureTime = "";
    double exposureTimeNum = 0.0;
    QString aperture = "";
    double apertureNum = 0.0;
    QString ISO = "";
    int ISONum = 0;
    double exposureCompensationNum = 0.0;
    QString exposureCompensation = "0";
    QString focalLength = "";
    int focalLengthNum = 0;
    QString shootingInfo = "";
    QString title = "";
    QString _title = "";                     // original value
    QString rating = "";
    QString _rating = "";                    // original value
    QString label = "";
    QString _label = "";                     // original value
    QString lens = "";
    QString creator = "";
    QString _creator = "";                   // original value
    QString copyright = "";
    QString _copyright = "";                 // original value
    QString email = "";
    QString _email = "";                     // original value
    QString url = "";
    QString _url = "";                       // original value
    QString cameraSN = "";
    QString lensSN = "";
    ulong shutterCount = 0;
    int year = 0;
    int month = 0;
    int day = 0;
    QString copyFileNamePrefix = "";
    bool metadataLoaded = false;            // all metadata except thumb
    bool isThumbLoaded = false;             // refers to thumb only
    bool thumbUnavailable = false;          // no embedded thumb
    bool imageUnavailable = false;          // no embedded preview
    QString err = "";
    QString searchStr = "";
    QByteArray nikonLensCode = nullptr;
};
Q_DECLARE_METATYPE(ImageMetadata)

class MetadataParameters
{
/*
This structure holds the parameters that are passed from Metadata to each of the
image file parsing classes ie parseSony.
*/
public:
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

#endif // IMAGEMETADATA_H
