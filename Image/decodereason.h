#ifndef DECODEREASON_H
#define DECODEREASON_H

#include <QBuffer>
#include <QByteArray>
#include <QImage>
#include <QImageReader>
#include <QString>

/*
    WHY A QT DECODE FAILED. QImage::load and loadFromData say only true or false; the
    reader they use knows why (unsupported format, damaged data). Asked ON FAILURE ONLY,
    by decoding again with a QImageReader, so the success path is unchanged and the cost
    is paid only by an image that already failed. Used by ImageDecoder, Pixmap and Thumb
    to give their issue-log messages a cause.
*/
namespace DecodeReason {

inline QString fromFile(const QString &path)
{
    QImageReader r(path);
    QImage probe;
    return r.read(&probe) ? QString("decoded on a second try") : r.errorString();
}

inline QString fromData(const QByteArray &data, const char *format)
{
    QBuffer b;
    b.setData(data);
    QImageReader r(&b, format);
    QImage probe;
    return r.read(&probe) ? QString("decoded on a second try") : r.errorString();
}

}   // namespace DecodeReason

#endif // DECODEREASON_H
