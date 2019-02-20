#ifndef MDCACHEMGR_H
#define MDCACHEMGR_H

#include <QtWidgets>
#include "Datamodel/datamodel.h"
#include "Metadata/metadata.h"
#include "Views/iconview.h"
#include "Image/thumb.h"
#include "Cache/mdcacher.h"

class MdCacheMgr : public QObject
{
    Q_OBJECT

public:
    MdCacheMgr(QObject *parent, DataModel *dm, IconView *thumbView);
    void loadMetadataCache(int startRow);
    void stop();

signals:
    void loadImageCache();
    void updateFilterCount();
    void updateAllMetadataLoaded(bool);
    void updateIsRunning(bool, bool, QString);

public slots:
    void done(int threadCount, bool allMetadataLoaded);

private:
    void launchCachers();
    void chunkify();
    void clear();
    void createCachers();

    DataModel *dm;
    IconView *thumbView;

    int threadCount;
    int threadTot;
    QStringList allFilePaths;

    // list of threads to run
    QList<QPointer<MdCacher> > cachers;

    // list of metadata objects created
    QList<QPointer<Metadata> > metas;

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
