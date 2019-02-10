#ifndef MDCACHEMGR_H
#define MDCACHEMGR_H

#include <QtWidgets>
#include "Datamodel/datamodel.h"
#include "Metadata/metadata.h"
#include "Views/thumbview.h"
#include "Image/thumb.h"
#include "Cache/mdcacher.h"

class MdCacheMgr : public QObject
{
    Q_OBJECT

public:
    MdCacheMgr(QObject *parent, DataModel *dm, ThumbView *thumbView);
    void loadMetadataCache(int startRow);

public slots:
    void done(int thread, bool allMetadataLoaded);

private:
    void launchCachers();
    void chunkify();
    DataModel *dm;
    ThumbView *thumbView;

    int thread;
    int threads;
    QStringList allFilePaths;

    // chunkified data for each thread
    QVector <QPointer<MdCacher>> cacher;
    QVector <QPointer<Metadata>> meta;
//    QVector <MdCacher*> cacher;
//    QVector <Metadata*> meta;

    // ThreadItem is declared in mdcacher.h
    ThreadItem threadItem;

    // array of items allocated to a thread
    QVector <ThreadItem> threadItems;

    // array of all thread arrays
    QVector <QVector <ThreadItem>> items;


    int startRow;
    bool isShowCacheStatus;
};

#endif // MDCACHEMGR_H
