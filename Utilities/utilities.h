#ifndef UTILITIES_H
#define UTILITIES_H

#include <QtWidgets>
#include "Utilities/inputdlg.h"
#ifdef Q_OS_WIN
#include "Utilities/win.h"
#endif
#ifdef Q_OS_MAC
#include <sys/stat.h>
#endif

class Utilities
{
public:
    static bool integrityCheck(const QString &path1, const QString &path2);
    static QSize fitScreen(QSize preferred);

    // File/folder path operations
    static QStringList listFromPath(QString path);
    static QString getDriveName(QString path);
    static QString getDrive(QString path);
    static QString getFileName(QString srcPath);
    static QString getFileBase(QString srcPath);
    static QString getSuffix(QString srcPath);
    static QString getFolderPath(QString fPath);
    static QString assocXmpPath(QString fPath);
    static QString replaceFileName(QString srcPath, QString newName);
    static QString replaceSuffix(QString srcPath, QString newSuffix);
    static QStringList getSidecarPaths(QString srcPath);
    static quint32 subFolderTreeCount(QString rootFolderPath);
    /* excludeDirs: folder paths whose subtree is not walked at all -- the excluded
       folder and everything under it is neither returned nor descended into. */
    static quint32 subFolderTree(const QString &rootFolderPath, QStringList &outSubdirs,
                                 const QStringList &excludeDirs = QStringList());

    // File status
    static bool isLocked(const QString& fPath);

    /* Can Winnow write beside the images in this folder?  Use this INSTEAD of
       QStorageInfo::isReadOnly(), which cannot answer the question on macOS: the
       system volume is a sealed read-only snapshot and /Users is a firmlink into
       the writable Data volume, but QStorageInfo matches mount points by path
       prefix, so every path under /Users resolves to "/" and reports read-only.
       access(W_OK) honours both the directory permissions and the mount flags, so
       a locked SD card and a read-only archive mount still answer correctly. */
    static bool folderIsWritable(const QString &dirPath);

    // Get unique
    static void uniqueFolderPath(QString &path, QString delimiter = "_");
    static void uniqueFilePath(QString &path, QString delimiter = "_");
    static void uniqueInList(QString &name, const QStringList &list, QString delimiter = "_");

    static bool comboBoxContainsText(QComboBox* comboBox, const QString& text);
    static bool clipboardHasUrls();

    static bool backup(QString fPath, QString subfolderName);
    static QString inputText(QString title, QString description,
                             QStringList doNotUse, QString input = "");
    static void log(QString function, QString msg);
    static void clearLog();

    static void setOpacity(QWidget *widget, qreal opacity);

    /*  Make every button in box at least as wide as its label. On macOS, under the
        global stylesheet (5 px padding, 100 px minimum), a QMessageBox gives a button
        LESS than its own size hint -- "Remove from Catalog" got 128 px for 129 px of
        text and was clipped at both ends. Measured after polishing, so the stylesheet's
        font is the one counted. */
    static void fitMessageBoxButtons(QMessageBox *box);
    /*  Apply fitMessageBoxButtons to EVERY QMessageBox the app shows, including the
        static QMessageBox::question / warning / ... calls, which never hand out the box.
        An event filter on the application that acts on QEvent::Show. Call once, after
        the QApplication exists. */
    static void installMessageBoxButtonFitter(QCoreApplication *app);

    // Format
    static QString formatMemory(qulonglong bytes, int precision = 1, bool useBinary = true);
    static QString enquote(QString &s);
    static QString centeredRptHdr(QChar padChar, QString title, int width = 180);
    // Format an integer with locale thousands separators, falling back to
    // `*`-fill (width - 1 chars) if the formatted value would exceed the
    // column width — used by tabular reports to keep adjacent columns aligned
    // when a value is anomalously large.
    static QString fitNumber(qint64 v, int width);
    static QString fitNumber(quint64 v, int width);

    // Window
    static bool isScreenValid(const QScreen *screen);

    // keyboard
    static const Qt::KeyboardModifiers modifiersMask;
    static bool modifiers(Qt::KeyboardModifiers modifiers, Qt::KeyboardModifiers val);

    // Convert
    static QString stringListToString(QStringList list);
    static QString pixmapToString(QPixmap &pixmap);
    static QString pngToString(QString fPath);

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

    // Memory
    static quint64 qvariantBytes(QVariant data);

    // Bit operations
    static QBitArray bytes2Bits(QByteArray bytes);
//    static void bytes2Bitset32(QByteArray bytes, std::bitset<32> &bits);
//    static uint bits2Int(QBitArray bits, int length);
//    static uint extractBits(std::bitset<32> &buf, uint n);

    // Formatted strings from binary data
    static void hexFromByteArray(QByteArray &ba, int cols = 0, int start = 0, int end = 0);

public slots:
    static void hideCursor();

private:
};

#endif // UTILITIES_H
