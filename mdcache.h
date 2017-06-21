

#ifndef MDCACHE_H
#define MDCACHE_H

#include <QtWidgets>
#include <QMutex>
#include <QSize>
#include <QThread>
#include <QWaitCondition>
#include "thumbview.h"
#include "metadata.h"


class MetadataCache : public QThread
{
    Q_OBJECT

public:
    MetadataCache(QObject *parent, ThumbView *thumbView,
                  Metadata *metadata);
    ~MetadataCache();
    void loadMetadataCache();
    void stopMetadateCache();
    bool restart;

protected:
    void run() Q_DECL_OVERRIDE;

signals:
    void loadThumbCache();
    void loadImageCache();
    void updateIsRunning(bool);

private:
    QMutex mutex;
    QWaitCondition condition;
    bool abort;
    ThumbView *thumbView;
    Metadata *metadata;

    void track(QString fPath, QString msg);
};

#endif // MDCACHE_H

