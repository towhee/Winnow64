#ifndef MOV_H
#define MOV_H

#include <QObject>
#include "Main/global.h"

class MOV : public QObject
{
    Q_OBJECT

public:
    MOV();
    static QDateTime createDate(const QString &filePath);
    static void walkAtomTree(const QString &filePath);

private:
    struct Atom {
        QString type;
        quint32 size;
        QList<Atom> children;
    };

    static quint32 readUInt32(QFile &file);
    // static bool findAtom(QFile &file, const char *targetAtomType, quint32 &atomSize);
    static bool findAtom(QFile &file, const char *targetAtomType, quint32 &atomSize, qint64 limit = -1);
    static bool readAtomHeader(QFile &file, quint32 &size, char (&type)[4]);
    static void readAtomTree(QFile &file, quint32 maxOffset, QList<Atom> &atomList);
    static void printAtomTree(const QList<Atom> &atomList, const QString &indent = "");

    static bool isDebug;
};

#endif // MOV_H
