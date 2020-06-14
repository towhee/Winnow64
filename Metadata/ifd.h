#ifndef IFD_H
#define IFD_H

#include <QtCore>

#include "Metadata/metareport.h"
#include "Utilities/utilities.h"
#include "Metadata/imagemetadata.h"

struct IFDData
{
    quint32 tagType;
    quint32 tagCount;
    quint32 tagValue;
    short tagReal;
};

class IFD
{
public:
    IFD();
    quint32 readIFD(MetadataParameters &p, ImageMetadata &m, bool isBigEnd = false);
    QList<quint32> getSubIfdOffsets(QFile &file, quint32 subIFDaddr, int count, bool isBigEnd = false);
    quint32 readIFD_B(MetadataParameters &p, ImageMetadata &m, bool isBigEnd = false);
    QList<quint32> getSubIfdOffsets_B(QFile &file, quint32 subIFDaddr, int count, bool isBigEnd = false);
    QHash<uint, IFDData> ifdDataHash;
};

#endif // IFD_H
