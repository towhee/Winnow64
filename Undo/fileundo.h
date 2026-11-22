#ifndef FILEUNDO_H
#define FILEUNDO_H

#include <QHash>
#include <QList>
#include <QPair>
#include <QString>
#include <QStringList>
#include <QVector>
#include "Develop/History/historyentry.h"
#include "Undo/undocommands.h"

/*
    An image Edit > Undo can bring back from the trash (Main/mwfileundo.cpp): where its
    files went, and what its delete purged from Winnow's stores (FileOps::onDeleted).
    MW::keepForTrashUndo fills it before the trash; MW::restoreTrashed puts it back.
*/
struct TrashedImage
{
    QString path;                                   // where the image was
    QList<QPair<QString, QString>> files;           // original -> trash, image first
    QHash<QString, QVector<qint64>> collections;    // row key -> collections it was in
    QStringList savedPicks;                         // row keys picked (SavedPicks)
    PathValues pickState;                           // row key -> pick column, rejects too
    bool duplicateOk = false;
    struct History {
        QString key;
        QVector<HistoryEntry> entries;
        int pos = -1;
        QString stamp;
    };
    QList<History> history;
};

#endif // FILEUNDO_H
