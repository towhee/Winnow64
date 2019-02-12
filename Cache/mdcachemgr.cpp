#include "mdcachemgr.h"
#include "Main/mainwindow.h"

/*
This class loads the metadata from the image files listed in the datamodel.  It is
called after the datamodel has iterated through the folder(s) and added the file
data available in QFileInfo for the eligible image files.

The ideal thread count n is determined and the list of files is divided into n chunks. N
threads are spawned to concurrently read the metadata from the files. N copies of Metadata are
created to read and store the metadata.

When the metadata has been read, each thread sends the metadata->metaCache QMap to
the datamodel to update.
*/

MdCacheMgr::MdCacheMgr(QObject *parent, DataModel *dm, ThumbView *thumbView) : QObject(parent)
{
    this->dm = dm;
    this->thumbView = thumbView;
}

void MdCacheMgr::stop()
{
    QMutableListIterator<QPointer<MdCacher> > i(cachers);
    while (i.hasNext()) {
        QPointer<MdCacher> cacher = i.next();
        if (cacher->isRunning()) cacher->stopMetadateCache();
    }

    // keep checking until nothing is running
    bool stillRunning = true;
    while (stillRunning){
        stillRunning = false;
        while (i.hasNext()) {
            QPointer<MdCacher> cacher = i.next();
            if (cacher->isRunning()) stillRunning = true;
        }
    }
}

void MdCacheMgr::done(int thread, bool allMetadataLoaded)
{
//    qDebug() << "Thread" << thread << "has finished";
    static int threadCount = 0;
    threadCount++;
    clear();
    if(threadCount == threadTot) {

//        qDebug() << "Metadata caching completed.  Loading image cache.";
        emit updateFilterCount();
        emit updateAllMetadataLoaded(allMetadataLoaded);
        emit updateIsRunning(false, true, __FUNCTION__);
        emit loadImageCache();
        threadCount = 0;
    }
}

void MdCacheMgr::clear()
{
    while (cachers.count()) {
        QMutableListIterator<QPointer<MdCacher>> i(cachers);
        while (i.hasNext()) {
            QPointer<MdCacher> cacher = i.next();
            if (cacher) {
                if (cacher->wait(100)) {
                    delete cacher;
                    i.remove();
                }
            }
            else
                i.remove();
        }
    }
    Q_ASSERT(cachers.isEmpty());

//    qDebug() << "\nDeleting Metas   Count =" << metas.count();
    int count = 0;
    while (metas.count()) {
        QMutableListIterator<QPointer<Metadata>> i(metas);
        while (i.hasNext()) {
            QPointer<Metadata> meta = i.next();
            if (meta) {
//                qDebug() << "Deleting meta #" << count;
                delete meta;
                i.remove();
            }
            else {
                i.remove();
            }
            count++;
        }
    }
    Q_ASSERT(metas.isEmpty());

    allFilePaths.clear();
    items.clear();
    threadItems.clear();
    cacher.clear();
    meta.clear();
//    thumbView->iconHash.clear();
//    dm->metaHash.clear();
}

void MdCacheMgr::loadMetadataCache(int startRow)
{
    stop();
    clear();
    // get a list of all the files requiring metadata extraction
    for(int row = 0; row < dm->rowCount(); row++) {
        QModelIndex idx = dm->index(row, G::PathColumn);
        allFilePaths.append(idx.data(G::PathRole).toString());
    }
    qDebug() << "\n NEW FOLDER";
    launchCachers();
}

void MdCacheMgr::createCachers()
{
    for (int thread = 0; thread < threadTot; ++thread) {
/*        qDebug() << "MdCacheMgr:: launchCachers  thread" << thread;
          for (int i = 0; i < 3; ++i) qDebug() << "\t fPath" << items[thread][i].fPath;

        QPointer<Metadata> meta = QPointer<Metadata>(new Metadata);
        QPointer<MdCacher> cacher =
            QPointer<MdCacher>(new MdCacher(this,
                                            dm,
                                            meta,
                                            &dm->metaHash,
                                            &thumbView->iconHash));
                                            */

        Metadata *meta = new Metadata;
        MdCacher *cacher = new MdCacher(this, dm, meta, &dm->metaHash, &thumbView->iconHash);

        cacher->thread = thread;

        cachers << cacher;
        metas << meta;

        connect(cacher, SIGNAL(processMetadataBuffer()),
                dm, SLOT(processMetadataBuffer()));

        connect(cacher, SIGNAL(processIconBuffer()),
                thumbView, SLOT(processIconBuffer()));

        connect(cacher, SIGNAL(endCaching(int, bool)), this, SLOT(done(int, bool)));

/*        connect(cacher, SIGNAL(setIcon(int, QImage)),
                thumbView, SLOT(setIcon(int, QImage)));

        connect(cacher, &QThread::finished, cacher, &QObject::deleteLater);

        connect(cacher, SIGNAL(finished()), cacher, SLOT(deleteLater()));
        connect(cacher, &MdCacher::finished, cacher, &QObject::deleteLater);
*/
    }
}

void MdCacheMgr::launchCachers()
{
    chunkify();
    if(cachers.size() == 0) createCachers();

    for (int thread = 0; thread < cachers.size(); thread++) {
        cachers.at(thread)->loadMetadataCache(items[thread], isShowCacheStatus);
    }
    emit updateIsRunning(true, true, __FUNCTION__);
}

void MdCacheMgr::chunkify()
{
    // rows = the number of files to load metadata from
    int rows = allFilePaths.count();

    // get number of threads = chunks to populate with metadata
    threadTot = QThread::idealThreadCount();
    if (rows < threadTot) threadTot = rows;

    int itemsPerThread;
    if(rows % threadTot == 0) itemsPerThread = (double)rows / threadTot;
    else itemsPerThread = (double)rows / threadTot + 1;

    qDebug() << "rows" << rows
             << "threads" << threadTot
             << "itemsPerThread" << itemsPerThread;

    // resize the array before populating
    items.resize(threadTot);
    for (int thread = 0; thread < threadTot; ++thread)
        items[thread].resize(itemsPerThread);

    threadItems.resize(rows / threadTot);
    for (int row = 0; row < rows; ++row) {
        threadItem.row = row;
        threadItem.fPath = allFilePaths.at(row);
        threadItem.thread = row % threadTot;
        int threadItemCount = row / threadTot;
        items[threadItem.thread][threadItemCount] = threadItem;
        qDebug() << "row" << row << "rows" << rows
                 << "thread" << threadItem.thread
                 << "threadItemCount" << threadItemCount
                 << "threadItem: row" << threadItem.row
                 << "fPath" << threadItem.fPath
                 << "thread" << threadItem.thread;
    }
}


