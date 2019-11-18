#ifndef IPTC_H
#define IPTC_H

#include <QtCore>

#include "Utilities/utilities.h"
#include "Metadata/imagemetadata.h"

class IPTC
{
public:
    IPTC();
    void read(QFile &file, quint32 &offset, ImageMetadata &m);
};

#endif // IPTC_H
