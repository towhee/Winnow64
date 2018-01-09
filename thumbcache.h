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
    ThumbCache(QObject *parent, DataModel *dm, Metadata *metadata);
    ~ThumbCache();
    void loadThumbCache();
    void stopThumbCache();
    bool restart;

protected:
    void run() Q_DECL_OVERRIDE;

signals:
    void setIcon(QModelIndex&, QImage&, QString&);
    void updateLoadThumb(QString fPath, QString errMsg);
    void updateIsRunning(bool);
    void updateStatus(bool, QString);
    void refreshThumbs();

private:
    DataModel *dm;
    Metadata *metadata;
    QMutex mutex;
    QWaitCondition condition;
    bool abort;

    QString fPath;
    QString folderPath;
    QImage thumb;
    QSize thumbMax;         // rgh review hard coding thumb size
    QString err;            // type of error

    bool loadThumb(int row);
    bool loadFromData(QFile imFile, QString fPath);
    bool loadFromEntireFile(QFile imFile, QString fPath);
    void checkOrientation();

    void track(QString fPath, QString msg);
};

#endif // THUMBCACHE_H
