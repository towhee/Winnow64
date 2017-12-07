

#ifndef MDCACHE_H
#define MDCACHE_H

#include <QtWidgets>
#include <QMutex>
#include <QSize>
#include <QThread>
#include <QWaitCondition>
#include "datamodel.h"
#include "metadata.h"


class MetadataCache : public QThread
{
    Q_OBJECT

public:
    MetadataCache(QObject *parent, DataModel *dm,
                  Metadata *metadata);
    ~MetadataCache();
    void loadMetadataCache();
    void stopMetadateCache();
    bool restart;

protected:
    void run() Q_DECL_OVERRIDE;

signals:
    void loadImageMetadata(QFileInfo, bool, bool);
    void loadThumbCache();
    void loadImageCache();
    void updateIsRunning(bool);
    void updateStatus(bool, QString);

private:
    QMutex mutex;
    QWaitCondition condition;
    bool abort;
    DataModel *dm;
    Metadata *metadata;

    void track(QString fPath, QString msg);
};

#endif // MDCACHE_H

