#include "metaread2.h"

/*
    MetaRead2, running in a separate thread, dispatches readers to load the metadata and
    icons into the datamodel (dm), which will already include the file information,
    loaded by DataModel::addFileData. Icons will be loaded up to iconChunkSize, which is
    set to 20000.

    The idealThreadCount number of reader are created.  Readers read the metadata and icon
    and update the DataModel and ImageCache.

    Steps:

        • Call setStartRow.  The start row can be any row in the datamodel.

        • Dispatch is called for each reader.  Each time dispatch is called it iterates
          through the datamodel in a ahead/behind order.

        • Start the ImageCache.

        • When each reader is finished it signals dispatch to be assigned a new file.

        • When all files read clean up excess icons.

        • Abort and restart when a new setCurrentRow is called.

    MetaRead2::setCurrentRow is called when:

        • a new folder is selected
        • a new image is selected
        • thumbnails are scrolled
        • the gridView or thumbView are resized
        • there is an insertion into the datamodel

    Immediately show image in loupe view while MetaRead2 is working

        • if the new start row has already been read, fileSelectionChange is called
          from MW::loadCurrent.
        • if the new start row has not been read, fileSelectionChange is emitted
          from dispatch when a reader returns after reading the start row.

    Note: All data in the DataModel must be set using a queued connection.  When subsequent
    actions are dependent on the data being set use Qt::BlockingQueuedConnection.

    qDebug format:
          function      col X
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

    // create n reader threads
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

void MetaRead2::setStartRow(int sfRow, bool fileSelectionChanged, QString src)
{
    if (G::isLogger || G::isFlowLogger) {
        QString running;
        isRunning() ? running = "true" : running = "false";
        QString s = "row = " + QString::number(sfRow) + " src = " + src + " isRunning = " + running;
        G::log("MetaRead2::setCurrentRow", s);
    }

    this->fileSelectionChanged = fileSelectionChanged;

    // has metadata already been read for this image file
    if (fileSelectionChanged) {
        if (dm->isMetadataAttempted(sfRow)) {
        //if (dm->isMetadataLoaded(sfRow)) {
            emit fileSelectionChange(dm->sf->index(sfRow, 0));
            if (G::metaReadDone) return;
        }
    }

    //if (isRunning() && (aIsDone && bIsDone)) stop();

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

    // load datamodel with metadata and icons
    //QMutexLocker locker(&mutex);
    mutex.lock();
    this->src = src;
    sfRowCount = dm->sf->rowCount();
    lastRow = sfRowCount - 1;
    if (sfRow >= 0 && sfRow < sfRowCount) startRow = sfRow;
    else startRow = 0;
    aIsDone = false;
    bIsDone = false;
    if (startRow == 0) bIsDone = true;
    setIconRange();
    abortCleanup = isRunning();
    isDone = false;
    mutex.unlock();

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
             << "instance =" << instance << "/" << dm->instance
            ;
    }

    if (G::useUpdateStatus) emit runStatus(true, true, "MetaRead2::run");

    if (isDispatching) {
        if (isDebug)
        qDebug() << "MetaRead2::setStartRow isNewStartRowWhileStillReading = true";
        mutex.lock();
        isNewStartRowWhileStillReading = true;
        mutex.unlock();
        for (int id = 0; id < readerCount; id++) {
            if (!reader[id]->isRunning()) {
                reader[id]->status = Reader::Status::Ready;
                reader[id]->fPath = "";
                dispatch(id);
            }
        }
    }
    else {
        if (isDebug)
        qDebug() << "MetaRead2::setStartRow                      "
                 << "Not dispatching so start()";
        isNewStartRowWhileStillReading = false;
        a = startRow;
        b = startRow - 1;
        if (!isRunning()) start();
    }
}

bool MetaRead2::stop()
/*
    MetaRead2::run dispatches all the readers and returns immediately.  The isDispatched flag
    indicates that dispatching is still running.
*/
{
    if (G::isLogger || G::isFlowLogger) G::log("MetaRead2::stop");
    if (isDebug)
    {
        qDebug() << "MetaRead2::stop  instance =" << instance << "/" << dm->instance;
    }

    // stop MetaRead2 first so not calling readers
    /*
    if (isRunning()) {
        mutex.lock();
        abort = true;
        condition.wakeOne();
        mutex.unlock();
        //wait();
    }
    */

    if (isDispatching && !abort) {
        mutex.lock();
        abort = true;
        mutex.unlock();
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
             << "isDispatching =" << isDispatching
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
        G::log("MetaRead2::initialize", dm->currentFolderPath);
    }
    if (isDebug)
    {
        qDebug() << "\nMetaRead2::initialize     initialize      "
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
    /* // to be removed
    toRead.clear();
    for (int i = 0; i < dmRowCount; i++) {
        toRead.append(i);
    }
    //*/
}

void MetaRead2::syncInstance()
{
    if (G::isLogger) G::log("MetaRead2::syncInstance");
    instance = dm->instance;
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
    rpt << "\n" << "dm->isAllMetadataLoaded:  " << QVariant(dm->isAllMetadataAttempted()).toString();
    if (s.size()) rpt << "  " << s.join(",");
    rpt << "\n" << "G::allMetadataLoaded:     " << QVariant(G::allMetadataLoaded).toString();
    rpt << "\n" << "G::allIconsLoaded:        " << QVariant(G::allIconsLoaded).toString();
    rpt << "\n";
    rpt << "\n" << "isDone:                   " << QVariant(isDone).toString();
    rpt << "\n" << "aIsDone && bIsDone:       " << QVariant(aIsDone && bIsDone).toString();
    rpt << "\n";
    rpt << "\n" << "abort:                    " << QVariant(abort).toString();
    rpt << "\n" << "isRunning:                " << QVariant(isRunning()).toString();
    rpt << "\n" << "isRunloop:                " << QVariant(runloop.isRunning()).toString();
    rpt << "\n" << "isDispatching:            " << QVariant(isDispatching).toString();
    rpt << "\n";
    rpt << "\n" << "redoCount:                " << QVariant(redoCount).toString();
    rpt << "\n" << "redoMax:                  " << QVariant(redoMax).toString();

    /* toRead (to be removed)
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
    //*/

    // errors
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
    /*
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
    */
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
    if (G::isLogger) G::log("MetaRead2::cleanupIcons");
    if (isDebug) qDebug() << "MetaRead2::cleanupIcons";

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
    if (!dm->sf->index(row, G::MetadataAttemptedColumn).data().toBool()) {
        return true;
    }
    if (row >= firstIconRow && row <= lastIconRow) {
        if (dm->sf->index(row, 0).data(Qt::DecorationRole).isNull()) {
            return true;
        }
    }

    /* Check if setStartRow called while still reading metadata, and the metadata has been read
       for the new start row.  This enables viewing images while MetaRead2 is still reading
       the metadata and icons.  */
    //if (fileSelectionChanged && row == startRow) emit fileSelectionChange(dm->sf->index(row, 0));

    return false;
}

bool MetaRead2::nextA()
{
    // find next a
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
/*
    Return the number of Readers that have been dispatched and not signalled back
    that they are done.
*/
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
/*
    If not all metadata or icons were successfully read then try again.
*/
{
    if (G::isLogger || G::isFlowLogger)
    {
        G::log("MetaRead2::redo", "count = " + QString::number(redoCount));
    }
    if (isDebug)
    {
        qDebug() << "MetaRead2::redo                             "
                 << "count =" << redoCount;//<< toRead;
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

    Initial conditions: a = startRow, b = a - 1, isAhead = true
    a = row in ahead position
    b = row in behind position
    readers signal back to dispatch

    - if reader returning
        - status update
        - deplete toRead list
        - quit if all read (return)
    - if a is done and b is done then return
    - if ahead then reader a, else reader b
    - increment a or decrement b to next datamodel row without metadata or icon

    Function logic:

        1. Check ok to continue or abort
        2. If returning reader[n]
           - check for instance clash
           - report read failure
           - trigger file selection if fileSelectionChange and is startRow
           - report progress
           - if finished then quit
        3. Dispatch reader[n]
           - assign next row to read
           - call reader[n] to read metadata and icon into datamodel
           - if last row then quit after delay
*/

    // terse pointer to reader[id]
    r = reader[id];
    r->pending = false;

    if (abort || instance != dm->instance) {
        if (isDebug)
        {
        qDebug().noquote()
            << "MetaRead2::dispatch aborting "
            << "reader =" << id
            << "abort =" << abort
            << "isDone =" << isDone
            << "dm instance =" << dm->instance
            << "instance =" << instance
            << "reader instance =" << reader[id]->instance
               ;
        }
        r->status = Reader::Status::Ready;
        r->fPath = "";
        return;
    }

    // New reader and less rows than readers, end reader
    if (r->fPath == "" && id >= dmRowCount) return;

    if (isDebug)
    {
        QString  row;
        r->fPath == "" ? row = "-1" : row = QString::number(r->dmIdx.row());
        QString s;
        if (r->fPath == "") s = "MetaRead2::dispatch     reader starting     ";
        else s = "MetaRead2::dispatch     reader returning    ";
        qDebug().noquote()
            << s
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

    if (G::isLogger || G::isFlowLogger)
    {
        QString  row;
        r->fPath == "" ? row = "-1" : row = QString::number(r->dmIdx.row());
        QString from;
        r->fPath == "" ? from = "start reader" : from = "return from reader";
        QString s = "id = " + QString::number(id).rightJustified(2, ' ');
        s += " row = " + row.rightJustified(4, ' ');
        G::log("MetaRead2::dispatch: " + from, s);
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

    if (r->fPath != "" && r->instance == dm->instance) {
        int dmRow = r->dmIdx.row();

        // progress counter
        metaReadCount++;

        // it is not ok to select while the datamodel is being built.
        if (metaReadCount == 1) emit okToSelect(true);

        /* deplete toRead (to be removed)
        for (int i = 0; i < toRead.size(); i++) {
            if (toRead.at(i) == dmRow) {
                toRead.removeAt(i);
                break;
            }
        }
        //*/

        // report read failure
        if (!(r->status == r->Status::Success)) {
            QString error = "row " + QString::number(dmRow).rightJustified(5)
                            + r->statusText.at(r->status) + " " + r->fPath;
            err.append(error);
            if (isDebug)
            {
            qDebug() << "MetaRead2::dispatch     Failure             "
                     << "id =" << QString::number(id).leftJustified(2, ' ')
                     << "row =" << dmRow
                     << "status =" << r->statusText.at(r->status)
                     << r->fPath;
            }
        }

        // trigger fileSelectionChange which starts ImageCache if this row = startRow
        if (fileSelectionChanged && dmRow == startRow) {
            QModelIndex  sfIdx = dm->proxyIndexFromModelIndex(r->dmIdx);  // rghZ already a filter??
            if (G::isFlowLogger)
            {
                G::log("MetaRead2::dispatch", "fileSelectionChange row = " + QString::number(dmRow));
            }
            if (isDebug) // fileSelectionChange
            {
                /*
                bool isMetaLoaded = dm->isMetadataLoaded(sfIdx.row());
                qDebug().noquote()
                    << "\nMetaRead2::dispatch     fileSelectionChange "
                    << "id =" << QString::number(id).leftJustified(2, ' ')
                    << "row =" << QString::number(dmRow).leftJustified(4, ' ')
                    << "metaLoaded =" << QVariant(isMetaLoaded).toString().leftJustified(5)
                    << r->fPath;
                    //*/
            }
            if (!abort) emit fileSelectionChange(sfIdx);
        }

        if (isDebug)  // returning reader, row has been processed by reader
        //if (toRead.size() < 10) // only last 10 rows in datamodel
        {
            bool allLoaded = (dm->isAllMetadataAttempted() && dm->allIconsLoaded());
            qDebug().noquote()
                << "MetaRead2::dispatch     processed           "
                << "id =" << QString::number(id).leftJustified(2, ' ')
                //<< "startRow =" << QString::number(startRow).leftJustified(4, ' ')
                << "row =" << QString::number(dmRow).leftJustified(4, ' ')
                << "isRunning =" << QVariant(r->isRunning()).toString().leftJustified(5)
                << "allLoaded =" << QVariant(allLoaded).toString().leftJustified(5)
                //<< "toRead =" << toRead.size()
                << "rowCount =" << dm->rowCount()
                << "a =" << QString::number(a).leftJustified(4, ' ')
                << "b =" << QString::number(b).leftJustified(4, ' ')
                << "startRow =" << QString::number(startRow).leftJustified(4, ' ')
                << "isReadIcon =" << QVariant(r->isReadIcon).toString().leftJustified(5)
                << "iconLoaded =" << QVariant(dm->iconLoaded(dmRow, instance)).toString().leftJustified(5)
                //<< "isRunning =" << QVariant(isRunning()).toString().leftJustified(5)
                ;
        }

        // report progress
        if (showProgressInStatusbar) {
            emit updateProgressInStatusbar(dmRow, dmRowCount);
            int progress = 1.0 * metaReadCount / dmRowCount * 100;
            emit updateProgressInFilter(progress);
        }

        // if readers have been dispatched for all rows
        if (aIsDone && bIsDone) {

            // all metadata and icons been loaded into datamodel?
            G::allMetadataAttempted = dm->isAllMetadataAttempted();
            bool allAttempted = G::allMetadataAttempted;
            /* problem with video files, where the FrameDecoder is still running when the icons
               loaded check is being made here.  Try skipping this check and see if works...
            if (dm->iconChunkSize >= sfRowCount) {
                G::allIconsLoaded = dm->allIconsLoaded();
                allAttempted = (G::allMetadataAttempted && G::allIconsLoaded);
            }
            else {
                allAttempted = (G::allMetadataAttempted && dm->allIconChunkLoaded(firstIconRow, lastIconRow));
            }
            */
            if (isDebug)  // dispatch for all rows completed
            {
                qDebug().noquote()
                    << "MetaRead2::dispatch     all dispatch done   "
                    << "id =" << QString::number(id).leftJustified(2, ' ')
                    << "row =" << QString::number(reader[id]->dmIdx.row()).leftJustified(4, ' ')
                    << "allLoaded =" << QVariant(allAttempted).toString().leftJustified(5)
                    //<< "toRead =" << toRead.size()
                    << "pending =" << pending()
                    << "isDone =" << isDone
                    << "metaReadCount =" << metaReadCount
                    //<< "toRead =" << toRead
                    ;
            }

            if (!allAttempted) {
                // are there still pending readers
                if (pending()) {
                    r->status = Reader::Status::Ready;
                    r->fPath = "";
                    return;
                }
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
                if (G::isLogger || G::isFlowLogger)  G::log("MW::dispatch", "Done");
                if (isDebug)
                qDebug().noquote()
                    << "MetaRead2::dispatch     We Are Done.     "
                    << QString::number(G::t.elapsed()).rightJustified((5)) << "ms"
                    << dm->currentFolderPath
                    //<< "toRead =" << toRead
                    << "pending =" << pending()
                    ;
                if (!abort) emit done();
                isDone = true;
                isDispatching = false;
            }

            r->status = Reader::Status::Ready;
            r->fPath = "";
            return;

        } // end aIsDone && bIsDone

    } // end returning reader

    // DISPATCH READER

    /* check if new start row while dispatch reading all metadata.  The user may have jumped
       to the end of a large folder while metadata is being read.  Also could have scrolled. */
    if (isNewStartRowWhileStillReading) {
        a = startRow;
        b = startRow - 1;
        isNewStartRowWhileStillReading = false;
        if (isDebug)
        {
            qDebug().noquote()
                << "MetaRead2::dispatch isNewStartRowWhileStillReading"
                << "startRow =" << QString::number(startRow).leftJustified(4, ' ')
                << "a =" << QString::number(a).leftJustified(4, ' ')
                << "b =" << QString::number(b).leftJustified(4, ' ')
                ;
        }
    }

    // assign either a or b as the next row to read in the datamodel
    if (nextRowToRead()) {
        QModelIndex sfIdx = dm->sf->index(nextRow, 0);
        QModelIndex dmIdx = dm->modelIndexFromProxyIndex(sfIdx);
        QString fPath = dmIdx.data(G::PathRole).toString();
        // only read icons within the icon chunk range
        if (isDebug)
        {
            qDebug().noquote()
                << "MetaRead2::dispatch     launch reader       "
                << "nextRow =" << nextRow
                << "firstIconRow =" << firstIconRow
                << "lastIconRow =" << lastIconRow
               ;
        }
        bool okReadIcon = (nextRow >= firstIconRow && nextRow <= lastIconRow);
        if (isDebug)
        {
            qDebug().noquote()
                << "MetaRead2::dispatch     launch reader       "
                << "id =" << QString::number(id).leftJustified(2, ' ')
                << "row =" << QString::number(nextRow).leftJustified(4, ' ')
                << "isReadIcon =" << QVariant(okReadIcon).toString().leftJustified(5, ' ')
                << "isAhead =" << QVariant(isAhead).toString().leftJustified(5, ' ')
                << "aIsDone =" << QVariant(aIsDone).toString().leftJustified(5, ' ')
                << "bIsDone =" << QVariant(bIsDone).toString().leftJustified(5, ' ')
                << "a =" << QString::number(a).leftJustified(4, ' ')
                << "b =" << QString::number(b).leftJustified(4, ' ')
                ;
        }

        /* read the image file metadata and icon.  When the reader is finished, it will
           signal dispatch to loop through to read another file...  */
        if (!abort) r->read(dmIdx, fPath, instance, okReadIcon);
    }

    // if done in both directions fire delay to quit in case isDone fails
    //if (aIsDone && bIsDone) {
    if (aIsDone && bIsDone && !isDone) {
        if (!quitAfterTimeoutInitiated) {
            if (isDebug)
            {
                qDebug().noquote()
                    << "MetaRead2::dispatch     aIsDone && bIsDone "
                    << QString::number(G::t.elapsed()).rightJustified((5)) << "ms"; G::t.restart();}
            quitAfterTimeoutInitiated = true;
            isDispatching = false;
            // if pending readers not all processed in delay ms then quit anyway
            int delay = 1000;
            if (isDebug)
            {
                qDebug()
                    << "MetaRead2::dispatch     aIsDone && bIsDone  quitAfterTimeoutInitiated in"
                    << delay << "ms";
            }
            QTimer::singleShot(delay, this, &MetaRead2::quitAfterTimeout);
        }
        r->status = Reader::Status::Ready;
        return;
    }

    isDispatching = true;
    r->status = Reader::Status::Ready;
}

void MetaRead2::dispatchReaders()
{
    if (G::isLogger) G::log("MetaRead2::dispatchReaders");
    if (isDebug)
    {
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
        if (reader[id]->isRunning()) reader[id]->stop();
        else {
            reader[id]->status = Reader::Status::Ready;
            reader[id]->fPath = "";
        }
        dispatch(id);
        if (abort) return;
    }
}

void::MetaRead2::quitAfterTimeout()
{
    if (!isDone) {
        if ((pending()) && (redoCount < redoMax)) {
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
                 ;
                 //<< dm->currentFolderPath << "toRead =" << toRead;
        }

        if (!abort) cleanupIcons();
        if (G::useUpdateStatus) emit runStatus(false, true, "MetaRead2::quitAnyway");

        emit done();
        isDispatching = false;
    }
}

void MetaRead2::run()
/*
    Loads the metadata and icons into the datamodel (dm) for a folder.  The iteration
    proceeds from the start row in an ahead/behind progression.

    Readers are dispatched and this function finishes.  The readers load the data into the
    DataModel and ImageCache, and then signal MW::dispatch, where they are iterated to read
    the next DataModel item.

    This means that this thread isRunning() == false while the dispatching is still running.
    Another flag, isDispatching, is used to track this.
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
    dispatchReaders();

    if (isDebug)
    {
        qDebug().noquote() << "MetaRead2::run          finished";
    }
}
