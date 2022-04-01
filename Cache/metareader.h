#ifndef METAREADER_H
#define METAREADER_H

#include <QtWidgets>
#include <QMutex>
#include <QThread>
#include <QWaitCondition>
#include "Main/global.h"
#include "Datamodel/datamodel.h"
#include "Metadata/metadata.h"
#include "Metadata/imagemetadata.h"

class MetaReader : public QThread
{
    Q_OBJECT
public:
    MetaReader(QObject *parent, int id, DataModel *dm/*, Metadata *metadata*/);
    ~MetaReader() override;
    void read(int row);
    void stop();
    bool isBusy();

    int threadId;
    int row;

protected:
    void run() Q_DECL_OVERRIDE;

signals:
    void done(int threadId);

private:
    QMutex mutex;
    QWaitCondition condition;
    bool abort;
    DataModel *dm;
    Metadata *metadata;
};

#endif // METAREADER_H
