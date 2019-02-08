#ifndef MDCACHER_H
#define MDCACHER_H


#include <QtWidgets>
#include <QMutex>
#include <QSize>
#include <QThread>
#include <QWaitCondition>
#include "Datamodel/datamodel.h"
#include "Metadata/metadata.h"
#include "Image/thumb.h"

struct ThreadItem{
    int row;
    QString fPath;
    int thread;
};

class MdCacher : public QThread
{
    Q_OBJECT

public:
    MdCacher(QObject *parent, Metadata *metadata);
    ~MdCacher() override;
    void loadMetadataCache(QVector<ThreadItem> items,
                           bool isShowCacheStatus);
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
    Metadata *metadata;
    Thumb *thumb;
    QMap<int, bool> loadMap;
    QString folderPath;


    ThreadItem threadItem;

    QStringList &fList;      // list of files to cache metadata
    QVector <ThreadItem> items;

    QModelIndex idx;
    int startRow;
    QSize thumbMax;         // rgh review hard coding thumb size
    QString err;            // type of error
    bool allMetadataLoaded;

    void createCacheStatus();
    void updateCacheStatus(int row);
    void loadMetadata();

     bool isShowCacheStatus;

    QElapsedTimer t;
};
#endif // MDCACHER_H
