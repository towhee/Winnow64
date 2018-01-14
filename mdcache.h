

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
    void loadImageCache();
    void updateIsRunning(bool);
    void showCacheStatus(QImage imCacheStatus);
    void updateStatus(bool, QString);
    void updateAllMetadataLoaded(bool);

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
    void checkIfNewFolder();
    void loadMetadata();
    void track(QString fPath, QString msg);

    // cache status
    QImage *cacheStatusImage;
    QLinearGradient *loadedGradient;
    QPainter *pnt;
    int pxTotWidth;
    int pxUnitWidth;
    int htOffset;       // the offset from the top of pnt to the progress bar
    int ht;             // the height of the progress bar

    bool isShowCacheStatus;

    QElapsedTimer t;
};

#endif // MDCACHE_H

