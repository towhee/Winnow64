#ifndef MOV_H
#define MOV_H

#include <QObject>
#include <QIODevice>
#include "Main/global.h"

class MOV : public QObject
{
    Q_OBJECT

public:
    MOV();
    static QDateTime createDate(const QString &filePath);
    /*  The movie's duration in milliseconds from its mvhd atom (moov/mvhd: timescale
        and duration), or -1 when the file has none that can be read. Works for the
        whole ISO-BMFF family (mov, mp4, m4v, 3gp...). Walks top-level boxes itself,
        honouring 64-bit (size 1) and to-end (size 0) boxes, because a camera file
        commonly puts a >4 GB mdat before its moov. The QIODevice overload is the
        parser (tests feed it a QBuffer). */
    static qint64 durationMs(const QString &filePath);
    static qint64 durationMs(QIODevice &dev);
    static void walkAtomTree(const QString &filePath);

private:
    struct Atom {
        QString type;
        quint32 size;
        QList<Atom> children;
    };

    static quint32 readUInt32(QFile &file);
    static bool findAtom(QFile &file, const char *targetAtomType, quint32 &atomSize, qint64 limit = -1);
    static bool readAtomHeader(QFile &file, quint32 &size, char (&type)[4]);
    static void readAtomTree(QFile &file, quint32 maxOffset, QList<Atom> &atomList);
    static void printAtomTree(const QList<Atom> &atomList, const QString &indent = "");

    static bool isDebug;
};

#endif // MOV_H
