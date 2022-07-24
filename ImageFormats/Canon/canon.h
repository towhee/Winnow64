#ifndef CANON_H
#define CANON_H

#include <QtWidgets>
#include "Main/global.h"
#include "Utilities/utilities.h"
#include "Metadata/imagemetadata.h"
#include "Metadata/exif.h"
#include "Metadata/ifd.h"
#include "Metadata/xmp.h"
#include "ImageFormats/Jpeg/jpeg.h"
#include "Metadata/metareport.h"

class Canon : public QObject
{
    Q_OBJECT

public:
    Canon();
    bool parse(MetadataParameters &p,
               ImageMetadata &m,
               IFD *ifd,
               Exif *exif,
               Jpeg *jpeg);
private:
    QHash<quint32, QString> canonMakerHash, canonFileInfoHash;
    Utilities u;
};

#endif // CANON_H
