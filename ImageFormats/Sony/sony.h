#ifndef SONY_H
#define SONY_H

#include <QtWidgets>
#include "Main/global.h"
#include "Utilities/utilities.h"
#include "Metadata/imagemetadata.h"
#include "Metadata/exif.h"
#include "Metadata/ifd.h"
#include "Metadata/xmp.h"
//#include "ImageFormats/Jpeg/jpeg.h"
#include "Metadata/metareport.h"

class Sony : public QObject
{
    Q_OBJECT

public:
    Sony();
    bool parse(MetadataParameters &p,
               ImageMetadata &m,
               IFD *ifd,
               Exif *exif);
private:
    QHash<quint32, QString> sonyMakerHash;
};

#endif // SONY_H
