#ifndef READMDCONCURRENT_H
#define READMDCONCURRENT_H


#include <QtWidgets>
#include "Datamodel/datamodel.h"

class ReadMdConcurrent : public QObject
{
    Q_OBJECT

public:
    ReadMdConcurrent(QObject *parent, DataModel *dm);
    ~ReadMdConcurrent();
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

#endif // READMDCONCURRENT_H
