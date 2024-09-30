#include "png.h"
#include <QtEndian>

PNG::PNG()
{
}

bool PNG::readChunkHeader(QFile &file, quint32 &size, char (&type)[4]) {
    if (file.read((char*)&size, 4) != 4) return false;
    size = qFromBigEndian(size);  // Convert from big endian to host byte order
    if (file.read(type, 4) != 4) return false;
    return true;
}

QByteArray PNG::readChunkData(QFile &file, quint32 size) {
    QByteArray data = file.read(size);
    file.seek(file.pos() + 4);  // Skip CRC (4 bytes)
    return data;
}

quint32 PNG::readUInt32(QFile &file) {
    quint32 value;
    file.read((char*)&value, 4);
    return qFromBigEndian(value);  // Convert to host byte order
}

QDateTime PNG::createDate(const QString &filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "Failed to open file" << filePath;
        return QDateTime();
    }

    // PNG Signature (8 bytes)
    QByteArray pngSignature = file.read(8);
    if (pngSignature != QByteArray::fromHex("89504E470D0A1A0A")) {
        qWarning() << "Not a valid PNG file!";
        return QDateTime();
    }

    quint32 chunkSize;
    char chunkType[4];

    while (!file.atEnd()) {
        if (!readChunkHeader(file, chunkSize, chunkType)) {
            qWarning() << "Failed to read chunk header!";
            break;
        }

        QString chunkTypeStr = QString::fromLatin1(chunkType, 4);
        // qDebug() << "PNG::createDate found chunk:" << chunkTypeStr;

        // Check for tEXt or iTXt chunks
        if (chunkTypeStr == "tEXt" || chunkTypeStr == "iTXt") {
            QByteArray chunkData = readChunkData(file, chunkSize);
            QList<QByteArray> fields = chunkData.split('\0');  // Split the key-value pair
            if (fields.size() == 2 && fields[0] == "date:create") {
                QString dateStr = QString::fromLatin1(fields[1]);
                QDateTime createDate = QDateTime::fromString(dateStr, Qt::ISODate);
                if (createDate.isValid()) {
                    // qDebug() << "PNG::creationDate found creation time:" << createDate;
                    return createDate;
                }
            }
        } else {
            // Skip chunk data and CRC
            file.seek(file.pos() + chunkSize + 4);  // Skip chunk data and CRC
        }
    }

    // qWarning() << "Creation time not found in PNG file!";
    return QDateTime();
}
