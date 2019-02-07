#ifndef READASYNC_H
#define READASYNC_H

#include <QtWidgets>
//#include <QMutex>
//#include <QSize>
//#include <QThread>
//#include <QWaitCondition>
#include "Datamodel/datamodel.h"
#include "Test/asynctask.h"

class ReadASync : public QObject
{
    Q_OBJECT

public:
    ReadASync(QObject *parent, DataModel *dm);
    ~ReadASync();
    void doTasks(const QStringList &sourceFiles);

public slots:
    void go(int amount);
    void checkIfDone();
    void announceProgress(bool saved, const QString &message);

private:
    DataModel *dm;
    volatile bool stopped;
    int total;
    int done;
    bool finished;
    QElapsedTimer t;
    QVector<int> chunkSizes(const int size, const int chunkCount);
};

#endif // READASYNC_H
