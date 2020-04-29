#ifndef GPS_H
#define GPS_H
#include <QtCore>

class GPS
{
public:
    GPS();
    QHash<quint32, QString> hash;
};

#endif // GPS_H
