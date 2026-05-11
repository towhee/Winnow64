#ifndef PNG_H
#define PNG_H

#include <QObject>
#include <QFile>
#include <QDateTime>
#include <QDebug>

#include "Metadata/imagemetadata.h"
#include "Metadata/exif.h"
#include "Metadata/ifd.h"
#include "Metadata/gps.h"

class PNG : public QObject
{
    Q_OBJECT

public:
    PNG();
    bool parse(MetadataParameters &p,
               ImageMetadata &m,
               IFD *ifd,
               Exif *exif,
               GPS *gps);
    static QDateTime createDate(const QString &filePath);

private:
    static bool readChunkHeader(QFile &file, quint32 &size, char (&type)[4]);
    static QByteArray readChunkData(QFile &file, quint32 size);
    static quint32 readUInt32(QFile &file);

    void applyTextKey(ImageMetadata &m, const QString &key, const QString &value);
    bool parseExifChunk(MetadataParameters &p, ImageMetadata &m,
                        IFD *ifd, Exif *exif, GPS *gps,
                        quint32 chunkDataOffset, quint32 chunkDataLength);
    QByteArray zlibInflate(const QByteArray &compressed);
    static double rationalToDouble(const QString &s);
};

#endif // PNG_H
