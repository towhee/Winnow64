#ifndef DNG_H
#define DNG_H

#include <QtWidgets>
#include "Utilities/utilities.h"
#include "Metadata/imagemetadata.h"
#include "Metadata/exif.h"
#include "Metadata/ifd.h"
#include "ImageFormats/Jpeg/jpeg.h"

class DNG
{
public:
    DNG();
    bool parse(MetadataParameters &p,
               ImageMetadata &m,
               IFD *ifd,
               IPTC *iptc,
               Exif *exif,
               Jpeg *jpeg,
               GPS *gps);
private:
    Utilities u;
};

#endif // DNG_H
