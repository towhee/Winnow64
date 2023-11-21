#include "metaread2.h"

/*
    MetaRead2, running in a separate thread, loads the metadata and icons into the
    datamodel (dm) for a folder. All the metadata will be loaded, and icons will be
    loaded up the iconChunkSize, which is set to 20000.

    MetaRead2::setCurrentRow is called when:

        • a new folder is selected
        • a new image is selected
        • thumbnails are scrolled
        • the gridView or thumbView are resized

    Steps:

        • Build the toRead list.  The queue lists the datamodel rows. The list is depleted each
          time a reader returns successfully.

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
    qDebug() << "MetaRead2::MetaRead2" << readerCount << "readers";
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
    // is this req'd?
    sfRowCount = dm->sf->rowCount();
    if (startRow >= sfRowCount) return;

    // All loaded, select file
    if (G::allMetadataLoaded && G::allIconsLoaded) {
        if (fileSelectionChanged) {
            emit fileSelectionChange(dm->sf->index(row, 0));
        }
        return;
    }

    // load datamodel with metadata and icons
    mutex.lock();
    this->src = src;
    iconChunkSize = dm->iconChunkSize;
    if (row >= 0 && row < dm->sf->rowCount()) startRow = row;
    else startRow = 0;
    a = startRow;
    b = startRow - 1;
    abortCleanup = isRunning();
    isDone = false;
    mutex.unlock();

    if (isDebug)
    {
    qDebug() << "\nMetaRead2::setStartRow "
             << "startRow =" << startRow
             << "a =" << a
             << "b =" << b
             << "fileSelectionChanged =" << fileSelectionChanged
             << "folder =" << dm->currentFolderPath
            ;
    }
    if (G::useUpdateStatus) emit runStatus(true, true, "MetaRead2::run");

    if (isRunning()) {
        //dispatchReaders();
    }
    else {
        start();
    }
}

bool MetaRead2::stop()
{
    if (isDebug || G::isLogger || G::isFlowLogger) G::log("MetaRead2::stop");

    // stop all readers
    for (int id = 0; id < readerCount; ++id) {
        reader[id]->stop();
    }

    // stop MetaRead2
    if (isRunning()) {
        mutex.lock();
        abort = true;
        condition.wakeOne();
        mutex.unlock();
        wait();
        abort = false;
    }

    if (isDebug)
    {
    qDebug() << "MetaRead2::stop"
             << "isRunning =" << isRunning()
        ;}
    return isRunning();
}

void MetaRead2::initialize()
/*
    Called when change folders.
*/
{
    if (isDebug || G::isLogger || G::isFlowLogger)
    {
        G::log("MetaRead2::initialize");
    }
    rowsWithIcon.clear();
    dmRowCount = dm->rowCount();
    metaReadCount = 0;
    metaReadItems = dmRowCount;
    redoCount = 0;
    instance = dm->instance;
    allDone = false;
    err.clear();
    toRead.clear();
    for (int i = 0; i < dmRowCount; i++) {
        toRead.append(i);
    }
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
    rpt << "\n" << "abort:                 " << (abort ? "true" : "false");
    rpt << "\n" << "isRunning:             " << (isRunning() ? "true" : "false");
    rpt << "\n";
    rpt << "\n" << "expansionFactor:       " << expansionFactor;
    rpt << "\n";
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
    rpt << "toRead: " << toRead.length() << " items\n";
    rpt.setFieldAlignment(QTextStream::AlignLeft);
    for (int i = 0; i < toRead.length(); i++) {
        rpt << "Item at row ";
        rpt.setFieldWidth(5);
        rpt << toRead.at(i) ;
        rpt << " dm metadata loaded ";
        rpt << QVariant(dm->isMetadataLoaded(toRead.at(i))).toString() << "\n";
    }
    rpt << "\n" << "Errors: " << err.length() << " items\n";
    for (int i = 0; i < err.length(); i++) {
        rpt.setFieldAlignment(QTextStream::AlignLeft);
        //rpt.setFieldWidth(5);
        rpt << err.at(i) << "\n";
    }

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
        if (reader[id]->isRunning()) {
            //qDebug() << "MetaRead2::readersRunning     " << "id =" << id << "is running";
            return true;
        }
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
        //dm->setIcon(dmIdx, QPixmap(), instance, "MetaRead2::cleanupIcons");
        rowsWithIcon.remove(i);
    }
}

bool MetaRead2::redo()
{
    if (++redoCount > redoMax) {
        qWarning() << "WARNING MetaRead2::redo limit exceeded: missing metadata or icon" << toRead;
        return false;
    }
    if (isDebug)
    {
        qDebug() << "MetaRead2::redo" << "count =" << redoCount << toRead;
    }
    a = startRow;
    b = a--;
    dispatchReaders();
    return true;
}

void MetaRead2::dispatch(int id)
{
    reader[id]->pending = false;
    if (abort) return;

    static bool isAhead = true;

    d = reader[id];
    if (d->fPath == "" && id >= dmRowCount) return;

    if (isDebug)
    {
        QString  row;
        d->fPath == "" ? row = "-1" : row = QString::number(d->dmIdx.row());
        qDebug().noquote()
            << "MetaRead2::dispatch     current top         "
            << "id =" << QString::number(id).leftJustified(2, ' ')
            << "row =" << row.leftJustified(4, ' ')
            //<< "isAhead =" << QVariant(isAhead).toString().leftJustified(5, ' ')
            << "aIsDone =" << QVariant(aIsDone).toString().leftJustified(5, ' ')
            << "bIsDone =" << QVariant(bIsDone).toString().leftJustified(5, ' ')
            << "allRead =" << QVariant(allDone).toString().leftJustified(5, ' ')
            << "a =" << QString::number(a).leftJustified(4, ' ')
            << "b =" << QString::number(b).leftJustified(4, ' ')
            ;
    }

    // if returning reader
    if (d->fPath != "" && d->instance == dm->instance) {
        int dmRow = d->dmIdx.row();

        // check status and deplete toRead
        {
        if (d->status == d->Status::Success) {  // or if metadata loaded??
            for (int i = 0; i < toRead.size(); i++) {
                if (toRead.at(i) == dmRow) {
                    toRead.removeAt(i);
                    break;
                }
            }
        }
        else {
            QString error = "row " + QString::number(dmRow) + " failed: "
                            + d->statusText.at(d->status) + " " + d->fPath;
            err.append(error);
            if (isDebug)
            qDebug() << "MetaRead2::dispatch  Failure    "
                     << "row =" << dmRow
                     << "status =" << d->statusText.at(d->status)
                     << d->fPath;
        }
        }

        // track rows with icon in case
        if (!abort) rowsWithIcon.append(dmRow);

        // trigger fileSelectionChange which starts ImageCache
        QModelIndex sfIdx;
        if (!abort) sfIdx = dm->proxyIndexFromModelIndex(d->dmIdx);
        if (dmRow == startRow) {
            if (isDebug)
            {
            qDebug() << "MetaRead2::dispatch  fileSelectionChange"
                     << "id =" << id
                     << "row =" << dmRow
                     << d->fPath;
            }
            if (!abort) emit fileSelectionChange(sfIdx);
        }

        if (isDebug)
        {
            qDebug().noquote()
                << "MetaRead2::dispatch     processed:         "
                << "id =" << QString::number(id).leftJustified(2, ' ')
                << "row =" << QString::number(dmRow).leftJustified(4, ' ')
                << "a =" << QString::number(a).leftJustified(4, ' ')
                << "b =" << QString::number(b).leftJustified(4, ' ')
                << "dm->isAllMetadataLoaded" << dm->isAllMetadataLoaded()
                ;
        }

        // report progress
        metaReadCount++;
        if (showProgressInStatusbar) {
            emit updateProgressInStatusbar(dmRow, dmRowCount);
            int progress = 1.0 * metaReadCount / dmRowCount * 100;
            emit updateProgressInFilter(progress);
        }

        // if all datamodel rows have been read we are done unless a read failure
        //if (allDone) {
        //if (metaReadCount >= metaReadItems) {
        if (toRead.size() == 0) {
            bool stillReadersPending = false;
            // pending out of order readers to process
            if (!dm->isAllMetadataLoaded()) {
                for (int id = 0; id < readerCount; ++id) {
                    if (reader[id]->pending || reader[id]->isRunning()) {
                        if (isDebug)
                        {
                            qDebug().noquote()
                                << "MetaRead2::dispatch     stillThreadsPending "
                                << "id =" << QString::number(id).leftJustified(2, ' ')
                                << "row =" << QString::number(reader[id]->dmIdx.row()).leftJustified(4, ' ')
                                << "allMetadataLoaded =" << dm->isAllMetadataLoaded() ;
                        }
                        stillReadersPending = true;
                        break;
                    }
                }
                if (isDebug)
                {
                qDebug().noquote()
                    << "MetaRead2::dispatch  terminate   "
                    << "id =" << QString::number(id).leftJustified(2, ' ')
                    << "row =" << QString::number(dmRow).leftJustified(4, ' ')
                    << "stillThreadsPending =" << QVariant(stillReadersPending).toString().leftJustified(5, ' ')
                    << "isDone =" << QVariant(isDone).toString().leftJustified(5, ' ')
                    << "readersRunning() =" << QVariant(readersRunning()).toString().leftJustified(5, ' ')
                    << "dm->isAllMetadataLoaded() =" << QVariant(dm->isAllMetadataLoaded()).toString().leftJustified(5, ' ')
                    ;
                }
            }
            // all readers processed
            else {
                /* redo
                // if not all metadata loaded then redo up to redoMax times
                if (!stillReadersPending && !dm->isAllMetadataLoaded()) {
                    if (!abort) {
                        // redo unless redo limit exceeded
                        if (redo()) return;
                    }
                }
                //*/

                // Now we are done
                if (G::useUpdateStatus) emit runStatus(false, true, "MetaRead2::run");
                if (!isDone) {
                    if (!abort) cleanupIcons();
                    qDebug() << "MetaRead2::dispatch     We Are Done.  toRead =" << toRead;
                    if (!abort) emit done();
                    isDone = true;
                }
            }

            return;
        }
    }

    if (allDone) return;

    aIsDone = (a == dmRowCount - 1);
    bIsDone = (b < 0);

    // launch a reader
    if (isAhead && aIsDone) isAhead = false;
    if (!isAhead && bIsDone) isAhead = true;
    int dmRow;
    isAhead ? dmRow = a : dmRow = b;
    QModelIndex dmIdx = dm->index(dmRow, 0);
    QString fPath = dmIdx.data(G::PathRole).toString();
    bool isReadIcon = true;
    reader[id]->pending = true;
    if (!abort) reader[id]->read(dmIdx, fPath, instance, isReadIcon);

    if (isDebug)
    {
        qDebug().noquote()
            << "MetaRead2::dispatch     read current item   "
            << "id =" << QString::number(id).leftJustified(2, ' ')
            << "row =" << QString::number(dmRow).leftJustified(4, ' ')
            << "isAhead =" << QVariant(isAhead).toString().leftJustified(5, ' ')
            << "aIsDone =" << QVariant(aIsDone).toString().leftJustified(5, ' ')
            << "bIsDone =" << QVariant(bIsDone).toString().leftJustified(5, ' ')
            << "allDone =" << QVariant(allDone).toString().leftJustified(5, ' ')
            << "a =" << QString::number(a).leftJustified(4, ' ')
            << "b =" << QString::number(b).leftJustified(4, ' ')
            ;
    }

    allDone = (aIsDone && bIsDone);
    if (allDone) return;

    // Assign a or b to next item to read by a reader
    if (isAhead && !bIsDone) {
        isAhead = !isAhead;
        // find next b
        for (int i = b - 1; i >= 0; i--) {
            if (!dm->index(i,0).data(G::MetadataLoadedColumn).toBool()) {
                b = i;
                return;
            }
        }
    }
    if (isAhead && bIsDone) {
        // find next a
        for (int i = a + 1; i < dmRowCount; i++) {
            if (!dm->index(i,0).data(G::MetadataLoadedColumn).toBool()) {
                a = i;
                return;
            }
        }
    }
    if (!isAhead && !aIsDone) {
        isAhead = !isAhead;
        // find next a
        for (int i = a + 1; i < dmRowCount; i++) {
            if (!dm->index(i,0).data(G::MetadataLoadedColumn).toBool()) {
                a = i;
                return;
            }
        }
    }
    if (isAhead && bIsDone) {
        // find next b
        for (int i = b - 1; i >= 0; i--) {
            if (!dm->index(i,0).data(G::MetadataLoadedColumn).toBool()) {
                b = i;
                return;
            }
        }
    }

}

void MetaRead2::dispatchReaders()
{
    if (isDebug || G::isLogger) G::log("MetaRead2::startThumbDecoders");
    if (isDebug) {
    qDebug() << "MetaRead2::startThumbDecoders"
             << "decoderCount =" << readerCount
                ;
    }

    for (int id = 0; id < readerCount; id++) {
        if (!reader[id]->isRunning()) {
            reader[id]->fPath = "";
            dispatch(id);
        }
    }
}

void MetaRead2::run()
/*
    Loads the metadata and icons into the datamodel (dm) for a folder.  The iteration
    proceeds from the start row in an ahead/behind progression.
*/
{
    if (isDebug || G::isLogger || G::isFlowLogger) G::log("MetaRead2::read", src);
    if (isDebug)
    {
        qDebug().noquote() << "MetaRead2::run"
                           << "src =" << src
                           << "startRow =" << startRow
                           << "a =" << a
                           << "b =" << b
                           << "sfRowCount =" << sfRowCount
                              ;
    }
    dispatchReaders();
}
