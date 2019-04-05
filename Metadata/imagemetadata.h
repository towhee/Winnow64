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
    int row;                // datamodel row
    bool isPicked;          //rgh required?
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
    bool metadataLoaded = false;        // all metadata except thumb
    bool isThumbLoaded;                 // refers to thumb only
    bool thumbUnavailable = false;      // no embedded thumb
    bool imageUnavailable = false;      // no embedded preview
    QString err = "";
};

Q_DECLARE_METATYPE(ImageMetadata)
#endif // IMAGEMETADATA_H
