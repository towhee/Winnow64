#ifndef READSYNC_H
#define READSYNC_H

#include <QtWidgets>
#include <QMutex>
#include <QSize>
#include <QThread>
#include <QWaitCondition>
#include "Datamodel/datamodel.h"
#include "Metadata/metadata.h"
#include "Image/thumb.h"

class ReadSync : public QThread
{
    Q_OBJECT

public:
    ReadSync(QObject *parent, DataModel *dm);
    ~ReadSync() Q_DECL_OVERRIDE;
    bool restart;
    void go(int amount);

protected:
    void run() Q_DECL_OVERRIDE;

private:
    QMutex mutex;
    QWaitCondition condition;
    bool abort;
    DataModel *dm;
    Metadata *metadata;
    QString folderPath;
    int total;
};

#endif // READSYNC_H
