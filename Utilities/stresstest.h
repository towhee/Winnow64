#ifndef STRESSTEST_H
#define STRESSTEST_H

#include <QtWidgets>
#include <QObject>
#include <QThread>
#include "Main/global.h"
#include "Datamodel/datamodel.h"
#include "File/bookmarks.h"
#include "File/fstree.h"

class StressTest : public QThread
{
    Q_OBJECT
public:
    // fix type for toClass and toFunction
    StressTest(QObject *parent, DataModel *dm, BookMarks *bookmarks, FSTree *fsTree);
    void stop();
    void begin();
    void begin(int msDelay, quint64 lower, quint64 higher);

signals:
    void next(Qt::KeyboardModifiers modifiers);
    void prev(Qt::KeyboardModifiers modifiers);
    void selectFolder(QString path, QString modifier = "None", QString src = "StressTest");
    void selectBookmark(QString path);

protected:
    void run() override;

private:
    DataModel *dm;
    BookMarks *bookmarks;
    FSTree *fsTree;
    QList<QString>bookMarkPaths;
    int bmCount;
    int delay;
    bool isForward;
    int uturnCounter;
    int uturnMax;
    int uturnAmount;
    QElapsedTimer t;
    bool isRandomFolderTime;
    quint64 folderTime;
    quint64 lower;
    quint64 higher;
};

#endif // STRESSTEST_H
