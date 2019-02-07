#include "Test/readmdconcurrent.h"
#include <QtConcurrentRun>

namespace {

void getMetadata(QObject *receiver, volatile bool *stopped,
                 const QStringList &sourceFiles)
{
    foreach (const QString &source, sourceFiles) {
        if (*stopped)
            return;
        QFileInfo fileInfo(source);
        QFile file;
        file.setFileName(source);
        file.open(QIODevice::ReadOnly);
        file.readAll();
        file.close();
        QString message = QObject::tr("Read '%1'").arg(source);
        QMetaObject::invokeMethod(receiver, "announceProgress",
                Qt::QueuedConnection, Q_ARG(bool, true),
                Q_ARG(QString, message));
    }
}

} // anonymous namespace

ReadMdConcurrent::ReadMdConcurrent(QObject *parent, DataModel *dm)
{
    this->dm = dm;
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
    total = amount;
    for(int row = 0; row < total; ++row) {
        sourceFiles.append(dm->index(row, 0).data(G::PathRole).toString());
    }
    doTasks(sourceFiles);
}

void ReadMdConcurrent::doTasks(const QStringList &sourceFiles) // = convertFiles
{

    t.restart();

    stopped = false;
    finished = false;
    total = sourceFiles.count();
    done = 0;
    const QVector<int> sizes = chunkSizes(total, QThread::idealThreadCount());

    int offset = 0;
    foreach (const int chunkSize, sizes) {

        QtConcurrent::run(getMetadata, this, &stopped,
                sourceFiles.mid(offset, chunkSize));


//        ASyncTask *task = new ASyncTask(this,
//                                        &stopped,
//                                        sourceFiles.mid(offset, chunkSize));
//        QThreadPool::globalInstance()->start(getMetadata);
        offset += chunkSize;
    }
    checkIfDone();
}

QVector<int> ReadMdConcurrent::chunkSizes(const int size, const int chunkCount)
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

void ReadMdConcurrent::checkIfDone()
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

void ReadMdConcurrent::announceProgress(bool readFile, const QString &message)
{
    if (stopped)
        return;
    qDebug() << message;
    if (readFile)
        ++done;
}

