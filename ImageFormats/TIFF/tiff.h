#ifndef TIFF_H
#define TIFF_H

#include <QtWidgets>
#include "Main/global.h"
#include "Utilities/utilities.h"
#include "Metadata/imagemetadata.h"
#include "Metadata/exif.h"
#include "Metadata/ifd.h"
#include "Metadata/xmp.h"
#include "ImageFormats/Jpeg/jpeg.h"
#include "Metadata/metareport.h"

class Tiff : public QObject
{
    Q_OBJECT

public:
    Tiff();
    bool parse(QFile &file,
               ImageMetadata &m,
               IFD *ifd,
               IPTC *iptc,
               Exif *exif,
               Jpeg *jpeg,
               bool report,
               QTextStream &rpt,
               QString &xmpString);
private:
//    QHash<quint32, QString> sonyMakerHash;
};

#endif // TIFF_H
