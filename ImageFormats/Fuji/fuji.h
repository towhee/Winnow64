#ifndef FUJI_H
#define FUJI_H

#include <QtWidgets>
#include "Main/global.h"
#include "Utilities/utilities.h"
#include "Metadata/imagemetadata.h"
#include "Metadata/exif.h"
#include "Metadata/ifd.h"
#include "Metadata/xmp.h"
#include "ImageFormats/Jpeg/jpeg.h"
#include "Metadata/metareport.h"

class Fuji
{
public:
    Fuji();
    bool parse(QFile &file,
               ImageMetadata &m,
               IFD *ifd,
               Exif *exif,
               Jpeg *jpeg,
               bool report,
               QTextStream &rpt,
               QString &xmpString);
private:
    QHash<quint32, QString> fujiMakerHash;
};

#endif // FUJI_H
