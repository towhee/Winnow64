#ifndef UTILITIES_H
#define UTILITIES_H

#include <QtWidgets>
//#include <bitset>

class Utilities
{
public:
    static void setOpacity(QWidget *widget, qreal opacity);
    static QString formatMemory(qulonglong bytes, int precision = 1);
    static QString enquote(QString &s);
    static QString centeredRptHdr(QChar padChar, QString title);
    static void saveByteArrayAsFile(QString fPath, QByteArray &ba);
    static int get4_1st(QByteArray c);
    static int get4_2nd(QByteArray c);
    static quint8 get8(QByteArray c);
    static quint16 get16(QByteArray c, bool isBigEnd = true);
    static quint32 get24(QByteArray c, bool isBigEnd = true);
    static quint32 get32(QByteArray c, bool isBigEnd = true);
    static quint64 get40(QByteArray c, bool isBigEnd = true);
    static quint64 get48(QByteArray c, bool isBigEnd = true);
    static quint64 get64(QByteArray c, bool isBigEnd = true);
    template<typename T>
    static double getReal(T &io, quint32 offset, bool isBigEnd = true);
    template<typename T>
    static double getReal_s(T &io, quint32 offset, bool isBigEnd = true);
    static QString getCString(QFile &file);
    template<typename T>
    static QString getString(T &io, quint32 offset, quint32 length);
    static QByteArray getByteArray(QFile &file, quint32 offset, quint32 length);
    static QBitArray bytes2Bits(QByteArray bytes);
//    static void bytes2Bitset32(QByteArray bytes, std::bitset<32> &bits);
//    static uint bits2Int(QBitArray bits, int length);
//    static uint extractBits(std::bitset<32> &buf, uint n);
public slots:
    static void hideCursor();
};

#endif // UTILITIES_H
