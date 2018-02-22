#ifndef XMP_H
#define XMP_H

#include <QObject>
#include <QtCore>
#include <QtWidgets>
#include <QtXmlPatterns>

class Xmp : public QObject
{
    Q_OBJECT
public:
    Xmp(QFile &file, ulong &offset, ulong &nextOffset, QObject *parent = nullptr);
    QString getItem(QByteArray item);
    bool setItem(QByteArray item, QByteArray value);
    QString metaAsString();
    void set(QFile &file, ulong &offset, ulong &nextOffset, QByteArray &xmpBA);

signals:

public slots:

private:
    QByteArray xmpBa;
    ulong xmpLength;
    int xmpmetaStart;
    int xmpmetaEnd;

    QByteArray xmpPrefix;
    QByteArray xmpSuffix;
};

#endif // XMP_H
