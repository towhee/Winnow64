#ifndef IRB_H
#define IRB_H

#include <QtCore>

#include "Utilities/utilities.h"
#include "Metadata/imagemetadata.h"

class IRB
{
public:
    IRB();
    bool read(MetadataParameters &p, ImageMetadata &m);
};

#endif // IRB_H
