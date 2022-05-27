#include "metaread.h"

/*
    to do:
        iconChunkSize
*/

MetaRead::MetaRead(QObject *parent, DataModel *dm) : QThread(parent)
{
    if (G::isLogger) G::log(__FUNCTION__);
    this->dm = dm;
    metadata = new Metadata;
    thumb = new Thumb(this, dm, metadata);
//    iconChunkSize = 4000;
    abort = false;
    debugCaching = false;
}

MetaRead::~MetaRead()
{
    stop();
    delete metadata;
    delete thumb;
}

void MetaRead::stop()
{
    if (G::isLogger) G::log(__FUNCTION__);
    // stop metaRead thread
    QString isRun;
    if (isRunning()) isRun = "true";
    else isRun = "false";
    G::track(__FUNCTION__, "Start: isRunning = " + isRun);
    if (isRunning()) {
        mutex.lock();
        abort = true;
        condition.wakeOne();
        mutex.unlock();
        wait();
        abort = false;
    }
    if (isRunning()) isRun = "true";
    else isRun = "false";
    G::track(__FUNCTION__, "Done:  isRunning = " + isRun);
}

void MetaRead::initialize()
{
    if (G::isLogger || G::isFlowLogger) G::log(__FUNCTION__);
    iconsLoaded.clear();
    visibleIcons.clear();
    priorityQueue.clear();
    imageCachingStarted = false;
}

void MetaRead::read(Action action, int sfRow, QString src)
{
    if (G::isLogger || G::isFlowLogger) G::log(__FUNCTION__, src + " action = " + QString::number(action));
    /*
    qDebug() << __FUNCTION__ << "src =" << src;
    //*/
//    stop();

    this->action = action;
    sfRowCount = dm->sf->rowCount();
    if (debugCaching) {
        qDebug().noquote() << __FUNCTION__
                           << "src =" << src
                           << " action =" << QString::number(action)
                           << "sfRowCount =" << sfRowCount
                              ;
    }
    if (sfRow >= sfRowCount) return;
    sfStart = sfRow;
    if (!abort) start();
}

void MetaRead::iconMax(QPixmap &thumb)
{
    if (G::isLogger) G::log(__FUNCTION__);
    if (G::iconWMax == G::maxIconSize && G::iconHMax == G::maxIconSize) return;

    // for best aspect calc
    int w = thumb.width();
    int h = thumb.height();
    if (w > G::iconWMax) G::iconWMax = w;
    if (h > G::iconHMax) G::iconHMax = h;
}

/*
void MetaRead::iconCleanup()
{
    When the icon targets (the rows that we want to load icons) change we need to clean up any
    other rows that have icons previously loaded in order to minimize memory consumption. Rows
    that have icons are tracked in the list iconsLoaded as the dm row (not dm->sf proxy).

    We need to keep the total icons loaded under iconChunkSize.  We want the icons to be loaded
    in the top of the priorityQueue, which is the list of sfRows, first by visible, and then
    alternating front and back for all of sf.

    Assuming the most complicated case, where the dataset has been filtered (sf), then the
    remainder of the dataset is non_sf. We first remove icons from non_sf and then in sf,
    at the end of the priorityQueue, until we have removed enough.

    - all items in datamodel (dm)
    i items with icon loaded (30 = iconChunkSize)
    f filtered items (21 with 9 icon loaded, need 21 - 9 = 12 icons)
    v visible items
    x icons to remove (12)
    + icons to add (12)
    o outcome after unnecessary icons removed and new ones added

    --iiiiii----------iiiiiiii-------------iiiiiiiiiii-----ii--iii-- 30
    ---------fffffff---ffffff---ffff------ffff---------------------- 21
    ---------vvvvvvv------------------------------------------------  6
    --xxxxxx----------x------x----------------xxx------------------- 12
    ---------+++++++------------++++------+------------------------- 12
    ---------ooooooo---oooooo---oooo------oooo-ooooooo-----oo--ooo-- 30

    if (G::isLogger) G::log(__FUNCTION__);

//    QPixmap null0Pm;
//    for (int row = 0; row < dm->rowCount(); ++row) {
//        QStandardItem *item = dm->itemFromIndex(dm->index(row, 0));
//        item->setIcon(null0Pm);
//    }
    dm->clearAllIcons();
    iconsLoaded.clear();
    return;

    // cleanup extra icons, first count icons loaded in dm->sf
    int allIconsLoaded = iconsLoaded.size() ;
    int sfIconsLoaded = 0;
    for (int sfRow : priorityQueue) {
        int dmRow = dm->modelRowFromProxyRow(sfRow);
        if (iconsLoaded.contains(dmRow)) sfIconsLoaded++;
    }
    // icons loaded but not in dm->sf
    int iconsLoadedNonSF = allIconsLoaded - sfIconsLoaded;
    // icons in dm->sf not loaded
    int sfIconsNotLoaded = sfRowCount - sfIconsLoaded;
//
//    qDebug() << __FUNCTION__
//             << "allIconsLoaded =" << allIconsLoaded
//             << "sfIconsLoaded =" << sfIconsLoaded
//             << "iconsLoadedNonSF =" << iconsLoadedNonSF
//             << "sfIconsNotLoaded =" << sfIconsNotLoaded
//             << "iconChunkSize =" << iconChunkSize
//             << "priorityQueue.size() =" << priorityQueue.size()
//                ;
    // is cleanup required
    if (allIconsLoaded + sfIconsNotLoaded > adjIconChunkSize) {
        int iconsToCleanup = allIconsLoaded + sfIconsNotLoaded - adjIconChunkSize;
        QPixmap nullPm;
        int iconsRemoved = 0;
        // remove iconsToCleanup (iconsLoaded are dmRow)
        for (int i : iconsLoaded) {
            int dmRow = iconsLoaded.at(i);
            qDebug() << __FUNCTION__ << dmRow;
            int sfRow = dm->proxyRowFromModelRow(dmRow);
            // not in dm->sf
            if (sfRow == -1) {
                // remove
                dm->itemFromIndex(dm->index(dmRow, 0))->setIcon(nullPm);
                #if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
                iconsLoaded.remove(i);
                #endif
                iconsRemoved++;
            }
            if (iconsToCleanup >= iconsRemoved) return;
        }
        // still need to remove more icons from back of priorityQueue
        for (int p = priorityQueue.size(); p > 0; --p) {
            int sfRow = priorityQueue.at(p);
            int dmRow = dm->modelRowFromProxyRow(sfRow);
            int i = iconsLoaded.indexOf(dmRow);
            if (i > -1) {
                // remove
                dm->itemFromIndex(dm->index(dmRow, 0))->setIcon(nullPm);
                #if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
                iconsLoaded.remove(i);
                #endif
                iconsRemoved++;
            }
            if (iconsToCleanup == iconsRemoved) return;
        }
    }
}
*/

bool MetaRead::isNotLoaded(int sfRow)
{
    /* not used  */

    QModelIndex sfIdx = dm->sf->index(sfRow, G::MetadataLoadedColumn);
    if (!sfIdx.data().toBool()) return true;
    if (!dm->iconLoaded(sfRow)) return true;
    return false;
}

bool MetaRead::isVisible(int sfRow)
{
    if (sfRow >= dm->firstVisibleRow && sfRow <= dm->lastVisibleRow) return true;
    /*
    if (sfRow >= dm->firstIconRow && sfRow <= dm->lastIconRow) return true;
    if (sfRow >= 0 && sfRow < dm->sf->rowCount()) return true;
    */
    else return false;
}

void MetaRead::dmRowRemoved(int dmRow)
{
    if (G::isLogger) G::log(__FUNCTION__);
//    if (abort) return;
    int idx = iconsLoaded.indexOf(dmRow);
    iconsLoaded.removeAt(idx);
}

void MetaRead::cleanupIcons()
{
    if (G::isLogger) G::log(__FUNCTION__);
    // cleanup non-visible icons
    if (debugCaching) {
        qDebug().noquote() << __FUNCTION__
                           << "start"
                              ;
    }
    QPixmap nullPm;
    for (int i = 0; i < iconsLoaded.size(); ++i) {
        int dmRow = iconsLoaded.at(i);
        // check if row has been deleted
        if (dmRow >= dm->rowCount()) {
            /*
            qDebug() << __FUNCTION__
                     << "dmRow =" << dmRow
                     << "rowCount =" << dm->rowCount()
                        ;
                        //*/
            #if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
            if (!abort) iconsLoaded.remove(i);
            #endif
            continue;
        }
        QModelIndex dmIdx = dm->index(dmRow, 0);
        int sfRow = dm->proxyRowFromModelRow(dmRow);
        /*
        qDebug() << __FUNCTION__
                 << "i =" << i
                 << "iconsLoaded.at(i) = dmRow =" << dmRow
                 << "sfRow =" << sfRow
                 << "dm->firstVisibleRow =" << dm->firstVisibleRow
                 << "dm->lastVisibleRow =" << dm->lastVisibleRow
                 << "isVisible(sfRow) =" << isVisible(sfRow)
                    ;
                    //*/
        if (isVisible(sfRow)) continue;
//        if (abort) return;
        /*if (!abort)*/ emit setIcon(dmIdx, nullPm);
        #if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
        /*if (!abort) */iconsLoaded.remove(i);
        #endif
    }
    if (debugCaching) {
        qDebug().noquote() << __FUNCTION__
                           << "done"
                              ;
    }
}

void MetaRead::updateIcons()
{
    if (G::isLogger) G::log(__FUNCTION__);
    if (debugCaching) {
        qDebug().noquote() << __FUNCTION__
                           << "start"
                              ;
    }

    // load any missing visible icons
    for (int sfRow = dm->firstVisibleRow; sfRow <= dm->lastVisibleRow; ++sfRow) {
        int dmRow = dm->modelRowFromProxyRow(sfRow);
        QModelIndex dmIdx = dm->index(dmRow, 0);
        if (!dm->iconLoaded(sfRow)) {
            QImage image;
            QString fPath = dmIdx.data(G::PathRole).toString();
            bool thumbLoaded = thumb->loadThumb(fPath, image, "MetaRead::readIcon");
            if (thumbLoaded && !abort) {
                QPixmap pm = QPixmap::fromImage(image.scaled(G::maxIconSize, G::maxIconSize, Qt::KeepAspectRatio));
//                if (abort) return;
                /*if (!abort) */emit setIcon(dmIdx, pm);
                /*if (!abort) */iconMax(pm);
                /*if (!abort) */iconsLoaded.append(dmRow);
            }
        }
//        if (abort) return;
    }

    cleanupIcons();
    if (debugCaching) {
        qDebug().noquote() << __FUNCTION__
                           << "done"
                              ;
    }
}

void MetaRead::buildMetadataPriorityQueue(int sfRow)
{
    if (G::isLogger) G::log(__FUNCTION__);
    priorityQueue.clear();
    firstVisible = dm->firstVisibleRow;
    lastVisible = dm->lastVisibleRow;

    if (debugCaching) {
        qDebug().noquote() << __FUNCTION__
                           << "start"
                           << "firstVisible =" << firstVisible
                           << "lastVisible =" << lastVisible
                           << "priorityQueue.size() =" << priorityQueue.size()
                           << "sfRowCount =" << sfRowCount
                              ;
    }
    // icon cleanup all icons no longer visible
    if (!abort) cleanupIcons();

    // alternate ahead/behind until finished
    int behind = sfRow;
    int ahead = sfRow + 1;
    while (behind >= 0 || ahead < sfRowCount) {
        if (behind >= 0) priorityQueue.append(behind--);
        if (ahead < sfRowCount) priorityQueue.append(ahead++);
//        if (abort) return;
    }
    if (debugCaching) {
        qDebug().noquote() << __FUNCTION__
                           << "start"
                           << "firstVisible =" << firstVisible
                           << "lastVisible =" << lastVisible
                           << "priorityQueue.size() =" << priorityQueue.size()
                           << "sfRowCount =" << sfRowCount
                              ;
    }
}

void MetaRead::readMetadata(QModelIndex sfIdx, QString fPath)
{
//    if (abort) return;
    if (G::isLogger) G::log(__FUNCTION__);
    int dmRow = dm->sf->mapToSource(sfIdx).row();
    if (debugCaching) qDebug().noquote() << __FUNCTION__ << "start  row =" << sfIdx.row();
    QFileInfo fileInfo(fPath);
    if (metadata->loadImageMetadata(fileInfo, true, true, false, true, __FUNCTION__)) {
        metadata->m.row = dmRow;
//        if (abort) return;
//        qDebug() << __FUNCTION__ << "addToDatamodel: start  row =" << sfIdx.row();
        if (debugCaching) qDebug().noquote() << __FUNCTION__ << "start  addToDatamodel"
                                             << "G::stop =" << G::stop
                                             << "abort =" << abort
                                                ;
        if (!abort) emit addToDatamodel(metadata->m);
        if (debugCaching) qDebug().noquote() << __FUNCTION__ << "done   addToDatamodel";
//        qDebug() << __FUNCTION__ << "addToDatamodel: done   row =" << sfIdx.row();
//        dm->addMetadataForItem(metadata->m);
    }
    if (debugCaching) qDebug().noquote() << __FUNCTION__ << "done row =" << sfIdx.row();
}

void MetaRead::readIcon(QModelIndex sfIdx, QString fPath)
{
//    if (abort) return;
    if (G::isLogger) G::log(__FUNCTION__);
    if (debugCaching) {
        qDebug().noquote() << __FUNCTION__
                           << "start  row =" << sfIdx.row()
                              ;
    }
    QModelIndex dmIdx = dm->sf->mapToSource(sfIdx);
    QImage image;
    bool thumbLoaded = thumb->loadThumb(fPath, image, "MetaRead::readIcon");
    if (thumbLoaded) {
        QPixmap pm = QPixmap::fromImage(image.scaled(G::maxIconSize, G::maxIconSize, Qt::KeepAspectRatio));
//        qDebug() << __FUNCTION__ << "setIcon: start  row =" << sfIdx.row();
        if (!abort) emit setIcon(dmIdx, pm);
//        qDebug() << __FUNCTION__ << "setIcon: done   row =" << sfIdx.row();
//        dm->setIcon(dmIdx, pm);
        /*if (!abort) */iconMax(pm);
        /*if (!abort) */iconsLoaded.append(dmIdx.row());
    }
    if (debugCaching) {
        qDebug().noquote() << __FUNCTION__
                           << "done   row =" << sfIdx.row()
                              ;
    }
}

void MetaRead::readRow(int sfRow)
{
//    if (abort) return;
    if (G::isLogger) G::log(__FUNCTION__);
    if (debugCaching) {
        qDebug().noquote() << __FUNCTION__
                           << "start  row =" << sfRow
                              ;
    }
    if (sfRow >= dm->sf->rowCount()) return;
    QModelIndex sfIdx = dm->sf->index(sfRow, 0);
    if (!sfIdx.isValid()) return;
    QString fPath = sfIdx.data(G::PathRole).toString();
    if (!G::allMetadataLoaded) {
//        if (abort) return;
        bool metaLoaded = dm->sf->index(sfRow, G::MetadataLoadedColumn).data().toBool();
        if (!metaLoaded && !abort) {
            readMetadata(sfIdx, fPath);
        }
    }

    // load icon
    /*
    qDebug() << __FUNCTION__
             << "sfRow =" << sfRow
             << "iconsLoaded.size() =" << iconsLoaded.size()
             << "iconChunkSize =" << iconChunkSize
             << "adjIconChunkSize =" << adjIconChunkSize
             ;
    //*/
    if (isVisible(sfRow) && !abort) {
        readIcon(sfIdx, fPath);
    }

    // update the imageCache item data
    if (!G::stop) emit addToImageCache(metadata->m);
    if (debugCaching) {
        qDebug().noquote() << __FUNCTION__
                           << "done   row =" << sfRow
                              ;
    }
}

void MetaRead::run()
{
/*
    Read metadata and thumbnails.  Priorities are:
        * visible icons
        * the rest

    priorityQueue is a list of sfRow with lowest = highest priority
*/
    if (G::isLogger || G::isFlowLogger) G::log(__FUNCTION__);

    // new folder or file selection change
    buildMetadataPriorityQueue(sfStart);
    int n = static_cast<int>(priorityQueue.size());
    for (int i = 0; i < n; i++) {
        if (abort) {
            G::track(__FUNCTION__, "Aborting MetaRead.  Row = " + QString::number(i));
            return;
        }
        if (G::isLogger || G::isFlowLogger) G::log(__FUNCTION__, "i =" + QString::number(i));
        readRow(priorityQueue.at(i));
        if (!G::allMetadataLoaded && !imageCachingStarted && !abort && !G::stop) {
            if (i == (n - 1) || i == 50) {
                G::track(__FUNCTION__, "emit delayedStartImageCache.");
                if (!abort) emit delayedStartImageCache();
                imageCachingStarted = true;
            }
        }
    }
    if (abort) {
        G::track(__FUNCTION__, "Aborting MetaRead.  All rows read.");
        return;
    }
    if (!abort) emit updateIconBestFit();
    if (!abort) emit done();
    G::track(__FUNCTION__, "Finished naturally.");
}
