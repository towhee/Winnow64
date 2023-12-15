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

    this->fileSelectionChanged = fileSelectionChanged;

    if (isRunning() && (aIsDone && bIsDone)) stop();

    if (isDebug)
    {
        qDebug() << "\nMetaRead2::setStartRow "
                 << "startRow =" << startRow
                 << "fileSelectionChanged =" << fileSelectionChanged
                 << "G::allMetadataLoaded =" << G::allMetadataLoaded
                 << "G::allIconsLoaded =" << G::allIconsLoaded
                 << "src =" << src
            ;
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
    //QMutexLocker locker(&mutex);
    //mutex.lock();
    this->src = src;
    sfRowCount = dm->sf->rowCount();
    lastRow = sfRowCount - 1;
    if (row >= 0 && row < sfRowCount) startRow = row;
    else startRow = 0;
    aIsDone = false;
    bIsDone = false;
    if (startRow == 0) bIsDone = true;
    if (startRow == sfRowCount - 1) aIsDone = true;
    setIconRange();
    abortCleanup = isRunning();
    isNewStartRowWhileStillReading = false;
    isDone = false;
   // mutex.unlock();

//    qDebug().noquote()
//        << "            MetaRead2::setStartRow                   "
//        << QString::number(G::t.elapsed()).rightJustified((5)) << "ms"; G::t.restart();

    if (isDebug)
    {
    qDebug() << "\nMetaRead2::setStartRow "
             << "startRow =" << startRow
             << "firstIconRow =" << firstIconRow
             << "lastIconRow =" << lastIconRow
             << "iconChunkSize =" << dm->iconChunkSize
             << "fileSelectionChanged =" << fileSelectionChanged
             << "G::allMetadataLoaded =" << G::allMetadataLoaded
             << "G::allIconsLoaded =" << G::allIconsLoaded
             << "folder =" << dm->currentFolderPath
             << "isRunning =" << isRunning()
             << "isDispatching =" << isDispatching
             << "isDone =" << isDone
            ;
    }

    if (G::useUpdateStatus) emit runStatus(true, true, "MetaRead2::run");

    if (isRunning()) {
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
        b = startRow - 1;
        start();
    }
}

bool MetaRead2::stop()
{
    if (G::isLogger || G::isFlowLogger) G::log("MetaRead2::stop");
    if (isDebug) {
        qDebug() << "MetaRead2::stop";
    }

    // stop MetaRead2 first so not calling readers
    if (isRunning()) {
        mutex.lock();
        abort = true;
        condition.wakeOne();
        mutex.unlock();
        // quit() signals event loop in run() to terminate
        quit();
        wait();
    }

    // stop all readers
    for (int id = 0; id < readerCount; ++id) {
        reader[id]->stop();
    }

    abort = false;
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
    if (isDebug)
    {
        qDebug() << "\nMetaRead2::dispatch     initialize          "
                 << dm->currentFolderPath;
    }
    abort = false;
    dmRowCount = dm->rowCount();
    metaReadCount = 0;
    metaReadItems = dmRowCount;
    instance = dm->instance;
    isAhead = true;
    aIsDone = false;
    bIsDone = false;
    isDone = false;
    quitAfterTimeoutInitiated = false;
    redoCount = 0;
    err.clear();
    toRead.clear();
    for (int i = 0; i < dmRowCount; i++) {
        toRead.append(i);
    }
}

void MetaRead2::setIconRange()
{
    if (dm->iconChunkSize > sfRowCount) {
        firstIconRow = 0;
        lastIconRow = sfRowCount - 1;
        return;
    }
    // adjust icon range to startRow
    int half = dm->iconChunkSize / 2;
    bool closeToStart = startRow < half;
    bool closeToEnd = startRow > sfRowCount - half;
    if (closeToStart) {
        firstIconRow = 0;
        lastIconRow = firstIconRow + dm->iconChunkSize - 1;
        return;
    }
    if (closeToEnd) {
        lastIconRow = lastRow;
        firstIconRow = lastIconRow - dm->iconChunkSize + 1;
        return;
    }
    firstIconRow = startRow - half;
    lastIconRow = firstIconRow + dm->iconChunkSize - 1;
}

QString MetaRead2::diagnostics()
{
    if (isDebug || G::isLogger) G::log("MetaRead2::diagnostics");

    QString reportString;
    QTextStream rpt;
    rpt.setString(&reportString);
    rpt << Utilities::centeredRptHdr('=', objectName() + " MetaRead2 Diagnostics");
    rpt << "\n" ;
    rpt << "\n" << "Load algorithm:           " << (G::isLoadLinear == true ? "Linear" : "Concurrent");
    rpt << "\n" << "instance:                 " << instance;
    rpt << "\n";
    rpt << "\n" << "expansionFactor:          " << expansionFactor;
    rpt << "\n";
    rpt << "\n" << "sfRowCount:               " << sfRowCount;
    rpt << "\n" << "dm->currentSfRow:         " << dm->currentSfRow;
    rpt << "\n";
    rpt << "\n" << "defaultIconChunkSize:     " << dm->defaultIconChunkSize;
    rpt << "\n" << "iconChunkSize:            " << dm->iconChunkSize;
    rpt << "\n" << "iconLimit:                " << iconLimit;
    rpt << "\n" << "firstIconRow:             " << firstIconRow;
    rpt << "\n" << "lastIconRow:              " << lastIconRow;
    rpt << "\n" << "dm->iconCount:            " << dm->iconCount();
    rpt << "\n";
    QStringList s;
    for (int i : dm->metadataNotLoaded()) s << QString::number(i);
    rpt << "\n" << "dm->isAllMetadataLoaded:  " << QVariant(dm->isAllMetadataLoaded()).toString();
    if (s.size()) rpt << "  " << s.join(",");
    rpt << "\n" << "G::allMetadataLoaded:     " << QVariant(G::allMetadataLoaded).toString();
    rpt << "\n" << "G::allIconsLoaded:        " << QVariant(G::allIconsLoaded).toString();
    rpt << "\n";
    rpt << "\n" << "isDone:                   " << QVariant(isDone).toString();
    rpt << "\n" << "aIsDone && bIsDone:       " << QVariant(aIsDone && bIsDone).toString();
    rpt << "\n";
    rpt << "\n" << "abort:                    " << QVariant(abort).toString();
    rpt << "\n" << "isRunning:                " << QVariant(isRunning()).toString();
    rpt << "\n" << "isDispatching:            " << QVariant(isDispatching).toString();
    rpt << "\n";
    rpt << "\n" << "redoCount:                " << QVariant(redoCount).toString();
    rpt << "\n" << "redoMax:                  " << QVariant(redoMax).toString();

    // toRead
    rpt << "\n";
    rpt << "\n" << "toRead: " << toRead.length() << " items";
    for (int i = 0; i < toRead.length(); i++) {
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
        rpt << "\n" << err.at(i);
    }

    // readers
    rpt << "\n";
    rpt << "\n";
    rpt << "Reader status:";
    for (int id = 0; id < readerCount; id++) {
        rpt.setFieldAlignment(QTextStream::AlignRight);
        rpt.setFieldWidth(4);
        rpt << "\n" << id;
        rpt.setFieldAlignment(QTextStream::AlignLeft);
        rpt.setFieldWidth(11);
        rpt << " isRunning";
        rpt.setFieldWidth(7);
        rpt << QVariant(reader[id]->isRunning()).toString();
        rpt.setFieldWidth(8);
        rpt << "pending";
        rpt.setFieldWidth(7);
        rpt << QVariant(reader[id]->pending).toString();
        rpt << "status " << reader[id]->statusText.at(reader[id]->status);
    }

    // rows with icons
    rpt << "\n";
    rpt << "\n";
    rpt.setFieldAlignment(QTextStream::AlignLeft);
    rpt << "Icon rows in datamodel:";
    rpt.setFieldAlignment(QTextStream::AlignRight);
    rpt.setFieldWidth(9);
    for (int i = 0; i < dm->rowCount(); i++) {
        if (dm->itemFromIndex(dm->index(i,0))->icon().isNull()) continue;
        rpt << "\n" << i;
    }

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
*/
    if (isDebug || G::isLogger) G::log("MetaRead2::cleanupIcons");

    // check if datamodel size is less than assigned icon cache chunk size
    if (dm->iconChunkSize >= sfRowCount) {
        return;
    }

    for (int i = 0; i < firstIconRow; i++) {
        if (!dm->index(i, 0).data(Qt::DecorationRole).isNull()) {
            dm->setData(dm->index(i, 0), nullIcon, Qt::DecorationRole);
        }
    }
    for (int i = lastIconRow + 1; i < sfRowCount; i++) {
        if (!dm->index(i, 0).data(Qt::DecorationRole).isNull()) {
            dm->setData(dm->index(i, 0), nullIcon, Qt::DecorationRole);
        }
    }
}

inline bool MetaRead2::needToRead(int row)
/*
    Returns true if either the metadata or icon (thumbnail) has not been read.
*/
{
    if (!dm->sf->index(row, G::MetadataLoadedColumn).data().toBool()) {
        return true;
    }
    if (row >= firstIconRow && row <= lastIconRow) {
        if (dm->sf->index(row, 0).data(Qt::DecorationRole).isNull()) {
            return true;
        }
    }
    return false;
}

bool MetaRead2::nextA()
{
    // find next a
    //QMutexLocker locker(&mutex);
    if (aIsDone) return false;
    for (int i = a; i < sfRowCount; i++) {
        if (needToRead(i)) {
            nextRow = i;
            a = i + 1;
            if (!bIsDone) isAhead = false;
            return true;
        }
    }
    a = sfRowCount;
    aIsDone = true;
    return false;
}

bool MetaRead2::nextB()
{
    // find next b
    //QMutexLocker locker(&mutex);
    if (bIsDone) return false;
    for (int i = b; i >= 0; i--) {
        if (needToRead(i)) {
            nextRow = i;
            b = i - 1;
            if (!aIsDone) isAhead = true;
            return true;
        }
    }
    b = -1;
    bIsDone = true;
    return false;
}

bool MetaRead2::nextRowToRead()
/*
    Alternate searching ahead/behind for the next datamodel row missing metadata or icon.
    Only search for missing icons within the iconChunk range.

    a (ahead row) or b (behind row) is updated.
*/
{
//    // direction: ahead or behind
//    if (isAhead && aIsDone) isAhead = false;
//    if (!isAhead && bIsDone) isAhead = true;

    // ahead
    if (isAhead) {
        if (nextA()) return true;
        else if (nextB()) return true;
        else return false;
    }
    else {
        if (nextB()) return true;
        else if (nextA()) return true;
        else return false;
    }
}

int MetaRead2::pending()
{
    int pendingCount = 0;
    for (int id = 0; id < readerCount; ++id) {
        if (reader[id]->pending) {
            pendingCount ++;
            if (isDebug)
            {
                qDebug().noquote()
                    << "MetaRead2::pending                          "
                    << "id =" << QString::number(id).leftJustified(2, ' ')
                    << "row =" << QString::number(reader[id]->dmIdx.row()).leftJustified(4, ' ')
                    << "allMetadataLoaded =" << G::allMetadataLoaded
                    ;
            }
        }
    }
    return pendingCount;
}

void MetaRead2::redo()
{
    if (isDebug)
    {
        qDebug() << "MetaRead2::redo                             "
                 << "count =" << redoCount << toRead;
    }
    redoCount++;
    aIsDone = false;
    bIsDone = false;
    a = startRow;
    b = startRow;
    dispatchReaders();
}

void MetaRead2::dispatch(int id)
{
/*
    All available readers (each with their own thread) are sent here by dispatchReaders().
    Each reader is assigned a file, which it reads and then updates the datamodel and
    imagecache.  When the reader is finished, it signals this function, where it iterates,
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
        - quit if all read (return)
    - if a is done and b is done then return
    - if ahead then reader a, else reader b
    - increment a or decrement b to next datamodel row without metadata or icon
*/
    if (abort) {
        if (isDebug) {
        qDebug().noquote()
            << "MetaRead2::dispatch finishing"
            << "reader =" << id
            << "abort =" << abort
            << "isDone =" << isDone
            << "reader instance =" << reader[id]->instance
            << "dm instance =" << dm->instance
               ;
        }
        quit();
        return;
    }

    reader[id]->pending = false;
    //static bool isAhead = true;

    // terse pointer to reader[id]
    d = reader[id];

    // New reader and less rows than readers, end reader
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

    /* RETURNING READER
       - track metadata rows read by depleting toRead
       - report read failures
       - trigger fileSelectionChange if reader row = startRow
       - update progress
       - if readers have been dispatched for all rows
         - check if still pending
         - check if all metadata and necessary icons loaded
         - if all checks out signal done
    */
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

        // trigger fileSelectionChange which starts ImageCache if this row = startRow
        if (fileSelectionChanged && dmRow == startRow) {
            QModelIndex  sfIdx = dm->proxyIndexFromModelIndex(d->dmIdx);  // rghZ
            if (isDebug) // trigger fileSelectionChange
            {
            qDebug().noquote()
                << "MetaRead2::dispatch     fileSelectionChange "
                << "id =" << QString::number(id).leftJustified(2, ' ')
                << "row =" << QString::number(dmRow).leftJustified(4, ' ')
                << d->fPath;
            }
            if (!abort) emit fileSelectionChange(sfIdx);
        }

        if (isDebug)  // returning reader, row has been processed by reader
        if (toRead.size() < 30) // only last 30 rows in datamodel
        {
            bool allLoaded = (dm->isAllMetadataLoaded() && dm->allIconsLoaded());
            qDebug().noquote()
                << "MetaRead2::dispatch     processed:          "
                << "id =" << QString::number(id).leftJustified(2, ' ')
                //<< "startRow =" << QString::number(startRow).leftJustified(4, ' ')
                << "row =" << QString::number(dmRow).leftJustified(4, ' ')
                << "isRunning =" << QVariant(d->isRunning()).toString().leftJustified(5)
                << "allLoaded =" << QVariant(allLoaded).toString().leftJustified(5)
                << "toRead =" << toRead.size()
                << "rowCount =" << dm->rowCount()
                << "a =" << QString::number(a).leftJustified(4, ' ')
                << "b =" << QString::number(b).leftJustified(4, ' ')
                << "startRow =" << QString::number(startRow).leftJustified(4, ' ')
                << "isReadIcon =" << QVariant(d->isReadIcon).toString().leftJustified(5)
                << "iconLoaded =" << QVariant(dm->iconLoaded(dmRow, instance)).toString().leftJustified(5)
                //<< "isRunning =" << QVariant(isRunning()).toString().leftJustified(5)
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

            // have all metadata and icons been loaded into datamodel
            bool allLoaded;
            G::allMetadataLoaded = dm->isAllMetadataLoaded();
            if (dm->iconChunkSize >= sfRowCount) {
                G::allIconsLoaded = dm->allIconsLoaded();
                allLoaded = (G::allMetadataLoaded && G::allIconsLoaded);
            }
            else {
                allLoaded = (G::allMetadataLoaded && dm->allIconChunkLoaded(firstIconRow, lastIconRow));
            }

            if (isDebug)  // dispatch for all rows completed
            {
                qDebug().noquote()
                    << "MetaRead2::dispatch     aIsDone && bIsDone  "
                    << "id =" << QString::number(id).leftJustified(2, ' ')
                    << "row =" << QString::number(reader[id]->dmIdx.row()).leftJustified(4, ' ')
                    << "allLoaded =" << QVariant(allLoaded).toString().leftJustified(5)
                    << "toRead =" << toRead.size()
                    << "pending =" << pending()
                    << "isDone =" << isDone
                    << "metaReadCount =" << metaReadCount
                    << "toRead =" << toRead
                    ;
            }

            if (!allLoaded) {
                // are there still pending readers
                if (pending()) return;
                // if all readers finished but not all loaded, then redo
                else {
                    if (redoCount < redoMax) {
                        if (isDebug) {
                            qDebug()
                                << "MetaRead2::dispatch     Redo                "
                                << "redoCount =" << redoCount
                                << "redoMax =" << redoMax
                                ;
                        }
                        // try to read again
                        if (!abort) {
                            redo();
                        }
                    }
                }
            }

            // Now we are done
            if (!isDone) {
                if (G::useUpdateStatus) emit runStatus(false, true, "MetaRead2::dispatch");
                if (!abort) cleanupIcons();
                if (G::isLogger || G::isFlowLogger)  G::log("MW::dispath", "Done");
                if (isDebug)
                qDebug().noquote()
                    << "            MetaRead2::dispatch     We Are Done.     "
                    << QString::number(G::t.elapsed()).rightJustified((5)) << "ms"
                    << dm->currentFolderPath
                    << "toRead =" << toRead
                    << "pending =" << pending()
                    ;
                if (!abort) emit done();
                isDone = true;
                quit();
            }
            return;

        } // end aIsDone && bIsDone

    } // end returning reader

    // check if new start row while dispatch reading all metadata.  The user may have jumped
    // to the end of a large folder while metadata is being read.  Also could have scrolled.
    if (isNewStartRowWhileStillReading) {
        a = startRow;
        b = startRow - 1;
        isNewStartRowWhileStillReading = false;
        //if (isDebug)
        {
            qDebug().noquote()
                << "MetaRead2::dispatch isNewStartRowWhileStillReading"
                << "startRow =" << QString::number(startRow).leftJustified(4, ' ')
                << "a =" << QString::number(a).leftJustified(4, ' ')
                << "b =" << QString::number(b).leftJustified(4, ' ')
                ;
        }
    }

//    aIsDone = (a == sfRowCount);
//    bIsDone = (b == -1);


    // launch a reader

    // assign either a or b as the next row to read in the datamodel
    if (nextRowToRead()) {
        QModelIndex dmIdx = dm->index(nextRow, 0);
        QString fPath = dmIdx.data(G::PathRole).toString();
        // only read icons within the icon chunk range
        bool isReadIcon = (nextRow >= firstIconRow && nextRow <= lastIconRow);
        if (isDebug)
        {
            qDebug().noquote()
                << "MetaRead2::dispatch     read current item   "
                << "id =" << QString::number(id).leftJustified(2, ' ')
                << "row =" << QString::number(nextRow).leftJustified(4, ' ')
                << "isReadIcon =" << QVariant(isReadIcon).toString().leftJustified(5, ' ')
                << "isAhead =" << QVariant(isAhead).toString().leftJustified(5, ' ')
                << "aIsDone =" << QVariant(aIsDone).toString().leftJustified(5, ' ')
                << "bIsDone =" << QVariant(bIsDone).toString().leftJustified(5, ' ')
                << "a =" << QString::number(a).leftJustified(4, ' ')
                << "b =" << QString::number(b).leftJustified(4, ' ')
                ;
        }

        /* read the image file metadata and icon.  When the reader is finished, it will
           signal dispatch to loop through to read another file...  */
        if (!abort) reader[id]->read(dmIdx, fPath, instance, isReadIcon);
    }

    // if done in both directions return
    if (aIsDone && bIsDone) {
        if (!quitAfterTimeoutInitiated) {
            if (isDebug)
            {
                qDebug().noquote()
                    << "MetaRead2::dispatch aIsDone && bIsDone   "
                    << QString::number(G::t.elapsed()).rightJustified((5)) << "ms"; G::t.restart();}
            quitAfterTimeoutInitiated = true;
            isDispatching = false;
            // if pending readers not all processed in delay ms then quit anyway
            int delay = 1000;
            //qDebug() << "MetaRead2::dispatch     aIsDone && bIsDone  quitAfterTimeoutInitiated in" << delay << "ms";
            QTimer::singleShot(delay, this, &MetaRead2::quitAfterTimeout);
        }
        return;
    }

    isDispatching = true;

}

void MetaRead2::dispatchReaders()
{
    if (G::isLogger) G::log("MetaRead2::dispatchReaders");
    if (isDebug) {
    qDebug().noquote()
             << "MetaRead2::dispatchReaders"
             << "readerCount =" << readerCount
                ;
    }

    for (int id = 0; id < readerCount; id++) {
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

void::MetaRead2::quitAfterTimeout()
{
    if (!isDone) {
        if ((!pending()) && (redoCount < redoMax)) {
            if (isDebug) {
                qDebug()
                    << "MetaRead2::dispatch     Redo                "
                    << "redoCount =" << redoCount
                    << "redoMax =" << redoMax
                    ;
            }
            // try to read again
            if (!abort) {
                redo();
            }
        }

        if (G::isLogger || G::isFlowLogger)  G::log("MW::quitAfterTimeout", "Done");

        if (isDebug)
        {
            qDebug().noquote()
                 << "MetaRead2::quitAfterTimeout  We Are Done."
                 << QString::number(G::t.elapsed()).rightJustified((5)) << "ms"
                 << dm->currentFolderPath << "toRead =" << toRead;
        }
        if (!abort) cleanupIcons();
        if (G::useUpdateStatus) emit runStatus(false, true, "MetaRead2::quitAnyway");
        if (!abort) emit done();
        quit();
    }
}

void MetaRead2::run()
/*
    Loads the metadata and icons into the datamodel (dm) for a folder.  The iteration
    proceeds from the start row in an ahead/behind progression.

    Readers are dispatched and iterate in the dispatch function via signals.  As a result,
    we loop after calling dispatchReaders(), waiting for a finished signal.  The finished
    signal is sent from the QThread quit function, which is called from dispatch() when
    abort == true or all the datamodel has been loaded.
*/
{
    if (G::isLogger || G::isFlowLogger) G::log("MetaRead2::run", src);
    if (isDebug)
    {
        qDebug().noquote() << "MetaRead2::run                             "
                           << "startRow =" << startRow
                           << "a =" << a
                           << "b =" << b
                           << "sfRowCount =" << sfRowCount
                              ;
    }

    isDone = false;

    QEventLoop loop;
    connect(this, &MetaRead2::finished, &loop, &QEventLoop::quit);
    dispatchReaders();
    loop.exec();
}
