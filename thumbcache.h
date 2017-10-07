#ifndef THUMBCACHE_H
#define THUMBCACHE_H

#include "global.h"
#include "thumbview.h"
#include "metadata.h"
#include <QMutex>
#include <QSize>
#include <QThread>
#include <QWaitCondition>

class ThumbCache : public QThread
{
    Q_OBJECT

public:
    ThumbCache(QObject *parent, ThumbView *thumbView, Metadata *metadata);
    ~ThumbCache();
    void loadThumbCache();
    void stopThumbCache();
    bool restart;

protected:
    void run() Q_DECL_OVERRIDE;

signals:
    void setIcon(QStandardItem*, QImage, QString);
    void updateIsRunning(bool);
    void updateStatus(bool, QString);
    void refreshThumbs();

private:
    QMutex mutex;
    QWaitCondition condition;
    bool abort;


    void track(QString fPath, QString msg);
    ThumbView *thumbView;
    Metadata *metadata;
};

#endif // THUMBCACHE_H
