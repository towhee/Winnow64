#ifndef NIKON_H
#define NIKON_H

#include <QtWidgets>
#include "Main/global.h"
#include "Utilities/utilities.h"
#include "Metadata/imagemetadata.h"
#include "Metadata/exif.h"
#include "Metadata/ifd.h"
#include "Metadata/xmp.h"
#include "ImageFormats/Jpeg/jpeg.h"
#include "Metadata/metareport.h"


class Nikon : public QObject
{
    Q_OBJECT

public:
    Nikon();
    bool parse(MetadataParameters &p,
               ImageMetadata &m,
               IFD *ifd,
               Exif *exif,
               Jpeg *jpeg);
private:
    QByteArray nikonDecrypt(QByteArray bData, uint32_t count, uint32_t serial);
    QHash<quint32, QString> nikonMakerHash;
    QHash<QString, QString> nikonLensHash;
};

#endif // NIKON_H
