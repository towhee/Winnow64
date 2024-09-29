#ifndef PNG_H
#define PNG_H

#include <QObject>
#include <QFile>
#include <QDateTime>
#include <QDebug>

class PNG : public QObject
{
    Q_OBJECT

public:
    PNG();
    static QDateTime createDate(const QString &filePath);

private:
    static bool readChunkHeader(QFile &file, quint32 &size, char (&type)[4]);
    static QByteArray readChunkData(QFile &file, quint32 size);
    static quint32 readUInt32(QFile &file);
};

#endif // PNG_H
