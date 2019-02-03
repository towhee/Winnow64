

#ifndef MDCACHE_H
#define MDCACHE_H

#include <QtWidgets>
#include <QMutex>
#include <QSize>
#include <QThread>
#include <QWaitCondition>
#include "Datamodel/datamodel.h"
#include "Metadata/metadata.h"
#include "Image/thumb.h"


class MetadataCache : public QThread
{
    Q_OBJECT

public:
    MetadataCache(QObject *parent, DataModel *dm,
                  Metadata *metadata);
    ~MetadataCache();
    void loadMetadataCache(int startRow, bool isShowCacheStatus);
    void stopMetadateCache();
    bool restart;

//    int cacheStatusWidth;

protected:
    void run() Q_DECL_OVERRIDE;

signals:
    void setIcon(int, QImage);
    void refreshThumbs();
    void loadImageMetadata(QFileInfo);
    void loadImageCache();
    void updateIsRunning(bool, bool, QString);
    void updateAllMetadataLoaded(bool);
    void updateFilterCount();
    void updateStatus(bool, QString);
    void showCacheStatus(int, bool);            // row, renew progress bar

private:
    QMutex mutex;
    QWaitCondition condition;
    bool abort;
    DataModel *dm;
    Metadata *metadata;
    Thumb *thumb;
    QMap<int, bool> loadMap;
    QString folderPath;

    QModelIndex idx;
    int startRow;
    QSize thumbMax;         // rgh review hard coding thumb size
    QString err;            // type of error
    bool allMetadataLoaded;

    void createCacheStatus();
    void updateCacheStatus(int row);
    void loadMetadata();
    void track(QString fPath, QString msg);

     bool isShowCacheStatus;

    QElapsedTimer t;
};

#endif // MDCACHE_H

