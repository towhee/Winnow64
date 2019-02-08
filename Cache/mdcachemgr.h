#ifndef MDCACHEMGR_H
#define MDCACHEMGR_H

#include <QtWidgets>
#include "Datamodel/datamodel.h"
#include "Metadata/metadata.h"
#include "Image/thumb.h"
#include "Cache/mdcacher.h"

class MdCacheMgr : public QObject
{
    Q_OBJECT

public:
    MdCacheMgr(QObject *parent, DataModel *dm);
    void loadMetadataCache(int startRow);

private:
    void launchCachers();
    void chunkify();
    DataModel *dm;

    int thread;
    int threads;
    QStringList allFilePaths;

    struct ThreadItem{
        int row;
        QString fPath;
        int thread;
    } threadItem;

    // chunkified data for each thread
    QVector <MdCacher*> cacher;
    QVector <Metadata*> meta;
    QVector <QVector <ThreadItem>> items;


    int startRow;
    bool isShowCacheStatus;
};

#endif // MDCACHEMGR_H
