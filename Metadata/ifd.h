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
    quint32 readIFD(MetadataParameters &p, bool isBigEnd = false);
    QList<quint32> getSubIfdOffsets(QFile &file, quint32 subIFDaddr, int count, bool isBigEnd = false);
    quint32 readIFD_B(MetadataParameters &p, bool isBigEnd = false);
    bool writeIFDCount(MetadataParameters &p, ImageMetadata &m, quint16 count);
    bool writeIFDItem(MetadataParameters &p, ImageMetadata &m,
                      quint16 tagId, quint16 tagType, quint32 tagCount, quint32 tagValue);
    QList<quint32> getSubIfdOffsets_B(QFile &file, quint32 subIFDaddr, int count, bool isBigEnd = false);
    QHash<uint, IFDData> ifdDataHash;
    QMutex mutex;
};

#endif // IFD_H
