#ifndef UTILITIES_H
#define UTILITIES_H

#include <QtWidgets>
#include "Utilities/inputdlg.h"
//#include <bitset>

class Utilities
{
public:
    static bool integrityCheck(const QString &path1, const QString &path2);
    static QSize fitScreen(QSize preferred);

    // File/folder path operations
    static QString getDrive(QString path);
    static QString getFileName(QString srcPath);
    static QString getFolderPath(QString fPath);
    static QString replaceFileName(QString srcPath, QString newName);
    static QString replaceSuffix(QString srcPath, QString newSuffix);
    static QStringList getPossibleSidecars(QString srcPath);

    // Get unique
    static void uniqueFolderPath(QString &path, QString delimiter = "_");
    static void uniqueFilePath(QString &path, QString delimiter = "_");
    static void uniqueInList(QString &name, const QStringList &list, QString delimiter = "_");

    static void backup(QString fPath, QString subfolderName);
    static QString inputText(QString title, QString description,
                             QStringList doNotUse, QString input = "");
    static void log(QString function, QString msg);
    static void setOpacity(QWidget *widget, qreal opacity);

    // Format
    static QString formatMemory(qulonglong bytes, int precision = 1);
    static QString enquote(QString &s);
    static QString centeredRptHdr(QChar padChar, QString title);

    // Convert
    static QString stringListToString(QStringList list);
    static QString pixmapToString(QPixmap &pixmap);

    // File variable byte operations
    static void saveByteArrayAsFile(QString fPath, QByteArray &ba);
    static int get4_1st(QByteArray c);
    static int get4_2nd(QByteArray c);
    static quint8 get8(QByteArray c);
    static QByteArray put8(quint8 x);
    static quint16 get16(QByteArray c, bool isBigEnd = true);
    static QByteArray put16(quint16 x, bool isBigEnd = true);
    static quint32 get24(QByteArray c, bool isBigEnd = true);
    static quint32 get32(QByteArray c, bool isBigEnd = true);
    static QByteArray put32(quint32 x, bool isBigEnd = true);
    static quint64 get40(QByteArray c, bool isBigEnd = true);
    static quint64 get48(QByteArray c, bool isBigEnd = true);
    static quint64 get64(QByteArray c, bool isBigEnd = true);
    template<typename T>
    static double getReal(T &io, quint32 offset, bool isBigEnd = true);
    static QByteArray putReal(quint32 x, bool isBigEnd = true);
    template<typename T>
    static double getReal_s(T &io, quint32 offset, bool isBigEnd = true);
    static QString getCString(QFile &file);
    static bool putCString(QFile &file, QString s);
    template<typename T>
    static QString getString(T &io, quint32 offset, quint32 length);
    static QByteArray getByteArray(QFile &file, quint32 offset, quint32 length);
    static int integer(int x);
    static quint32 swapEndian32(quint32 x);

    // Bit operations
    static QBitArray bytes2Bits(QByteArray bytes);
//    static void bytes2Bitset32(QByteArray bytes, std::bitset<32> &bits);
//    static uint bits2Int(QBitArray bits, int length);
//    static uint extractBits(std::bitset<32> &buf, uint n);

    // Formatted strings from binary data
    static void hexFromByteArray(QByteArray &ba, int cols = 0, int start = 0, int end = 0);

public slots:
    static void hideCursor();
};

#endif // UTILITIES_H
