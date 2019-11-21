#ifndef OLYMPUS_H
#define OLYMPUS_H

#include <QtWidgets>
#include "Main/global.h"
#include "Utilities/utilities.h"
#include "Metadata/imagemetadata.h"
#include "Metadata/exif.h"
#include "Metadata/ifd.h"
#include "Metadata/xmp.h"
#include "ImageFormats/Jpeg/jpeg.h"
#include "Metadata/metareport.h"

class Olympus : public QObject
{
    Q_OBJECT

public:
    Olympus();
    bool parse(QFile &file,
               ImageMetadata &m,
               IFD *ifd,
               Exif *exif,
               Jpeg *jpeg,
               bool report,
               QTextStream &rpt,
               QString &xmpString);
private:
    QHash<quint32, QString> olympusMakerHash, olympusCameraSettingsHash;
};

#endif // OLYMPUS_H
