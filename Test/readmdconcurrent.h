#ifndef READMDCONCURRENT_H
#define READMDCONCURRENT_H


#include <QtWidgets>
#include "Datamodel/datamodel.h"
#include "Metadata/metadata.h"

class ReadMdConcurrent : public QObject
{
    Q_OBJECT

public:
    ReadMdConcurrent(QObject *parent,
                     DataModel *dm,
                     Metadata *md);
    ~ReadMdConcurrent();
    void doTasks(const QStringList &sourceFiles);
    QFutureWatcher<void> watcher;

signals:
    void loadMetadata(QFileInfo fileInfo, bool, bool, bool isReport, bool);

public slots:
    void go(int amount);
    void checkIfDone();
    void announceProgress(bool saved, const QString &source);

private:
    DataModel *dm;
    Metadata *md;
    volatile bool stopped;
    int total;
    int done;
    QElapsedTimer t;
    QVector<int> chunkSizes(const int size, const int chunkCount);
};

#endif // READMDCONCURRENT_H
