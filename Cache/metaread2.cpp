#include "metaread2.h"

/*
    MetaRead2, running in a separate thread, loads the metadata and icons into the
    datamodel (dm) for a folder. All the metadata will be loaded, and icons will be
    loaded up to either the iconChunkSize or visibleIcons, depending on the preferences.

    MetaRead2::setCurrentRow is called when:

        • a new folder is selected
        • a new image is selected
        • thumbnails are scrolled
        • the gridView or thumbView are resized

    Steps:

        • Build the priority queue.  The queue lists the dm->sf rows, starting with the
          current row and alternating one ahead and one behind.

        • Cleanup (remove) icons exceeding amount set in preferences.  If loadOnlyVisibleIcons
          then remove all icons except those visible in either thumbView or gridView.  Other-
          wise, remove icons exceeding iconChunkSize based on the priorityQueue.

        • Iterate through the priorityQueue, loading the metadata and icons.  Part way through
          the queue start the ImageCache.

        • Abort and restart when a new setCurrentRow is called.

    Note: All data in the DataModel must be set using a queued connection.  When subsequent
    actions are dependent on the data being set use Qt::BlockingQueuedConnection.


*/

MetaRead2::MetaRead2(QObject *parent,
                   DataModel *dm,
                   Metadata *metadata,
                   FrameDecoder *frameDecoder,
                   ImageCache *imageCache)
    : QThread(parent)
{
    if (isDebug || G::isLogger) G::log("MetaRead2::MetaRead2");
    this->dm = dm;
    this->metadata = metadata;
    this->frameDecoder = frameDecoder;
    this->imageCache = imageCache;
    thumb = new Thumb(dm, metadata, frameDecoder);

    // create n decoder threads
    decoderCount = QThread::idealThreadCount();
    for (int id = 0; id < decoderCount; ++id) {
        Reader *r = new Reader(this, id, dm, imageCache);
        reader.append(r);
        connect(reader[id], &Reader::done, this, &MetaRead2::decodeThumbs);
    }

    instance = 0;
    abort = false;
    isDebug = false;
}

MetaRead2::~MetaRead2()
{
    if (isDebug) qDebug() << "MetaRead2::~MetaRead2";
    delete thumb;
}

void MetaRead2::setStartRow(int row, QString src)
{
    if (isDebug || G::isLogger || G::isFlowLogger) {
        QString running;
        isRunning() ? running = "true" : running = "false";
        QString s = "row = " + QString::number(row) + " src = " + src + " isRunning = " + running;
        G::log("MetaRead2::setCurrentRow", s);
    }

    if (G::allMetadataLoaded && G::allIconsLoaded) return;
    this->src = src;
    stop();
    mutex.lock();
    iconChunkSize = dm->iconChunkSize;
    if (row >= 0 && row < dm->sf->rowCount()) startRow = row;
    else startRow = 0;
    abortCleanup = isRunning();
    mutex.unlock();
    if (!isRunning()) {
        start();
    }
}

void MetaRead2::stop()
{
    if (isDebug || G::isLogger || G::isFlowLogger) G::log("MetaRead2::stop");
    if (isDebug)
    qDebug() << "MetaRead2::stop"
             << "src =" << src
             << "startRow =" << startRow
             << "abort =" << abort
                ;
    if (isRunning()) {
        mutex.lock();
        abort = true;
        condition.wakeOne();
        mutex.unlock();
//        wait();
        abort = false;
    }
    // signal MW all stopped if a folder change
//    if (G::stop) emit stopped("MetaRead2");
    return;
}

int MetaRead2::interrupt()
{
    // is this being used?

    if (isDebug || G::isLogger || G::isFlowLogger) G::log("MetaRead2::pause");
    G::stop = false;
    qDebug() << "MetaRead2::interrupt";
    mutex.lock();
    interrupted = true;
    mutex.unlock();
    stop();
//    qDebug() << "MetaRead2::interrupt after stop  interruptRow =" << interruptedRow;
    return interruptedRow;
}

void MetaRead2::initialize()
{
    if (isDebug || G::isLogger || G::isFlowLogger) G::log("MetaRead2::initialize");
    rowsWithIcon.clear();
    queue.clear();
    imageCachingStarted = false;
    instance = dm->instance;
    dmRowCount = dm->rowCount();
    metaReadCount = 0;
}

QString MetaRead2::diagnostics()
{
    if (isDebug || G::isLogger) G::log("MetaRead2::diagnostics");

    QString reportString;
    QTextStream rpt;
    rpt.setString(&reportString);
    rpt << Utilities::centeredRptHdr('=', objectName() + " MetaRead2 Diagnostics");
    rpt << "\n" ;
    rpt << "\n" << "Load algorithm:        " << (G::isLoadLinear == true ? "Linear" : "Concurrent");
    rpt << "\n" << "instance:              " << instance;
    rpt << "\n" << "abort:              " << (abort ? "true" : "false");
    rpt << "\n" << "isRunning:          " << (isRunning() ? "true" : "false");
    rpt << "\n";
    rpt << "\n" << "imageCacheTriggerCount:" << imageCacheTriggerCount;
    rpt << "\n" << "expansionFactor:       " << expansionFactor;
    rpt << "\n";
//    rpt << "\n" << "visibleIconCount:      " << visibleIconCount;
    rpt << "\n" << "sfRowCount:            " << sfRowCount;
    rpt << "\n" << "dm->currentSfRow:      " << dm->currentSfRow;
    rpt << "\n";
    rpt << "\n" << "defaultIconChunkSize:  " << dm->defaultIconChunkSize;
    rpt << "\n" << "iconChunkSize:         " << iconChunkSize;
    rpt << "\n" << "iconLimit:             " << iconLimit;
    rpt << "\n" << "firstIconRow:          " << firstIconRow;
    rpt << "\n" << "lastIconRow:           " << lastIconRow;
    rpt << "\n" << "rowsWithIcon:          " << rowsWithIcon.size();
    rpt << "\n" << "dm->iconCount:         " << dm->iconCount();
    rpt << "\n\n";
    rpt << "Queue: " << queue.length() << " items\n";
    for (int i = 0; i < queue.length(); i++) {
        int row = dm->rowFromPath(queue.at(i));
        rpt.setFieldAlignment(QTextStream::AlignRight);
        rpt.setFieldWidth(5);
        rpt << row;
        rpt.reset();
        rpt << "  " << queue.at(i) << "\n";
    }
    rpt << "\n";
    rpt << "rowsWithIcon in datamodel:";
    rpt.setFieldAlignment(QTextStream::AlignRight);
    rpt.setFieldWidth(9);
    for (int i = 0; i < dm->rowCount(); i++) {
        if (dm->itemFromIndex(dm->index(i,0))->icon().isNull()) continue;
        rpt << "\n" << i;
    }
//    for (int i = 0; i < rowsWithIcon.size(); i++) {
//        rpt << "\n" << i << rowsWithIcon.at(i);
//    }
//    rpt << reportMetaCache();

    rpt << "\n\n" ;
    return reportString;
}

QString MetaRead2::reportMetaCache()
{
    if (isDebug || G::isLogger) G::log("MetaRead2::reportMetaCache");
    QString reportString;
    QTextStream rpt;
    rpt.flush();
    reportString = "MetaCache";
    rpt.setString(&reportString);

    rpt << "\n";
    return reportString;
}

void MetaRead2::cleanupIcons()
{
/*
    Remove icons not in icon range after start row change, iconChunkSize change or
    MW::deleteFiles.

    The icon range is the lesser of iconChunkSize and dm->sf->rowCount(), centered on
    the current datamodel row dm->currentSfRow.

    For efficiency, the currently cached icons are tracked in the QList rowsWithIcon.
    Each item in rowsWithIcon is the dmRow in the datamodel.

    If a new startRow is sent to setStartPosition while running then the icon
    cleanup is interrupted.
*/
    if (isDebug || G::isLogger) G::log("MetaRead2::cleanupIcons");

    // check if datamodel size is less than assigned icon cache chunk size
    if (iconChunkSize >= sfRowCount) {
        G::allIconsLoaded = true;
        return;
    }

    int i = 0;
    while (rowsWithIcon.size() > iconChunkSize) {
        if (i >= rowsWithIcon.size()) break;
        if (abortCleanup) break;
        int dmRow = rowsWithIcon.at(i);
        QModelIndex dmIdx = dm->index(dmRow, 0);
        int sfRow = dm->sf->mapFromSource(dmIdx).row();
        // if row in icon chunk range then skip
        if (sfRow >= firstIconRow && sfRow <= lastIconRow) {
            ++i;
            continue;
        }
        // if no icon continue
        if (dmIdx.data(Qt::DecorationRole).isNull()) {
            i++;
            continue;
        }
        // remove icon
        emit setIcon(dmIdx, QPixmap(), instance, "MetaRead2::cleanupIcons");
        rowsWithIcon.remove(i);
    }
}

void MetaRead2::startThumbDecoders()
{
    if (isDebug || G::isLogger) G::log("MetaRead2::startThumbDecoders");
    qDebug() << "MetaRead2::startThumbDecoders"
             << "decoderCount =" << decoderCount
                ;
    for (int i = 0; i < decoderCount; i++) {
        if (i >= sfRowCount) break;
        if (!reader[i]->isRunning()) {
            reader[i]->fPath = "";
            decodeThumbs(i);
        }
    }
}
void MetaRead2::decodeThumbs(int id)
{
    if (abort) {
        return;
    }

    d = reader[id];
    qDebug() << "MetaRead2::decodeThumbs"
             << "id =" << id
             << "d->fPath =" << d->fPath
                ;

    // update datamodel and imagecache
    if (d->fPath != "") {
        // update progress in case filters panel activated before all metadata loaded
        metaReadCount++;
        qDebug() << "MetaRead2::decodeThumbs  decoded"
                 << "metaReadCount =" << metaReadCount
                 << "sfRowCount =" << sfRowCount
                 << d->fPath;
        int progress = 1.0 * metaReadCount / sfRowCount * 100;
        emit updateProgress(progress);
        rowsWithIcon.append(d->dmIdx.row());
        // delayed start ImageCache
        if (metaReadCount == sfRowCount || metaReadCount == imageCacheTriggerCount) {
            // start image caching thread after head start
            emit triggerImageCache("Change to startPath","Initial");
            imageCachingStarted = true;
            qDebug() << "MetaRead2::decodeThumbs triggerImageCache"
                     << metaReadCount
                     << queue;
        }
    }

    // if queue is empty we're done
    if (queue.size() == 0) {
        if (!decodeThumbsTerminated) completed();
        return;
    }

    // next in queue
    QString fPath = queue.first();
    int dmRow = dm->rowFromPath(fPath);
    QModelIndex dmIdx = dm->index(dmRow, 0);
    int sfRow = dm->proxyRowFromModelRow(dmIdx.row());
    queue.removeFirst();
    qDebug() << "MetaRead2::decodeThumbs  next in queue"
             << fPath
             << "remaining queue count =" << queue.size();

    // read icon if in icon range
    bool isReadIcon = false;
    if (sfRow >= firstIconRow && sfRow <= lastIconRow) {
        if (!dm->iconLoaded(sfRow, instance)) {
            isReadIcon = true;
        }
    }

    // decode
    reader[id]->decode(dmIdx, fPath, instance, isReadIcon);
}

bool MetaRead2::readMetadata(QModelIndex sfIdx, QString fPath)
{
    if (isDebug || G::isLogger) G::log("MetaRead2::readMetadata");
    if (isDebug)
    {
        qDebug() << "MetaRead2::readMetadata"
                 << "instance =" << instance
                 << "dm->instance" << dm->instance
                 << "row =" << sfIdx.row()
                 << "abort =" << abort
                 << fPath;
    }
    if (instance != dm->instance) {
        qWarning() << "WARNING MetaRead2::readMetadata Instance Clash"
                   << "row =" << sfIdx.row();
        abort = true;
        return false;
    }

    // read metadata from file into metadata->m
    int dmRow = dm->sf->mapToSource(sfIdx).row();
    QFileInfo fileInfo(fPath);
    bool isMetaLoaded = metadata->loadImageMetadata(fileInfo, instance, true, true, false, true, "MetaRead2::readMetadata");
    if (!isMetaLoaded) {
        qDebug() << "MetaRead2::readMetadata"
                 << "row =" << sfIdx.row()
                 << "Load meta failed"
                 ;
    }

    metadata->m.row = dmRow;
    metadata->m.instance = instance;

    // update progress in case filters panel activated before all metadata loaded
    metaReadCount++;
    int progress = 1.0 * metaReadCount / dmRowCount * 100;
    emit updateProgress(progress);

    if (isDebug)
    {
        qDebug() << "MetaRead2::readMetadata"
                 << "added row =" << sfIdx.row()
                 << "abort =" << abort
                 ;
    }

    // add metadata->m to DataModel dm
    emit addToDatamodel(metadata->m, "MetaRead2::readMetadata");
    if (isDebug)
    {
        qDebug() << "MetaRead2::readMetadata"
                 << "addToDatamodel row =" << sfIdx.row()
                 << "abort =" << abort
                 ;
    }

    // add to ImageCache icd->cacheItemList (used to manage image cache)
    if (G::useImageCache) {
        emit addToImageCache(metadata->m);
    }

    if (isDebug) {
        qDebug() << "MetaRead2::readMetadata"
                 << "addToImageCache row =" << sfIdx.row()
                 << "abort =" << abort
                 ;
    }
    return true;
}

void MetaRead2::readIcon(QModelIndex sfIdx, QString fPath)
{
    if (isDebug || G::isLogger) G::log("MetaRead2::readIcon");
    if (isDebug) {
        qDebug().noquote() << "MetaRead2::readIcon"
                           << "start  row =" << sfIdx.row()
                              ;
    }
    if (!sfIdx.isValid()) {
        qWarning() << "WARNING" << "MetaRead2::readIcon"
                   << "sfIdx.isValid() =" << sfIdx.isValid() << sfIdx;
        return;
    }

//  //  dmIdx = dm->sf->mapToSource(sfIdx); // EXC_BAD_ACCESS crash when rapid folder changes
    int dmRow = dm->rowFromPath(fPath);
    QModelIndex dmIdx = dm->index(dmRow, 0);

    bool isVideo = false;
    isVideo = dm->index(dmRow, G::VideoColumn).data().toBool();

    // get thumbnail
    QImage image;
    bool thumbLoaded = false;
    thumbLoaded = thumb->loadThumb(fPath, image, instance, "MetaRead2::readIcon");
    if (isVideo) {
        rowsWithIcon.append(dmRow);
        return;
    }
    QPixmap pm;
    if (thumbLoaded) {
        pm = QPixmap::fromImage(image.scaled(G::maxIconSize, G::maxIconSize, Qt::KeepAspectRatio));
    }
    else {
        pm = QPixmap(":/images/error_image256.png");
        qWarning() << "WARNING" << "MetadataCache::loadIcon" << "Failed to load thumbnail." << fPath;
    }
    emit setIcon(dmIdx, pm, instance, "MetaRead2::readIcon");
    rowsWithIcon.append(dmRow);

    if (isDebug)
    {
        qDebug().noquote() << "MetaRead2::readIcon"
                           << "done   row =" << sfIdx.row()
                              ;
    }
}

void MetaRead2::readRow(int sfRow)
{
    if (isDebug || G::isLogger) G::log("MetaRead2::readRow");
    if (isDebug)
    {
        qDebug().noquote() << "MetaRead2::readRow"
                           << "start  row =" << sfRow
                           << "dm->sf->rowCount()=" << dm->sf->rowCount()
                              ;
    }

    // IconView scroll signal can be delayed
    emit updateScroll();

    // range check
    if (sfRow >= dm->sf->rowCount()) return;
    // valid index check
    QModelIndex sfIdx = dm->sf->index(sfRow, 0);
    if (!sfIdx.isValid()) {
        if (isDebug) {
            qDebug().noquote() << "MetaRead2::readRow  "
                               << "invalid sfidx =" << sfIdx
                                  ;
        }
        return;
    }

    // load metadata
    QString fPath = sfIdx.data(G::PathRole).toString();
    // add to the thumb decoder queue
    queue.append(fPath);
    if (!G::allMetadataLoaded /*&& !isVideo*/ && G::useReadMetadata) {
        bool metaLoaded = dm->sf->index(sfRow, G::MetadataLoadedColumn).data().toBool();
        if (!metaLoaded /*&& !abort*/) {
            if (!abort) {
                if (!readMetadata(sfIdx, fPath)) {
                    return;
                }
            }
            if (abort) return;
        }
    }
    return;     // decoder

    // load icon

    // can ignore if debugging
    if (!G::useReadIcons) return;

    // read icon if in icon range
    if (sfRow >= firstIconRow && sfRow <= lastIconRow) {
        if (!dm->iconLoaded(sfRow, instance)) {
            if (!abort) readIcon(sfIdx, fPath);
            if (rowsWithIcon.size() > iconLimit) {
                if (!abort) cleanupIcons();
            }
        }
    }

    if (isDebug)
    {
        qDebug().noquote() << "MetaRead2::readRow"
                           << "done   row =" << sfRow
                              ;
    }
}

void MetaRead2::buildQueue()
{
    if (isDebug || G::isLogger || G::isFlowLogger) G::log("MetaRead2::buildQueue", src);

    queue.clear();

    int row = startRow;
    int count = -1;                     // used to delay start ImageCache
    bool ahead = true;
    int lastRow = sfRowCount - 1;
    bool moreAhead = row < lastRow;
    bool moreBehind = row >= 0;
    int rowAhead = row;
    int rowBehind = row;

    while (moreAhead || moreBehind) {
        // abort?
        if (dm->instance != instance) {
            abort = true;
        }
        if (abort) {
            if (interrupted) {
                interruptedRow = row;
            }
            qDebug() << "MetaRead2::run ** ABORT ** Returning out of loop at row" << row;
            abort = false;
            return;
        }

        // check if start row has changed
        if (startRow != -1) {
            row = startRow;
            moreAhead = row < lastRow;
            moreBehind = row >= 0;
            rowAhead = row;
            rowBehind = row;
            moreAhead ? ahead = true : ahead = false;

            // icon range in case changes ie thumb size change
            iconChunkSize = dm->iconChunkSize;
            iconLimit = static_cast<int>(iconChunkSize * 1.2);
            firstIconRow = startRow - iconChunkSize / 2;
            if (firstIconRow < 0) firstIconRow = 0;
            lastIconRow = firstIconRow + iconChunkSize - 1;
            if (lastIconRow > lastRow) lastIconRow = lastRow;
            // qDebug() << "MetaRead2::run" << "row =" << row;
            abortCleanup = false;
            startRow = -1;
        }

        // add datamodel row to queue
        if (!dm->sf->index(row, G::MetadataLoadedColumn).data().toBool()) {
            QModelIndex sfIdx = dm->sf->index(row, 0);
            QString fPath = sfIdx.data(G::PathRole).toString();
            queue.append(fPath);
        }

        // next row to process
        if (ahead) {
            if (moreBehind) ahead = false;
            if (moreAhead) {
                ++rowAhead;
                row = rowAhead;
                moreAhead = rowAhead <= sfRowCount;
            }
        }
        else {
            if (moreAhead) ahead = true;
            if (moreBehind) {
                --rowBehind;
                row = rowBehind;
                moreBehind = row >= 0;
            }
        }
    } // end loop building queue
}

void MetaRead2::run()
{
/*
    Loads the metadata and icons into the datamodel (dm) for a folder.  The iteration
    proceeds from the start row in an ahead/behind progression.
*/
    if (isDebug || G::isLogger || G::isFlowLogger) G::log("MetaRead2::read", src);

    folderPath = dm->currentFolderPath;
    sfRowCount = dm->sf->rowCount();

    if (isDebug)
    {
        qDebug().noquote() << "MetaRead2::run"
                           << "src =" << src
                           << "startRow =" << startRow
                           << "sfRowCount =" << sfRowCount
                              ;
    }
    if (startRow >= sfRowCount) return;

    if (G::useUpdateStatus) emit runStatus(true, true, "MetaRead2::run");

    decodeThumbsTerminated = false;

    buildQueue();
    startThumbDecoders();

    if (isDebug) qDebug() << "MetaRead2::run  Decoders started";    // finished or aborted
}

void MetaRead2::completed()
{
    if (isDebug || G::isLogger || G::isFlowLogger) G::log("MetaRead2::done", src);

    decodeThumbsTerminated = true;
    if (G::useUpdateStatus) emit runStatus(false, true, "MetaRead2::run");

    G::allMetadataLoaded = true;
    cleanupIcons();

    emit updateIconBestFit();
    emit done();

    if (isDebug) qDebug() << "MetaRead2::run  Done.";
}
