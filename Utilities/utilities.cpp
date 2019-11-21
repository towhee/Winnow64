#include "utilities.h"

void Utilities::setOpacity(QWidget *widget, qreal opacity)
{
    QGraphicsOpacityEffect * effect = new QGraphicsOpacityEffect(widget);
    effect->setOpacity(opacity);
    widget->setGraphicsEffect(effect);
}

QString Utilities::formatMemory(qulonglong bytes, int precision)
{
    qulonglong x = 1024;
    if (bytes == 0) return "0";
    if (bytes < x) return QString::number(bytes) + " bytes";
    if (bytes < x * 1024) return QString::number((float)bytes / x, 'f', precision) + " KB";
    x *= 1024;
    if (bytes < (x * 1024)) return QString::number((float)bytes / x, 'f', precision) + " MB";
    x *= 1024;
    if (bytes < (x * 1024)) return QString::number((float)bytes / x, 'f', precision) + " GB";
    x *= 1024;
    if (bytes < (x * 1024)) return QString::number((float)bytes / x, 'f', precision) + " TB";
    return "More than TB";
}

QString Utilities::enquote(QString &s)
{
    return QChar('\"') + s + QChar('\"');
}

void Utilities::hideCursor()
{
    QApplication::setOverrideCursor(Qt::BlankCursor);
}

QString Utilities::centeredRptHdr(QChar padChar, QString title)
{
    QString hdr;
    QTextStream rpt;
    rpt.setString(&hdr);
    int hdrWidth = 90;
    int titleWidth = title.size() + 4;
    int padWidth = (hdrWidth - titleWidth) / 2;
    rpt.reset();
    rpt << "\n";
    rpt.setPadChar(padChar);
    rpt.setFieldWidth(padWidth);
    rpt.setFieldAlignment(QTextStream::AlignRight);
    rpt << padChar;
    rpt.reset();
    rpt.setFieldWidth(titleWidth);
    rpt.setFieldAlignment(QTextStream::AlignCenter);
    rpt << title;
    rpt.setPadChar(padChar);
    rpt.setFieldWidth(padWidth);
    rpt.setFieldAlignment(QTextStream::AlignRight);
    rpt << padChar;
    return hdr;
}

// numbers: big endian = "MM" = 0x4D4D   little endian = "II" = 0x4949

int Utilities::get4_1st(QByteArray c)
{
    return static_cast<int>(c[0] >> 4);
}

int Utilities::get4_2nd(QByteArray c)
{
    return static_cast<int>(c[0]&0xF);
}

quint8 Utilities::get8(QByteArray c)
{
    return c[0]&0xFF;
}

quint16 Utilities::get16(QByteArray c, bool isBigEnd)
{
    if (isBigEnd) {
        quint16 x = c[0]&0xFF;
        x = static_cast<quint16>((x << 8) | (c[1]&0xFF));
        return x;
    }
    else {
        quint16 x = c[1]&0xFF;
        x = static_cast<quint16>((x << 8) | (c[0]&0xFF));
        return x;
    }
}

quint32 Utilities::get24(QByteArray c, bool isBigEnd)
{
    if (isBigEnd) {
        quint32 x = c[0]&0xFF;
        x = static_cast<quint32>((x << 8) | (c[1]&0xFF));
        x = static_cast<quint32>((x << 8) | (c[2]&0xFF));
        return x;
    }
    else {
        quint32 x = c[2]&0xFF;
        x = static_cast<quint32>((x << 8) | (c[1]&0xFF));
        x = static_cast<quint32>((x << 8) | (c[0]&0xFF));
        return x;
    }
}

quint32 Utilities::get32(QByteArray c, bool isBigEnd)
{
    if (isBigEnd) {
        quint32 x = c[0]&0xFF;
        x = static_cast<quint32>((x << 8) | (c[1]&0xFF));
        x = static_cast<quint32>((x << 8) | (c[2]&0xFF));
        x = static_cast<quint32>((x << 8) | (c[3]&0xFF));
        return x;
    }
    else {
        quint32 x = c[3]&0xFF;
        x = static_cast<quint32>((x << 8) | (c[2]&0xFF));
        x = static_cast<quint32>((x << 8) | (c[1]&0xFF));
        x = static_cast<quint32>((x << 8) | (c[0]&0xFF));
        return x;
    }
}

quint64 Utilities::get40(QByteArray c, bool isBigEnd)
{
    if (isBigEnd) {
        quint64 x = c[0]&0xFF;
        x = static_cast<quint64>((x << 8) | (c[1]&0xFF));
        x = static_cast<quint64>((x << 8) | (c[2]&0xFF));
        x = static_cast<quint64>((x << 8) | (c[3]&0xFF));
        x = static_cast<quint64>((x << 8) | (c[4]&0xFF));
        return x;
    }
    else {
        quint64 x = c[4]&0xFF;
        x = static_cast<quint64>((x << 8) | (c[3]&0xFF));
        x = static_cast<quint64>((x << 8) | (c[2]&0xFF));
        x = static_cast<quint64>((x << 8) | (c[1]&0xFF));
        x = static_cast<quint64>((x << 8) | (c[0]&0xFF));
        return x;
    }
}

quint64 Utilities::get48(QByteArray c, bool isBigEnd)
{
    if (isBigEnd) {
        quint64 x = c[0]&0xFF;
        x = static_cast<quint64>((x << 8) | (c[1]&0xFF));
        x = static_cast<quint64>((x << 8) | (c[2]&0xFF));
        x = static_cast<quint64>((x << 8) | (c[3]&0xFF));
        x = static_cast<quint64>((x << 8) | (c[4]&0xFF));
        x = static_cast<quint64>((x << 8) | (c[5]&0xFF));
        return x;
    }
    else {
        quint64 x = c[5]&0xFF;
        x = static_cast<quint64>((x << 8) | (c[4]&0xFF));
        x = static_cast<quint64>((x << 8) | (c[3]&0xFF));
        x = static_cast<quint64>((x << 8) | (c[2]&0xFF));
        x = static_cast<quint64>((x << 8) | (c[1]&0xFF));
        x = static_cast<quint64>((x << 8) | (c[0]&0xFF));
        return x;
    }
}

quint64 Utilities::get64(QByteArray c, bool isBigEnd)
{
    if (isBigEnd) {
        quint64 x = c[0]&0xFF;
        x = static_cast<quint64>((x << 8) | (c[1]&0xFF));
        x = static_cast<quint64>((x << 8) | (c[2]&0xFF));
        x = static_cast<quint64>((x << 8) | (c[3]&0xFF));
        x = static_cast<quint64>((x << 8) | (c[4]&0xFF));
        x = static_cast<quint64>((x << 8) | (c[5]&0xFF));
        x = static_cast<quint64>((x << 8) | (c[6]&0xFF));
        x = static_cast<quint64>((x << 8) | (c[7]&0xFF));
        return x;
    }
    else {
        quint64 x = c[7]&0xFF;
        x = static_cast<quint64>((x << 8) | (c[6]&0xFF));
        x = static_cast<quint64>((x << 8) | (c[5]&0xFF));
        x = static_cast<quint64>((x << 8) | (c[4]&0xFF));
        x = static_cast<quint64>((x << 8) | (c[3]&0xFF));
        x = static_cast<quint64>((x << 8) | (c[2]&0xFF));
        x = static_cast<quint64>((x << 8) | (c[1]&0xFF));
        x = static_cast<quint64>((x << 8) | (c[0]&0xFF));
        return x;
    }
}

double Utilities::getReal(QFile &file, quint32 offset, bool isBigEnd)
{
    /*
    In IFD type 5, 10, 11, 12 = rational = real/float
    */
        file.seek(offset);
        quint32 a = get32(file.read(4), isBigEnd);
        quint32 b = get32(file.read(4), isBigEnd);
        if (b == 0) return 0;
        return static_cast<double>(a) / b;
}

QString Utilities::getCString(QFile &file)
{
    quint32 offset = static_cast<quint32>(file.pos());
    uint byte = 1;
    int length = -1;
    while (byte) {
        byte = get8(file.read(1));
        length++;
    }
    if (length) {
        file.seek(offset);
        return(file.read(length));
    }
    else {
        file.seek(offset + 1);
        return "";
    }
}

QString Utilities::getString(QFile &file, quint32 offset, quint32 length)
{
    /*
    In IFD type 2 = string
    */
        file.seek(offset);
        return(file.read(length));
}

QByteArray Utilities::getByteArray(QFile &file, quint32 offset, quint32 length)
{
    file.seek(offset);
    return(file.read(length));
}
