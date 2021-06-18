#ifndef IRB_H
#define IRB_H

#include <QtCore>

#include "Main/global.h"
#include "Utilities/utilities.h"
#include "Metadata/imagemetadata.h"

class IRB
{
public:
    IRB();
    void readThumb(MetadataParameters &p, ImageMetadata &m);
    bool foundTifThumb = false;
};

#endif // IRB_H
