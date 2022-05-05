#include "metaread.h"

/*
    to do:
        memRequired - done except checking
        iconCleanup - done except checking
        iconChunkSize
        run when scroll event
        file selection change while not allMetadataLoaded
        viewport size change
*/

MetaRead::MetaRead(QObject *parent,
                   DataModel *dm/*,
                   ImageCache2 *imageCacheThread2*/)
    : QThread(parent)
{
    if (G::isLogger) G::log(__FUNCTION__);
    this->dm = dm;
//    this->imageCacheThread2 = imageCacheThread2;
    metadata = new Metadata;
    thumb = new Thumb(this, dm, metadata);
//    iconChunkSize = 4000;
    abort = false;
}

MetaRead::~MetaRead()
{
    mutex.lock();
    abort = true;
    condition.wakeOne();
    mutex.unlock();
    wait();
}

void MetaRead::stop()
{
    if (G::isLogger) G::log(__FUNCTION__);
    // stop metaRead thread
    if (isRunning()) {
        mutex.lock();
        abort = true;
        condition.wakeOne();
        mutex.unlock();
        wait();
        abort = false;
    }
}

void MetaRead::initialize()
{
    if (G::isLogger || G::isFlowLogger) G::log(__FUNCTION__);
    iconsLoaded.clear();
    visibleIcons.clear();
    imageCachingStarted = false;
}

void MetaRead::read(Action action, int sfRow, QString src)
{
    if (G::isLogger || G::isFlowLogger) G::log(__FUNCTION__, src + " action = " + QString::number(action));
    /*
    qDebug() << __FUNCTION__ << "src =" << src;
    //*/
    stop();
    abort = false;
    this->action = action;
    sfRowCount = dm->sf->rowCount();
    sfStart = sfRow;
    start();
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
    QModelIndex sfIdx = dm->sf->index(sfRow, G::MetadataLoadedColumn);
    if (!sfIdx.data().toBool()) return true;
    if (!dm->iconLoaded(sfRow)) return true;
    return false;
}

bool MetaRead::isVisible(int sfRow)
{
    if (sfRow >= dm->firstVisibleRow && sfRow <= dm->lastVisibleRow) return true;
    else return false;
}

void MetaRead::cleanupIcons()
{
    if (G::isLogger) G::log(__FUNCTION__);
    // cleanup non-visible icons
    QPixmap nullPm;
    for (int i = 0; i < iconsLoaded.size(); ++i) {
        int dmRow = iconsLoaded.at(i);
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
//        dm->itemFromIndex(dm->index(dmRow, 0))->setIcon(nullPm);
        dm->setIcon(dmIdx, nullPm);
        #if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
        iconsLoaded.remove(i);
        #endif
    }
}

void MetaRead::updateIcons()
{
    if (G::isLogger) G::log(__FUNCTION__);

//    int firstVisible = dm->firstVisibleRow;
//    int lastVisible = dm->lastVisibleRow;

    // load any missing visible icons
    for (int sfRow = dm->firstVisibleRow; sfRow <= dm->lastVisibleRow; ++sfRow) {
//        QModelIndex dmIdx = dm->sf->mapToSource(dm->sf->index(sfRow,0));
//        int dmRow = dmIdx.row();
        int dmRow = dm->modelRowFromProxyRow(sfRow);
        QModelIndex dmIdx = dm->index(dmRow, 0);
        if (!dm->iconLoaded(sfRow)) {
            QImage image;
            QString fPath = dmIdx.data(G::PathRole).toString();
            bool thumbLoaded = thumb->loadThumb(fPath, image, "MetaRead::readIcon");
            if (thumbLoaded) {
                QPixmap pm = QPixmap::fromImage(image.scaled(G::maxIconSize, G::maxIconSize, Qt::KeepAspectRatio));
//                mutex.lock();
//                dm->itemFromIndex(dmIdx)->setIcon(pm);
//                mutex.unlock();
//                dm->setIcon(dmIdx, pm);
                emit setIcon(dmIdx, pm);
                iconMax(pm);
                iconsLoaded.append(dmRow);
            }
        }
        if (abort) return;
    }

    cleanupIcons();
}

void MetaRead::buildMetadataPriorityQueue(int sfRow)
{
    if (G::isLogger) G::log(__FUNCTION__);
    priorityQueue.clear();
    firstVisible = dm->firstVisibleRow;
    lastVisible = dm->lastVisibleRow;
    /*
    visibleIconCount = lastVisible - firstVisible + 1;
    if (visibleIcons > iconChunkSize) adjIconChunkSize = visibleIcons;
    else adjIconChunkSize = iconChunkSize;
    adjIconChunkSize = dm->rowCount();
    bool noIconsLoaded = iconsLoaded.size() == 0;
    */

    /*
    qDebug() << __FUNCTION__
             << "dm->firstVisibleRow =" << dm->firstVisibleRow
             << "dm->lastVisibleRow =" << dm->lastVisibleRow
                ;
    //*/

//    // visible rows (thumbnails)
//    for (int sfRow = firstVisible; sfRow < lastVisible; ++sfRow) {
//        if (isNotLoaded(sfRow)) priorityQueue.append(sfRow);
//        if (abort) return;
//    }

    // icon cleanup all icons no longer visible
    cleanupIcons();

    // alternate ahead/behind until finished
//    int behind = firstVisible;
//    int ahead = lastVisible - 1;
    int behind = sfRow;
    int ahead = sfRow + 1;
    while (behind >= 0 || ahead < sfRowCount) {
//        qDebug() << __FUNCTION__ << behind << ahead;
        priorityQueue.append(behind--);
        priorityQueue.append(ahead++);
//        if (behind > -1) priorityQueue.append(behind--);
//        if (ahead < sfRowCount) priorityQueue.append(ahead++);
        if (abort) return;
    }
        /*
        qDebug() << __FUNCTION__
                 << "firstVisible =" << firstVisible
                 << "lastVisible =" << lastVisible
                 << "priorityQueue.size() =" << priorityQueue.size()
                 << "sfRowCount =" << sfRowCount
                    ;
                 //*/
}

void MetaRead::readMetadata(QModelIndex sfIdx, QString fPath)
{
    if (G::isLogger) G::log(__FUNCTION__);
    int dmRow = dm->sf->mapToSource(sfIdx).row();
    QFileInfo fileInfo(fPath);
    if (metadata->loadImageMetadata(fileInfo, true, true, false, true, __FUNCTION__)) {
        metadata->m.row = dmRow;
//        if (abort) return;
        emit addToDatamodel(metadata->m);
    }
}

void MetaRead::readIcon(QModelIndex sfIdx, QString fPath)
{
    if (G::isLogger) G::log(__FUNCTION__);
    QModelIndex dmIdx = dm->sf->mapToSource(sfIdx);
    QImage image;
    bool thumbLoaded = thumb->loadThumb(fPath, image, "MetaRead::readIcon");
    if (thumbLoaded) {
        QPixmap pm = QPixmap::fromImage(image.scaled(G::maxIconSize, G::maxIconSize, Qt::KeepAspectRatio));
//        dm->itemFromIndex(dmIdx)->setIcon(pm);
//        dm->setIcon(dmIdx, pm);
//        mutex.lock();
//        dm->itemFromIndex(dmIdx)->setIcon(pm);
//        mutex.unlock();
        emit setIcon(dmIdx, pm);
        iconMax(pm);
        iconsLoaded.append(dmIdx.row());
    }
}

void MetaRead::readRow(int sfRow)
{
    if (G::isLogger) G::log(__FUNCTION__);
    QModelIndex sfIdx = dm->sf->index(sfRow, 0);
    if (!sfIdx.isValid()) return;
    QString fPath = sfIdx.data(G::PathRole).toString();
    if (!G::allMetadataLoaded) {
        bool metaLoaded = dm->sf->index(sfRow, G::MetadataLoadedColumn).data().toBool();
        if (!metaLoaded) {
            readMetadata(sfIdx, fPath);
        }
    }

    // load icon
    /*
    qDebug() << __FUNCTION__
             << "sfRow =" << sfRow
             << "isNullIcon =" << isNullIcon
             << "iconsLoaded.size() =" << iconsLoaded.size()
             << "iconChunkSize =" << iconChunkSize
             << "adjIconChunkSize =" << adjIconChunkSize
             ;
    //*/
    if (isVisible(sfRow)) {
        readIcon(sfIdx, fPath);
    }
    // update the imageCache item data
    emit addToImageCache(metadata->m);
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

//    if (action == Scroll || action == SizeChange) {
//        if (G::allMetadataLoaded) {
//            updateIcons();
//            return;
//        }
//    }

    // new folder or file selection change
    buildMetadataPriorityQueue(sfStart);
    if (abort) return;
    int n = static_cast<int>(priorityQueue.size());
//    bool imageCachingStarted = false;
    for (int i = 0; i < n; i++) {
        readRow(priorityQueue.at(i));
        if (abort) return;
        if (!G::allMetadataLoaded && !imageCachingStarted) {
            if (i == (n - 1) || i == 50) {
                emit delayedStartImageCache();
                imageCachingStarted = true;
            }
        }
    }
    emit updateIconBestFit();
    emit done();
}
