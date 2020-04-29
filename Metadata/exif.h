#ifndef EXIF_H
#define EXIF_H
#include <QtCore>

class Exif
{
public:
    Exif();
    QHash<quint32, QString> hash;
};

#endif // EXIF_H
