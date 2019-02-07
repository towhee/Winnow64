#include "readsync.h"

ReadSync::ReadSync(QObject *parent, DataModel *dm)
{
    this->dm = dm;
    restart = false;
    abort = false;
}

ReadSync::~ReadSync()
{
    mutex.lock();
    abort = true;
    condition.wakeOne();
    mutex.unlock();
    wait();
}

void ReadSync::go(int amount)
{
    total = amount;
    start(TimeCriticalPriority);
}

void ReadSync::run()
{
    QElapsedTimer t;
    t.start();
    for(int row = 0; row < total; ++row) {
        QString fPath = dm->index(row, 0).data(G::PathRole).toString();
        qDebug() << fPath;
        QFileInfo fileInfo(fPath);
        QFile file;
        file.setFileName(fPath);
        file.open(QIODevice::ReadOnly);
        file.readAll();
        file.close();
    }
    qDebug() << "Elapsed ms =" << t.elapsed();
}
