#ifndef IFD_H
#define IFD_H

#include <QtCore>

#include "Metadata/metareport.h"
#include "Utilities/utilities.h"
#include "Metadata/imagemetadata.h"

struct IFDData_
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
    quint32 readIFD(QFile &file, quint32 &offset, ImageMetadata &m,
                           QHash<quint32, QString> &hash,
                           bool report, QTextStream &rpt, QString &hdr,
                           bool isBigEnd = false);
    QList<quint32> getSubIfdOffsets(QFile &file, quint32 subIFDaddr,
                                    int count, bool isBigEnd = false);
    QHash<uint, IFDData_> ifdDataHash;
};

#endif // IFD_H
