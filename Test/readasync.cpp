#include "readasync.h"

ReadASync::ReadASync(QObject *parent, DataModel *dm)
{
    this->dm = dm;

}

ReadASync::~ReadASync()
{
}

void ReadASync::go(int amount)        // = convertOrCancel
{
    stopped = true;
    if (QThreadPool::globalInstance()->activeThreadCount())
        QThreadPool::globalInstance()->waitForDone();
    QStringList sourceFiles;
    total = amount;
    for(int row = 0; row < total; ++row) {
        sourceFiles.append(dm->index(row, 0).data(G::PathRole).toString());
    }
    doTasks(sourceFiles);
}

void ReadASync::doTasks(const QStringList &sourceFiles) // = convertFiles
{

    t.restart();

    stopped = false;
    finished = false;
    total = sourceFiles.count();
    done = 0;
    const QVector<int> sizes = chunkSizes(total, QThread::idealThreadCount());

    int offset = 0;
    foreach (const int chunkSize, sizes) {
        ASyncTask *task = new ASyncTask(this,
                                        &stopped,
                                        sourceFiles.mid(offset, chunkSize));
        QThreadPool::globalInstance()->start(task);
        offset += chunkSize;
    }
    checkIfDone();
}

QVector<int> ReadASync::chunkSizes(const int size, const int chunkCount)
{
    Q_ASSERT(size > 0 && chunkCount > 0);
    if (chunkCount == 1)
        return QVector<int>() << size;
    qDebug() << "chunkCount" << chunkCount;
    QVector<int> result(chunkCount, size / chunkCount);
    if (int remainder = size % chunkCount) {
        int index = 0;
        for (int i = 0; i < remainder; ++i) {
            ++result[index];
            ++index;
            index %= chunkCount;
        }
    }
    return result;
}

void ReadASync::checkIfDone()
{
    if (QThreadPool::globalInstance()->activeThreadCount())
        QTimer::singleShot(500, this, SLOT(checkIfDone()));
    else {
        if (done == total) {
            qDebug() << "Finished";
            finished = true;
            qDebug() << "Elapsed ms =" << t.elapsed();
        }
        else {
            qDebug() << done << "so far";
        }
        stopped = true;
    }
}

void ReadASync::announceProgress(bool readFile, const QString &message)
{
    if (stopped)
        return;
//    qDebug() << message;
    if (readFile)
        ++done;
}

