

#ifndef MDCACHE_H
#define MDCACHE_H

#include <QtWidgets>
#include <QMutex>
#include <QSize>
#include <QThread>
#include <QWaitCondition>
#include "datamodel.h"
#include "metadata.h"
#include "thumb.h"


class MetadataCache : public QThread
{
    Q_OBJECT

public:
    MetadataCache(QObject *parent, DataModel *dm,
                  Metadata *metadata);
    ~MetadataCache();
    void loadMetadataCache(int startRow);
    void stopMetadateCache();
    bool restart;

protected:
    void run() Q_DECL_OVERRIDE;

signals:
    void setIcon(int, QImage);
//    void setIcon(QModelIndex*, QImage*, QString);
    void refreshThumbs();
    void loadImageMetadata(QFileInfo, bool, bool, bool);
//    void loadThumbCache();
    void loadImageCache();
    void updateIsRunning(bool);
    void updateStatus(bool, QString);
    void updateAllMetadataLoaded(bool);

private:
    QMutex mutex;
    QWaitCondition condition;
    bool abort;
    DataModel *dm;
    Metadata *metadata;
    Thumb *thumb;

    QModelIndex idx;
    int startRow;
    QSize thumbMax;         // rgh review hard coding thumb size
    QString err;            // type of error
    bool allMetadataLoaded;

    bool loadMetadata();
    void track(QString fPath, QString msg);

    QElapsedTimer t;
};

#endif // MDCACHE_H

