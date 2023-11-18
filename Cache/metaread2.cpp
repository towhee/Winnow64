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
    readerCount = QThread::idealThreadCount();
    for (int id = 0; id < readerCount; ++id) {
        Reader *r = new Reader(this, id, dm, imageCache);
        reader.append(r);
        connect(reader[id], &Reader::done, this, &MetaRead2::dispatch);
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

void MetaRead2::setStartRow(int row, bool fileSelectionChanged, QString src)
{
    if (isDebug || G::isLogger || G::isFlowLogger) {
        QString running;
        isRunning() ? running = "true" : running = "false";
        QString s = "row = " + QString::number(row) + " src = " + src + " isRunning = " + running;
        G::log("MetaRead2::setCurrentRow", s);
    }

    // Nothing to read
    if (G::allMetadataLoaded && G::allIconsLoaded) {
        if (fileSelectionChanged) {
            emit fileSelectionChange(dm->sf->index(row, 0));
        }
        return;
    }

    stop();

    mutex.lock();
    this->src = src;
    iconChunkSize = dm->iconChunkSize;
    if (row >= 0 && row < dm->sf->rowCount()) startRow = row;
    else startRow = 0;
    targetRow = startRow;
    abortCleanup = isRunning();
    isDone = false;
    mutex.unlock();

    //if (isDebug)
    {
    qDebug() << "MetaRead2::setStartRow "
             << "startRow =" << startRow
             << "fileSelectionChanged =" << fileSelectionChanged;
    }
    if (!isRunning()) {
        start();
    }
}

bool MetaRead2::stop()
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
    return isRunning();
}

void MetaRead2::initialize()
/*
    Called when change folders.
*/
{
    if (isDebug || G::isLogger || G::isFlowLogger)
    {
        G::log("MetaRead2::initialize",
               "imageCacheTriggerCount = " + QString::number(imageCacheTriggerCount));
    }
    rowsWithIcon.clear();
    dmRowCount = dm->rowCount();
    metaReadCount = 0;
    instance = dm->instance;
    queue.clear();
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

bool MetaRead2::readersRunning()
{
    for (int id = 0; id < readerCount; ++id) {
        if (reader[id]->isRunning()) return true;
    }
    return false;
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
        G::allIconsLoaded = true;   // rgh review
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

void MetaRead2::resetTrigger()
{
    //hasBeenTriggered = false;
}

void MetaRead2::startReaders()
{
    if (isDebug || G::isLogger) G::log("MetaRead2::startThumbDecoders");
    if (isDebug) {
    qDebug() << "MetaRead2::startThumbDecoders"
             << "decoderCount =" << readerCount
                ;
    }

    for (int i = 0; i < readerCount; i++) {
        if (i >= sfRowCount) break;
        if (!reader[i]->isRunning()) {
            reader[i]->fPath = "";
            dispatch(i);
        }
    }
}

void MetaRead2::dispatch(int id)
{
    // if queue is empty and all metadata loaded we're done
    if (queue.size() == 0) {
        if (!isDone && !readersRunning() && dm->isAllMetadataLoaded()) completed();
        return;
    }

    if (abort) {
        return;
    }

    d = reader[id];
    if (isDebug)
    {
    qDebug() << "MetaRead2::dispatch               "
             << "id =" << id
             << "d->fPath =" << d->fPath
                ;
    }

    // if returning reader
    if (d->fPath != "") {
        // update progress in case filters panel activated before all metadata loaded
        metaReadCount++;
        if (isDebug)
        {
        qDebug() << "MetaRead2::dispatch  decoded  "
                 << "metaReadCount =" << metaReadCount
                 << "sfRowCount =" << sfRowCount
                 << d->fPath;
        }
        //int progress = 1.0 * metaReadCount / sfRowCount * 100;
        //emit updateProgress(progress);
        rowsWithIcon.append(d->dmIdx.row());

        QModelIndex sfIdx = dm->proxyIndexFromModelIndex(d->dmIdx);
        int dmRow = d->dmIdx.row();
        // trigger fileSelectionChange which starts ImageCache
        if (dmRow == startRow) {
            //if (isDebug)
            {
            qDebug() << "MetaRead2::dispatch  fileSelectionChange"
                     << "id =" << id
                     << "row =" << dmRow
                     << d->fPath;
            }
            emit fileSelectionChange(sfIdx);
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
    QModelIndex sfIdx = dm->sf->index(sfRow, 0);
    queue.removeFirst();
    if (isDebug)
    {
    qDebug() << "MetaRead2::dispatch  next in queue"
             << "id =" << id
             << "row =" << sfRow
             << fPath
             << "remaining queue count =" << queue.size();
    }


    // progress
//    if (showProgressInStatusbar) {
        emit updateProgressInStatusbar(dmRow, dmRowCount);
        int progress = 1.0 * metaReadCount / dmRowCount * 100;
        emit updateProgressInFilter(progress);
//    }

    // read icon if in icon range
    bool isReadIcon = true;
    /*
    bool isReadIcon = false;
    if (sfRow >= firstIconRow && sfRow <= lastIconRow) {
        if (!dm->iconLoaded(sfRow, instance)) {
            isReadIcon = true;
        }
    }
    */

    // read metadata and icon
    reader[id]->read(dmIdx, fPath, instance, isReadIcon);
}

void MetaRead2::buildQueue()
{
    if (isDebug || G::isLogger || G::isFlowLogger) G::log("MetaRead2::buildQueue", src);

    queue.clear();

    int a = startRow;           // ahead
    int b = startRow - 1;       // back

    while (a < sfRowCount || b >= 0) {
        // abort?
        if (dm->instance != instance) {
            abort = true;
        }
        if (abort) {
            abort = false;
            return;
        }

        if (a < sfRowCount) {
            if (!dm->isMetadataLoaded(a)) {
                QModelIndex sfIdx = dm->sf->index(a, 0);
                QString fPath = sfIdx.data(G::PathRole).toString();
                queue.append(fPath);
            }
            a++;
        }

        if (b >= 0) {
            if (!dm->isMetadataLoaded(b)) {
                QModelIndex sfIdx = dm->sf->index(b, 0);
                QString fPath = sfIdx.data(G::PathRole).toString();
                queue.append(fPath);
            }
            b--;
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
    startReaders();

    if (isDebug) qDebug() << "MetaRead2::run  Decoders started";    // finished or aborted
}

void MetaRead2::completed()
{
    if (isDebug || G::isLogger || G::isFlowLogger) G::log("MetaRead2::done", src);

    decodeThumbsTerminated = true;
    if (G::useUpdateStatus) emit runStatus(false, true, "MetaRead2::run");

    G::allMetadataLoaded = true;
    cleanupIcons();

    emit done();
    isDone = true;

    if (isDebug) qDebug() << "MetaRead2::run  Done.";
}
