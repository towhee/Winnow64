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
//MW *m;

MdCacheMgr::MdCacheMgr(QObject *parent, DataModel *dm) : QObject(parent)
{
//    m = qobject_cast<MW*>(parent);
    this->dm = dm;
}

void MdCacheMgr::loadMetadataCache(int startRow)
{
    // get a list of all the files requiring metadata extraction
    allFilePaths.clear();
    for(int row = 0; row < dm->rowCount(); row++) {
        QModelIndex idx = dm->index(row, G::PathColumn);
        allFilePaths.append(idx.data(G::PathRole).toString());
    }
    launchCachers();
}

void MdCacheMgr::launchCachers()
{
    chunkify();
    for (int thread = 0; thread < threads; ++thread) {
        cacher.at(thread)->loadMetadataCache(items[thread], isShowCacheStatus);
    }
}

void MdCacheMgr::chunkify()
{
    // get number of threads = chunks to populate with metadata
    threads = QThread::idealThreadCount();

    // divvy up the files by thread / chunk
    int row = 0;
    int threadItemCount = 0;
    while (row < allFilePaths.count()) {
        for(int thread = 0; thread < threads; ++thread) {
            threadItem.row = row;
            threadItem.fPath = allFilePaths.at(row);
            threadItem.thread = thread;
            items[thread][threadItemCount] = threadItem;
            row++;
        }
        threadItemCount++;
    }

    // create cacher workers with associated metadata classes for each thread
    for(int chunk = 0; chunk < threads; ++chunk) {
        meta[chunk] = new Metadata;
        cacher[chunk] = new MdCacher(this, threadFileList[chunk], meta[chunk]);
    }
}


