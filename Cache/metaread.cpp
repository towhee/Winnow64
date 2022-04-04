#include "metaread.h"

/*
    to do:
        memRequired
        iconCleanup
        iconChunkSize
        initialize req'd ?
        setIconRange req'd ?
        run when scroll event
        new folder start row > 0
*/

MetaRead::MetaRead(QObject *parent,
                   DataModel *dm,
                   ImageCache2 *imageCacheThread2)
    : QThread(parent)
{
    if (G::isLogger) G::log(__FUNCTION__);
    this->dm = dm;
    this->imageCacheThread2 = imageCacheThread2;
    metadata = new Metadata;
    thumb = new Thumb(this, dm, metadata);
    iconChunkSize = 3000;
    abort = false;
}

MetaRead::~MetaRead()
{
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

void MetaRead::initialize(int firstVisibleRow, int lastVisibleRow)
{
    if (G::isLogger) G::log(__FUNCTION__);
    rowCount = dm->sf->rowCount();
//    setIconRange(firstVisibleRow, lastVisibleRow);
}

void MetaRead::read(int currentRow)
{
    if (G::isLogger) G::log(__FUNCTION__);
    stop();
    abort = false;
    this->currentRow = currentRow;
    rowCount = dm->sf->rowCount();
    /*
    qDebug() << __FUNCTION__
             << "dm->firstVisibleRow =" << dm->firstVisibleRow
             << "dm->lastVisibleRow =" << dm->lastVisibleRow
                ;
                //*/
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

void MetaRead::iconCleanup(int sfRow)
{
/*
    When the icon targets (the rows that we want to load icons) change we need to clean up any
    other rows that have icons previously loaded in order to minimize memory consumption. Rows
    that have icons are tracked in the list iconsCached as the dm row (not dm->sf proxy).
*/
    if (G::isLogger) G::log(__FUNCTION__);

//    QMutableListIterator<int> i(iconsCached);
//    QPixmap nullPm;
//    while (i.hasNext()) {
//        if (abort) return;
//        i.next();
//        // the datamodel row dmRow
//        int dmRow = i.value();
//        // the filtered proxy row sfRow
//        int sfRow = dm->sf->mapFromSource(dm->index(dmRow, 0)).row();
//        /* mapFromSource returns -1 if dm->index(dmRow, 0) is not in the filtered dataset
//        This can happen is the user switches folders and the datamodel is cleared before the
//        thread abort is processed.
//        */
//        if (sfRow == -1) return;

//        // remove all loaded icons outside target range
//        if (sfRow < icons.startRow || sfRow > endRow) {
//            i.remove();
//            dm->itemFromIndex(dm->index(dmRow, 0))->setIcon(nullPm);
////            qDebug() << __FUNCTION__ << actionList[action]
////                     << "Removing icon for row" << sfRow;
//        }
//    }
}

void MetaRead::setIconRange(int firstVisibleRow, int lastVisibleRow)
{
/*
    Define the range of icons to cache: prev + current + next viewports/pages of icons.
    Variables are set in MW::updateIconsVisible
*/
    if (G::isLogger) G::log(__FUNCTION__);

    icons.firstVisible = firstVisibleRow;
    icons.lastVisible = lastVisibleRow;
    icons.visibleCount = icons.lastVisible - icons.firstVisible + 1;

    /*
    // total per page (tpp)
    icons.pageCount = icons.visibleCount;
    if (defaultIconsPerPage > icons.pageCount) icons.pageCount = defaultIconsPerPage;

    // if icons visible greater than chunk size then increase chunk size
//    if (tpp > icons.maxCount) icons.maxCount = tpp;

    // first to cache (icons.startRow)
    icons.startRow = icons.firstVisible - icons.pageCount;
    if (icons.startRow < 0) icons.startRow = 0;

    // last to cache (endRow)
    icons.endRow = icons.lastVisible + icons.pageCount;
    if (icons.endRow - icons.startRow < icons.maxCount) icons.endRow = icons.startRow + icons.maxCount;
    if (icons.endRow >= rowCount) icons.endRow = rowCount;
    */
}

bool MetaRead::isNotLoaded(int sfRow)
{
    QModelIndex sfIdx = dm->sf->index(sfRow, G::MetadataLoadedColumn);
    if (!sfIdx.data().toBool()) return true;
    sfIdx = dm->sf->index(sfRow, 0);
    if (sfIdx.data(Qt::DecorationRole).isNull()) return true;
    return false;
}

void MetaRead::buildPriorityQueue(int currentRow)
{
    if (G::isLogger) G::log(__FUNCTION__);
    int firstVisible = dm->firstVisibleRow;
    int lastVisible = dm->lastVisibleRow;
    priority.clear();
    // visible rows (thumbnails)
    for (int row = firstVisible; row < lastVisible; ++row) {
        if (isNotLoaded(row)) priority.append(row);
    }
    /*
    // next page
    for (int row = icons.lastVisible; row < icons.endRow; ++row) {
        if (isNotLoaded(row)) priority.append(row);
    }
    // prev page
    for (int row = icons.startRow; row < icons.firstVisible; ++row) {
        if (isNotLoaded(row)) priority.append(row);
    }
    */
    // alternate ahead/behind until finished
    int behind = firstVisible;
    int ahead = lastVisible - 1;
    while (behind > 0 || ahead < rowCount - 1) {
        if (behind > 0) priority.append(--behind);
        if (ahead < rowCount - 1) priority.append(++ahead);
    }
}

void MetaRead::readMetadata(QModelIndex sfIdx, QString fPath)
{
    if (G::isLogger) G::log(__FUNCTION__);
    int dmRow = dm->sf->mapToSource(sfIdx).row();
    QFileInfo fileInfo(fPath);
    if (metadata->loadImageMetadata(fileInfo, true, true, false, true, __FUNCTION__)) {
//        qDebug() << __FUNCTION__ <<  sfIdx.row() << fPath;
        metadata->m.row = dmRow;
        if (abort) return;
        emit addToDatamodel(metadata->m);
        emit addToImageCache(metadata->m);
        /* direct call
        dm->addMetadataForItem(metadata->m);
        imageCacheThread2->addCacheItem(metadata->m);
        */
    }
}

void MetaRead::readIcon(QModelIndex sfIdx, QString fPath)
{
    if (G::isLogger) G::log(__FUNCTION__);
    QModelIndex dmIdx = dm->sf->mapToSource(sfIdx);
    QImage image;
    bool thumbLoaded = thumb->loadThumb(fPath, image, "MetaRead::readIcon");
    QPixmap pm;
    if (thumbLoaded) {
        pm = QPixmap::fromImage(image.scaled(G::maxIconSize, G::maxIconSize, Qt::KeepAspectRatio));
        dm->itemFromIndex(dmIdx)->setIcon(pm);
        iconMax(pm);
//                iconsCached.append(sfRow);
    }
}

void MetaRead::readRow(int sfRow)
{
    if (G::isLogger) G::log(__FUNCTION__);
    QModelIndex sfIdx = dm->sf->index(sfRow, 0);
    QString fPath = sfIdx.data(G::PathRole).toString();
    bool metaLoaded = dm->sf->index(sfRow, G::MetadataLoadedColumn).data().toBool();
//    qDebug() << __FUNCTION__ << sfRow << G::allMetadataLoaded << metaLoaded;
    // load metadataqDebug() << __FUNCTION__ <<
    if (!G::allMetadataLoaded && !metaLoaded) {
        readMetadata(sfIdx, fPath);
        if (abort) return;
    }
    // load icon
    if (sfIdx.isValid() && sfIdx.data(Qt::DecorationRole).isNull()) {
        readIcon(sfIdx, fPath);
    }
}

void MetaRead::run()
{
/*
    Read metadata and thumbnails.  Priorities are:
        * visible icons
        * the rest
*/
    buildPriorityQueue(currentRow);
//    qDebug() << __FUNCTION__ << priority.length() << priority;
    QModelIndex sfIdx = dm->sf->index(dm->firstVisibleRow, 0);
    QString fPath = sfIdx.data(G::PathRole).toString();
    int i;
    for (i = 0; i < priority.length(); i++) {
        readRow(priority.at(i));
        if (abort) return;
        if (i == 50) emit delayedStartImageCache();
//        if (i == 50) emit setImageCachePosition(fPath);
    }
    if (i <= 50) emit delayedStartImageCache();
    qDebug() << __FUNCTION__ << i;
    emit done();
}
