#ifndef NIKON_H
#define NIKON_H

#include <QtWidgets>
#include "Utilities/utilities.h"
#include "Metadata/imagemetadata.h"
#include "Metadata/exif.h"
#include "Metadata/ifd.h"
#include "Metadata/xmp.h"
#include "Metadata/gps.h"
#include "ImageFormats/Jpeg/jpeg.h"
#include "Metadata/metareport.h"
// req'd to report embedded jpeg
#include "Metadata/iptc.h"
#include "Metadata/gps.h"


class Nikon : public QObject
{
    Q_OBJECT

public:
    Nikon();
    bool parse(MetadataParameters &p,
               ImageMetadata &m,
               IFD *ifd,
               Exif *exif,
               Jpeg *jpeg,
               GPS *gps);
private:
    QByteArray nikonDecrypt(QByteArray bData, uint32_t count, uint32_t serial);
    QHash<quint32, QString> nikonMakerHash;
    QHash<QString, QString> nikonLensHash;
    Utilities u;
};

#endif // NIKON_H
