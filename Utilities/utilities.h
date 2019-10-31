#ifndef UTILITIES_H
#define UTILITIES_H

#include <QtWidgets>

class Utilities
{
public:
    Utilities();
    static void setOpacity(QWidget *widget, qreal opacity);
    static QString formatMemory(qulonglong bytes, int precision = 1);
    static QString enquote(QString &s);
    static QString centeredRptHdr(QChar padChar, QString title);
    static quint8 get8(QByteArray c);
    static quint16 get16(QByteArray c, bool bigEndian = true);
    static quint32 get24(QByteArray c, bool bigEndian = true);
    static quint32 get32(QByteArray c, bool bigEndian = true);
    static quint64 get40(QByteArray c, bool bigEndian = true);
    static quint64 get48(QByteArray c, bool bigEndian = true);
    static quint64 get64(QByteArray c, bool bigEndian = true);

public slots:
    static void hideCursor();

};

#endif // UTILITIES_H
