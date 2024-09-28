#ifndef MP4_H
#define MP4_H

#include <QObject>
#include "Main/global.h"

class MP4 : public QObject
{
    Q_OBJECT

public:
    MP4();
    static QDateTime createDate(const QString &filePath);
    static void walkAtomTree(const QString &filePath);

private:
    struct Atom {
        QString type;
        quint32 size;
        QList<Atom> children;
    };

    static quint32 readUInt32(QFile &file);
    static bool findAtom(QFile &file, const char *targetAtomType, quint32 &atomSize);
    static bool readAtomHeader(QFile &file, quint32 &size, char (&type)[4]);
    static void readAtomTree(QFile &file, quint32 maxOffset, QList<Atom> &atomList);
    static void printAtomTree(const QList<Atom> &atomList, const QString &indent = "");

    bool isDebug;
};

#endif // MP4_H
