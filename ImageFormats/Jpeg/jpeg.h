#ifndef JPEG_H
#define JPEG_H

#include <QtWidgets>
#include "Main/global.h"
#include "Utilities/utilities.h"
#include "Metadata/imagemetadata.h"
#include "Metadata/exif.h"
#include "Metadata/ifd.h"
#include "Metadata/iptc.h"
#include "Metadata/xmp.h"
#include "Metadata/metareport.h"

class Jpeg : public QObject
{
    Q_OBJECT

public:
    Jpeg();
    bool parse(MetadataParameters &p,
               ImageMetadata &m,
               IFD *ifd,
               IPTC *iptc,
               Exif *exif);
    
    void initSegCodeHash();
    void getJpgSegments(MetadataParameters &p, ImageMetadata &m);
    bool getDimensions(MetadataParameters &p, ImageMetadata &m);

    QHash<QString, quint32> segmentHash;

private:

    QHash<quint32, QString> segCodeHash;
    quint32 order;
    QString err;
    bool isXmp;
};

#endif // JPEG_H
