#include "Test/readmdconcurrent.h"
#include <QtConcurrentRun>

namespace {

// running on a separate thread
void getMetadata(QObject *receiver,
                 volatile bool *stopped,
                 const QStringList &sourceFiles,
                 Metadata *md)
{
    foreach (const QString &source, sourceFiles) {
        if (*stopped) return;
/*        QFile file;
        file.setFileName(source);
        file.open(QIODevice::ReadOnly);
        file.readAll();
        file.close();
        QString message = QObject::tr("Read '%1'").arg(source);*/

        qDebug() << "getMetadata" << source;
        QMetaObject::invokeMethod(receiver, "announceProgress",
                Qt::QueuedConnection, Q_ARG(bool, true),
                Q_ARG(QString, source));
    }
}

} // anonymous namespace

ReadMdConcurrent::ReadMdConcurrent(QObject *parent,
                                   DataModel *dm,
                                   Metadata *md)
{
    this->dm = dm;
    this->md = md;
}

ReadMdConcurrent::~ReadMdConcurrent()
{
}

void ReadMdConcurrent::go(int amount)        // = convertOrCancel
{
    stopped = true;
    if (QThreadPool::globalInstance()->activeThreadCount())
        QThreadPool::globalInstance()->waitForDone();
    QStringList sourceFiles;
//    total = amount;
    total = dm->rowCount();
    for(int row = 0; row < total; ++row) {
        sourceFiles.append(dm->index(row, 0).data(G::PathRole).toString());
    }
    qDebug() << sourceFiles;
    doTasks(sourceFiles);
}

void ReadMdConcurrent::doTasks(const QStringList &sourceFiles) // = convertFiles
{

    t.restart();

    stopped = false;
    total = sourceFiles.count();
    done = 0;
    const QVector<int> sizes = chunkSizes(total, QThread::idealThreadCount());

    int offset = 0;
    foreach (const int chunkSize, sizes) {
//        qDebug() << "sourceFiles.mid(offset, chunkSize)" << sourceFiles.mid(offset, chunkSize);
        QFuture<void> future = QtConcurrent::run(getMetadata, this, &stopped,
                          sourceFiles.mid(offset, chunkSize),
                          md);
        watcher.setFuture(future);
        offset += chunkSize;
    }
    checkIfDone();
}

QVector<int> ReadMdConcurrent::chunkSizes(const int size, const int chunkCount)
{
    Q_ASSERT(size > 0 && chunkCount > 0);
    if (chunkCount == 1)
        return QVector<int>() << size;
//    qDebug() << "chunkCount" << chunkCount;
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

void ReadMdConcurrent::checkIfDone()
{
    int n = QThreadPool::globalInstance()->activeThreadCount();
    qDebug() << "QThreadPool::globalInstance()->activeThreadCount() =" << n;

    if (QThreadPool::globalInstance()->activeThreadCount())
        QTimer::singleShot(500, this, SLOT(checkIfDone()));
    else {
        stopped = true;
        if (done == total) {
            qDebug() << "checkIfDone  Stopped / Done" << stopped  << done;
            qDebug() << "Elapsed ms =" << t.elapsed();
        }
        else {
            qDebug() << done << "so far";
        }
    }
}

void ReadMdConcurrent::announceProgress(bool readFile, const QString &source)
{
    if (stopped)
        return;
    QFileInfo fileInfo(source);
//    emit loadMetadata(fileInfo, true, true, false, true);
    qDebug() << "announceProgress" << source;
    if (readFile)
        ++done;
}

