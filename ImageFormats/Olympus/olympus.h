#ifndef OLYMPUS_H
#define OLYMPUS_H

#include <QtWidgets>
#include "Utilities/utilities.h"
#include "Metadata/imagemetadata.h"
#include "Metadata/exif.h"
#include "Metadata/ifd.h"
#include "Metadata/xmp.h"
#include "Metadata/gps.h"
#include "ImageFormats/Jpeg/jpeg.h"
#include "Metadata/metareport.h"

class Olympus : public QObject
{
    Q_OBJECT

public:
    Olympus();
    bool parse(MetadataParameters &p,
               ImageMetadata &m,
               IFD *ifd,
               Exif *exif,
               Jpeg *jpeg,
               GPS *gps);
private:
    QHash<quint32, QString> olympusMakerHash,
                            olympusCameraSettingsHash,
                            olympusEquipmentHash;
    Utilities u;
};

#endif // OLYMPUS_H
