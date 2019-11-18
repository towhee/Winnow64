#ifndef EXIF_H
#define EXIF_H
#include <QtCore>

class Exif
{
public:
    Exif();
    QHash<quint32, QString> hash;
//    QHash<uint, IFDData> ifdDataHash;
};

#endif // EXIF_H
