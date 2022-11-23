#ifndef GPS_H
#define GPS_H
#include <QtCore>
#include "Metadata/ifd.h"
#include "Utilities/utilities.h"

class GPS
{
public:
    GPS();
    QString decode(QFile &file,
                   QHash<uint, IFDData> &ifdDataHash,
                   bool isBigEnd,
                   int offset = 0);
    QHash<quint32, QString> hash;
private:
    Utilities u;
};

#endif // GPS_H
