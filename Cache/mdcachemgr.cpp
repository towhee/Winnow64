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

// this works because prefdlg is a friend class of MW
//MW *mw7;

MdCacheMgr::MdCacheMgr(QObject *parent, DataModel *dm, ThumbView *thumbView) : QObject(parent)
{
//    mw7 = qobject_cast<MW*>(parent);
    this->dm = dm;
    this->thumbView = thumbView;
}

void MdCacheMgr::done(int thread, bool allMetadataLoaded)
{
//    qDebug() << "Thread" << thread << "has finished";
    static int threadCount = 0;
    threadCount++;
    if(threadCount == threadTot) {
        stop();
        items.clear();
        threadItems.clear();
        cacher.clear();
        meta.clear();

//        qDebug() << "Metadata caching completed.  Loading image cache.";
        emit loadImageCache();
        emit updateFilterCount();
        emit updateAllMetadataLoaded(true);
        emit updateIsRunning(false, true, __FUNCTION__);
        threadCount = 0;
    }
}

void MdCacheMgr::stop()
{
    while (metas.count()) {
        QMutableListIterator<QPointer<Metadata> > i(metas);
        while (i.hasNext()) {
            QPointer<Metadata> meta = i.next();
            if (meta) {
                delete meta;
                i.remove();
            }
            else
                i.remove();
        }
    }
    while (cachers.count()) {
        QMutableListIterator<QPointer<MdCacher> > i(cachers);
        while (i.hasNext()) {
            QPointer<MdCacher> thread = i.next();
            if (thread) {
                if (thread->wait(100)) {
                    delete thread;
                    i.remove();
                }
            }
            else
                i.remove();
        }
    }
    Q_ASSERT(cachers.isEmpty());
}

void MdCacheMgr::loadMetadataCache(int startRow)
{
    stop();
    // get a list of all the files requiring metadata extraction
    allFilePaths.clear();
    for(int row = 0; row < dm->rowCount(); row++) {
        QModelIndex idx = dm->index(row, G::PathColumn);
        allFilePaths.append(idx.data(G::PathRole).toString());
    }
//    qDebug() << "allFilePaths" << allFilePaths;
    launchCachers();
}

void MdCacheMgr::launchCachers()
{
    chunkify();
    for (int thread = 0; thread < threadTot; ++thread) {
//        qDebug() << "MdCacheMgr:: launchCachers  thread" << thread;
//        for (int i = 0; i < 3; ++i) qDebug() << "\t fPath" << items[thread][i].fPath;

        QPointer<Metadata> meta = QPointer<Metadata>(new Metadata);
        QPointer<MdCacher> cacher = QPointer<MdCacher>(new MdCacher(this, dm, meta, &dm->metaHash));
//        Metadata *meta = new Metadata;
//        MdCacher *cacher = new MdCacher(this, dm, meta);

        cachers << cacher;
        metas << meta;

        connect(cacher, SIGNAL(processBuffer()),
                dm, SLOT(processMetadataBuffer()));

        connect(cacher, SIGNAL(setIcon(int, QImage)),
                thumbView, SLOT(setIcon(int, QImage)));

        connect(cacher, SIGNAL(updateAllMetadataLoaded(int, bool)), this, SLOT(done(int, bool)));
//        connect(cacher, &QThread::finished, cacher, &QObject::deleteLater);

        //        connect(cacher, SIGNAL(finished()), cacher, SLOT(deleteLater()));
//        connect(cacher, &MdCacher::finished, cacher, &QObject::deleteLater);

        cacher->loadMetadataCache(items[thread], isShowCacheStatus);
        emit updateIsRunning(true, true, __FUNCTION__);
    }
}

void MdCacheMgr::chunkify()
{
    // get number of threads = chunks to populate with metadata
    threadTot = QThread::idealThreadCount();
    int itemsPerThread;
    int rows = allFilePaths.count();
    if(rows % threadTot == 0) itemsPerThread = (double)rows / threadTot;
    else itemsPerThread = (double)rows / threadTot + 1;

//    qDebug() << "rows" << rows << "threads" << threads << "itemsPerThread" << itemsPerThread;

    items.resize(threadTot);
    for (int thread = 0; thread < threadTot; ++thread)
        items[thread].resize(itemsPerThread);

    threadItems.resize(itemsPerThread);


    // divvy up the files by thread / chunk
    int row = 0;
    int threadItemCount = 0;
    while (row < (rows - 1)) {
        for(int thread = 0; thread < threadTot; ++thread) {
            threadItem.row = row;
            threadItem.fPath = allFilePaths.at(row);
            threadItem.thread = thread;
            items[thread][threadItemCount] = threadItem;
//            qDebug() << "row" << row << "thread" << thread
//                     << "threadItemCount" << threadItemCount
//                     << "threadItem: row" << threadItem.row
//                     << "fPath" << threadItem.fPath
//                     << "thread" << threadItem.thread;
            row++;
            if (row > rows - 1) break;
        }
        threadItemCount++;
    }

//    qDebug() << "Made it to here";

//    // create cacher workers with associated metadata classes for each thread
//    meta.resize(threads);
//    cacher.resize(threads);
//    for(int thread = 0; thread < threads; ++thread) {
//        meta[thread] = new Metadata;
//        cacher[thread] = new MdCacher(this, meta[thread]);
//    }
}


