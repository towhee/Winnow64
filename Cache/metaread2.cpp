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

    Debug function      col X
          Desc          col +24
          Param         col +44
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
    if (G::isLogger || G::isFlowLogger) {
        QString running;
        isRunning() ? running = "true" : running = "false";
        QString s = "row = " + QString::number(row) + " src = " + src + " isRunning = " + running;
        G::log("MetaRead2::setCurrentRow", s);
    }

    // All loaded, select file
    if (G::allMetadataLoaded && G::allIconsLoaded) {
        if (fileSelectionChanged) {
            //qDebug() << "Just do fileSelectionChange";
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
    abortCleanup = isRunning();
    isNewStartRowWhileStillReading = false;
    mutex.unlock();

    if (isDebug)
    {
    qDebug() << "\nMetaRead2::setStartRow "
             << "startRow =" << startRow
             << "fileSelectionChanged =" << fileSelectionChanged
             << "folder =" << dm->currentFolderPath
             << "isRunning =" << isRunning()
             << "isDispatching =" << isDispatching
             << "isDone =" << isDone
            ;
    }
    if (G::useUpdateStatus) emit runStatus(true, true, "MetaRead2::run");

    if (isDispatching) {
        //qDebug() << "MetaRead2::setStartRow isNewStartRowWhileStillReading = true";
        mutex.lock();
        isNewStartRowWhileStillReading = true;
        mutex.unlock();
    }
    else {
        if (isDebug)
        qDebug() << "MetaRead2::setStartRow                      "
                 << "Not dispatching so start()";
        isNewStartRowWhileStillReading = false;
        a = startRow;
        b = startRow;
        start();
    }
}

bool MetaRead2::stop()
{
    if (G::isLogger || G::isFlowLogger) G::log("MetaRead2::stop");
    if (isDebug)
        qDebug() << "MetaRead2::stop";
    mutex.lock();
    abort = true;
    mutex.unlock();

    // stop all readers
    for (int id = 0; id < readerCount; ++id) {
        if (isDebug)
        qDebug() << "MetaRead2::stop   stopping reader" << id;
        reader[id]->stop();
    }

    // stop MetaRead2
    if (isRunning()) {
        mutex.lock();
        abort = true;
        condition.wakeOne();
        mutex.unlock();
        wait();
    }

    //abort = false;
    if (isDebug)
    {
    qDebug() << "MetaRead2::stop"
             << "isRunning =" << isRunning()
        ;
    }
    return isRunning();
}

void MetaRead2::initialize()
/*
    Called when change folders.
*/
{
    if (G::isLogger || G::isFlowLogger)
    {
        G::log("MetaRead2::initialize");
    }
    abort = false;
    rowsWithIcon.clear();
    dmRowCount = dm->rowCount();
    metaReadCount = 0;
    metaReadItems = dmRowCount;
    redoCount = 0;
    instance = dm->instance;
    isDispatching = false;
    firstDone = false;
    isDone = false;
    //allDone = false;
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
    rpt << "\n" << "isDispatching:         " << (isDispatching ? "true" : "false");
    rpt << "\n" << "isDone:                " << (isDone ? "true" : "false");
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
    rpt << "\n";
    rpt << "\n" << "redoCount:             " << QVariant(redoCount).toString();
    rpt << "\n" << "isAllMetadataLoaded:   " << QVariant(dm->isAllMetadataLoaded()).toString();
    rpt << "\n" << "isDone:                " << QVariant(isDone).toString();

    rpt << "\n";
    rpt << "\n" << "toRead: " << toRead.length() << " items";
    for (int i = 0; i < toRead.length(); i++) {
        //rpt.setFieldAlignment(QTextStream::AlignLeft);
        rpt << "\n";
        rpt << "Item at row ";
        rpt.setFieldWidth(5);
        rpt << toRead.at(i) ;
        rpt << " dm metadata loaded ";
        rpt << QVariant(dm->isMetadataLoaded(toRead.at(i))).toString();
    }
    rpt << "\n";
    rpt << "\n" << "Errors: " << err.length() << " items";
    rpt.setFieldWidth(5);
    for (int i = 0; i < err.length(); i++) {
        //rpt.setFieldAlignment(QTextStream::AlignLeft);
        rpt << "\n" << err.at(i);
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

void MetaRead2::redo()
{
    //if (isDebug)
    {
        qDebug() << "MetaRead2::redo" << "count =" << redoCount << toRead;
    }
    redoCount++;
    a = startRow;
    b = startRow;
    dispatchReaders();
}

void MetaRead2::dispatch(int id)
{
/*
    All available readers (each with their own thread) are sent here by dispatchReaders.
    Each reader is assigned a file, which it reads and then updates the datamodel and
    imagecache.  When it is finished, the reader signals this function, where it iterates,
    being dispatched to read another file until all the files have been read by the
    readers.

    From the startRow position, each datamodel row file is read, alternating ahead and
    behind.

    Initial conditions: a = b = startRow, isHead = true
    a = row in ahead position
    b = row in behind position
    readers call back to dispatch

    - if reader returning
        - status update
        - deplete toRead list
        - quit if all read
    - if a is done and b is done then return
    - if ahead then reader a, else reader b
    - increment a or decrement b to next datamodel row without metadata
*/
    reader[id]->pending = false;
    if (abort || isDone /*|| reader[id]->instance != dm->instance*/) {
        if (isDebug)
        qDebug().noquote()
            << "MetaRead2::dispatch finishing"
            << "reader =" << id
            << "abort =" << abort
            << "isDone =" << isDone
            << "reader instance =" << reader[id]->instance
            << "dm instance =" << dm->instance
               ;
        return;
    }

    static bool isAhead = true;

    // terse pointer to reader[id]
    d = reader[id];

    // If not new reader and all rows already being read, then return
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
            //<< "allRead =" << QVariant(allDone).toString().leftJustified(5, ' ')
            << "a =" << QString::number(a).leftJustified(4, ' ')
            << "b =" << QString::number(b).leftJustified(4, ' ')
            ;
    }

    // returning reader
    if (d->fPath != "" && d->instance == dm->instance) {
        int dmRow = d->dmIdx.row();

        // deplete toRead
        for (int i = 0; i < toRead.size(); i++) {
            if (toRead.at(i) == dmRow) {
                toRead.removeAt(i);
                break;
            }
        }

        // report read failure
        if (!(d->status == d->Status::Success)) {
            QString error = "row " + QString::number(dmRow).rightJustified(5)
                            + d->statusText.at(d->status) + " " + d->fPath;
            err.append(error);
            if (isDebug)
            {
            qDebug() << "MetaRead2::dispatch  Failure    "
                     << "row =" << dmRow
                     << "status =" << d->statusText.at(d->status)
                     << d->fPath;
            }
        }

        // track rows with icon
        if (!abort) rowsWithIcon.append(dmRow);

        // trigger fileSelectionChange which starts ImageCache if this row = startRow
        if (dmRow == startRow) {
            QModelIndex  sfIdx = dm->proxyIndexFromModelIndex(d->dmIdx);  // rghZ
            if (isDebug)
            {
            qDebug() << "MetaRead2::dispatch     fileSelectionChange "
                     << "id =" << QString::number(id).leftJustified(2, ' ')
                     << "row =" << QString::number(dmRow).leftJustified(4, ' ')
                     << d->fPath;
            }
            if (!abort) emit fileSelectionChange(sfIdx);
        }

        if (isDebug)
        {
            qDebug().noquote()
                << "MetaRead2::dispatch     processed:          "
                << "id =" << QString::number(id).leftJustified(2, ' ')
                << "startRow =" << QString::number(startRow).leftJustified(4, ' ')
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

        // if readers have been dispatched for all rows
        if (aIsDone && bIsDone) {
            isDispatching = false;
            G::allMetadataLoaded = dm->isAllMetadataLoaded();
            G::allIconsLoaded = dm->allIconsLoaded();
            if (isDebug)
            {
            qDebug().noquote()
                << "MetaRead2::dispatch     aIsDone && bIsDone  "
                << "id =" << QString::number(id).leftJustified(2, ' ')
                << "row =" << QString::number(reader[id]->dmIdx.row()).leftJustified(4, ' ')
                << "allMetadataLoaded =" << dm->isAllMetadataLoaded();
            }

            // are there still pending readers
            bool stillReadersPending = false;
            int pendingCount = 0;
            if (!G::allMetadataLoaded || !G::allIconsLoaded) {
                for (int id = 0; id < readerCount; ++id) {
                    if (reader[id]->pending /*|| reader[id]->isRunning()*/) {
                        stillReadersPending = true;
                        pendingCount ++;
                        if (isDebug)
                        {
                            qDebug().noquote()
                                << "MetaRead2::dispatch     ThreadsStillPending:"
                                << "id =" << QString::number(id).leftJustified(2, ' ')
                                << "row =" << QString::number(reader[id]->dmIdx.row()).leftJustified(4, ' ')
                                << "allMetadataLoaded =" << dm->isAllMetadataLoaded() ;
                        }
                        //break;
                    }
                }
                // if pending then still waiting for return
                if (pendingCount) {
                    return;
                }
            }

            // if all readers finished has all metadata been added to datamodel
            redoMax = 0;  // do not redo
            if ((toRead.size() > 0 || !G::allMetadataLoaded) && (redoCount < redoMax)) {
                //qDebug() << "MetaRead2::dispatch     No readers pending";
                qDebug() << "MetaRead2::dispatch     Not all metadata loaded";
                // try to read again
                if (!abort) {
                    redo();
                }
            }

            // Now we are done
            if (G::useUpdateStatus) emit runStatus(false, true, "MetaRead2::run");
            if (!isDone) {
                if (!abort) cleanupIcons();
                if (isDebug)
                qDebug() << "MetaRead2::dispatch     We Are Done.         "
                         << dm->currentFolderPath << "toRead =" << toRead;
                if (!abort) emit done();
                isDone = true;
            }
            return;

        } // end aIsDone && bIsDone

        /*
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
                // if not all metadata loaded then redo up to redoMax times
                if (!stillReadersPending && !dm->isAllMetadataLoaded()) {
                    if (!abort) {
                        // redo unless redo limit exceeded
                        if (redo()) return;
                    }
                }

                // Now we are done
                if (G::useUpdateStatus) emit runStatus(false, true, "MetaRead2::run");
                if (!isDone) {
                    if (!abort) cleanupIcons();
                    qDebug() << "MetaRead2::dispatch     We Are Done.  toRead =" << toRead;
                    if (!abort) emit done();
                    isDone = true;
                    isStarted = false;
                }
            }

            return;
        }
        */
    }

    // check if new start row while dispatch reading all metadata.  The user may have jumped
    // to the end of a large folder while metadata is being read.
    if (isNewStartRowWhileStillReading) {
        a = startRow;
        b = startRow;
        isNewStartRowWhileStillReading = false;
        if (isDebug)
        {
            qDebug().noquote()
                << "MetaRead2::dispatch isNewStartRowWhileStillReading"
                << "a =" << QString::number(a).leftJustified(4, ' ')
                << "b =" << QString::number(b).leftJustified(4, ' ')
                ;
        }
    }

    aIsDone = (a == dmRowCount);
    bIsDone = (b == -1);

    // if done in both directions invoke quitAnyway with a delay and return
    if (aIsDone && bIsDone) {
        if (!firstDone) {
            firstDone = true;
            isDispatching = false;
            // if pending readers not all processed in delay ms then quit anyway
            int delay = 10000;
//            qDebug() << "MetaRead2::dispatch     aIsDone && bIsDone  quitAnyway in" << delay << "ms";
//            QTimer::singleShot(delay, this, &MetaRead2::quitAnyway);
        }
        return;
    }

    // launch a reader
    if (isAhead && aIsDone) isAhead = false;
    if (!isAhead && bIsDone) isAhead = true;
    int dmRow;
    isAhead ? dmRow = a : dmRow = b;
    QModelIndex dmIdx = dm->index(dmRow, 0);
    QString fPath = dmIdx.data(G::PathRole).toString();
    // rgh change to if in icon range isReadIcon = true
    bool isReadIcon = true;
    // read the image file metadata and icon.  When the reader is finished, it will
    // signal dispatch to loop through to read another file...
    if (isDebug)
    {
        qDebug().noquote()
            << "MetaRead2::dispatch     read current item   "
            << "id =" << QString::number(id).leftJustified(2, ' ')
            << "row =" << QString::number(dmRow).leftJustified(4, ' ')
            << "isAhead =" << QVariant(isAhead).toString().leftJustified(5, ' ')
            << "aIsDone =" << QVariant(aIsDone).toString().leftJustified(5, ' ')
            << "bIsDone =" << QVariant(bIsDone).toString().leftJustified(5, ' ')
            << "a =" << QString::number(a).leftJustified(4, ' ')
            << "b =" << QString::number(b).leftJustified(4, ' ')
            ;
    }

    if (!abort) reader[id]->read(dmIdx, fPath, instance, isReadIcon);
    //reader[id]->pending = true;

    // Update a or b to next item to read by a reader
    if (isAhead && !bIsDone) {
        isAhead = !isAhead;
        // find next b
        for (int i = b - 1; i >= 0; i--) {
            if (!dm->index(i,0).data(G::MetadataLoadedColumn).toBool()) {
                b = i;
                return;
            }
        }
        b--;
    }
    if (isAhead && bIsDone) {
        // find next a
        for (int i = a + 1; i < dmRowCount; i++) {
            if (!dm->index(i,0).data(G::MetadataLoadedColumn).toBool()) {
                a = i;
                return;
            }
        }
        a++;
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
        a++;
    }
    if (!isAhead && aIsDone) {
        // find next b
        for (int i = b - 1; i >= 0; i--) {
            if (!dm->index(i,0).data(G::MetadataLoadedColumn).toBool()) {
                b = i;
                return;
            }
        }
        b--;
    }

}

void MetaRead2::dispatchReaders()
{
    //if (isDebug || G::isLogger) G::log("MetaRead2::dispatchReaders");
    if (isDebug) {
    qDebug().noquote()
             << "MetaRead2::dispatchReaders"
             << "readerCount =" << readerCount
                ;
    }

    for (int id = 0; id < readerCount; id++) {
        if (!reader[id]->isRunning()) {
            if (isDebug) {
                qDebug().noquote()
                    << "MetaRead2::dispatchReaders                  "
                    << "reader =" << id << "dispatched"
                    ;
            }
            reader[id]->fPath = "";
            dispatch(id);
        }
    }
}

void::MetaRead2::quitAnyway()
{
    if (!isDone) {
        isDone = true;
        qDebug() << "MetaRead2::quitAnyway   We Are Done.  toRead =" << toRead;
        if (!abort) emit done();
    }
}

void MetaRead2::run()
/*
    Loads the metadata and icons into the datamodel (dm) for a folder.  The iteration
    proceeds from the start row in an ahead/behind progression.
*/
{
    if (G::isLogger || G::isFlowLogger) G::log("MetaRead2::run", src);
    if (isDebug)
    {
        qDebug().noquote() << "MetaRead2::run                              "
                           << "startRow =" << startRow
                           << "a =" << a
                           << "b =" << b
                           << "sfRowCount =" << sfRowCount
                              ;
    }
    isDispatching = true;
    isDone = false;
    dispatchReaders();
}
