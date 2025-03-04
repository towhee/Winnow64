#ifndef PANASONIC_H
#define PANASONIC_H

#include <QtWidgets>
#include "Utilities/utilities.h"
#include "Metadata/imagemetadata.h"
#include "Metadata/exif.h"
#include "Metadata/ifd.h"
#include "Metadata/xmp.h"
#include "ImageFormats/Jpeg/jpeg.h"
#include "Metadata/metareport.h"

class Panasonic : public QObject
{
    Q_OBJECT

public:
    Panasonic();
    bool parse(MetadataParameters &p,
               ImageMetadata &m,
               IFD *ifd,
               Exif *exif,
               Jpeg *jpeg);
private:
    QHash<quint32, QString> panasonicMakerHash;
    Utilities u;
};

#endif // PANASONIC_H
