#ifndef IMAGEMETADATA_H
#define IMAGEMETADATA_H

#include <QtCore>

class ImageMetadata
{
/*
This structure of all the fields in metadata is used to insert a new item
into "QMap<QString, ImageMetadata> metaCache" in Metadata::loadImageMetadata.
*/
public:
    int row = 0;                // datamodel row
    bool isPicked = false;          //rgh required?
    bool isSearch = false;
    quint32 offsetFullJPG = 0;
    quint32 lengthFullJPG = 0;
    quint32 offsetThumbJPG = 0;
    quint32 lengthThumbJPG = 0;
    quint32 offsetSmallJPG = 0;
    quint32 lengthSmallJPG = 0;

    quint32 xmpSegmentOffset = 0;
    quint32 xmpNextSegmentOffset = 0;
    bool isXmp = false;
    quint32 orientationOffset = 0;
    int orientation = 0;
    int rotationDegrees = 0;            // additional rotation from edit
    uint width = 0;
    uint height = 0;
    QString dimensions = "";
    QDateTime createdDate;
    QString make = "";
    QString model = "";
    QString exposureTime = "";
    float exposureTimeNum = 0.0;
    QString aperture = "";
    float apertureNum = 0.0;
    QString ISO = "";
    int ISONum = 0;
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
};

Q_DECLARE_METATYPE(ImageMetadata)
#endif // IMAGEMETADATA_H
