#include "metaread.h"
#include "Main/global.h"

/*
    MetaRead, running in a separate thread, dispatches readers to load the metadata and
    icons into the datamodel (dm), which will already include the file information,
    loaded by DataModel::addFileData. Icons will be loaded up to iconChunkSize, which
    defaults to 3000, and can be customized in settings.

    The idealThreadCount number of reader are created.  Readers read the metadata and icon
    and update the DataModel and ImageCache.

    Steps:

        • Call setStartRow.  The start row can be any row in the datamodel.

        • Dispatch is called for each reader.  Each time dispatch is called it iterates
          through the datamodel in a ahead/behind order.

        • Start the ImageCache when startRow has been read.

        • When each reader is finished it signals dispatch to be assigned a new file.

        • When all files read clean up excess icons and signal MW::loadCurrentDone.

        • Abort and restart when a new setCurrentRow is called.

    MetaRead::setCurrentRow is called when:

        • a new folder is selected
        • a new image is selected
        • thumbnails are scrolled
        • the gridView or thumbView are resized
        • there is an insertion into the datamodel

    Immediately show image in loupe view while MetaRead is working

        • if the new start row has already been read, fileSelectionChange is called
          from MW::loadCurrent.
        • if the new start row has not been read, fileSelectionChange is emitted
          from dispatch when a reader returns after reading the start row.

    Are we done?

        Determining if all the metadata and iconChunkRange have been loaded needs to
        consider if a read operation has failed or if the file has been read but the
        reader done signal has not been processed yet.

        • dm->isAllMetadataAttempted and dm->isAllIconChunkLoaded check if datamodel is
          complete.  If not, calling redo will redispatch readers to try again, up to
          5 times.
        • quitAfterTimeout runs if dispatchFinished has not run after a delay of 1000 ms.
          redo is called and then dispatchFinished.
        • dispatchFinished cleans up icons, resets flags, updates the statusbar and emits
          done, which signals MW::loadCurrentDone.

    Note: All data in the DataModel must be set using a queued connection.  When subsequent
    actions are dependent on the data being set use Qt::BlockingQueuedConnection.

    qDebug format:
          function      col X
          Desc          col +24
          Param         col +44


*/

MetaRead::MetaRead(QObject *parent,
                   DataModel *dm,
                   Metadata *metadata,
                   // FrameDecoder *frameDecoder,
                   ImageCache *imageCache)
    : QObject(nullptr)
{
    if (isDebug || G::isLogger) G::log("MetaRead::MetaRead");

    moveToThread(&metaReadThread);

    this->dm = dm;
    this->metadata = metadata;
    this->frameDecoder = frameDecoder;
    this->imageCache = imageCache;
    // thumb = new Thumb(dm, frameDecoder);

    readerCount = QThread::idealThreadCount();
    for (int id = 0; id < readerCount; ++id) {
        Reader *reader = new Reader(id, dm, imageCache);
        QThread *thread = new QThread;
        reader->readerThread = thread;
        reader->moveToThread(thread);  // Move to MetaRead's thread
        connect(reader, &Reader::done, this, &MetaRead::dispatch);
        thread->start();
        readers.append(reader);
        readerThreads.append(thread);
        cycling.append(false);
    }

    instance = 0;
    abort = false;
    isDebug = false;
    debugLog = false;

    // metaReadThread.start();
}

MetaRead::~MetaRead()
{
    if (isDebug) qDebug() << "MetaRead::~MetaRead";
    // delete thumb;
}

void MetaRead::setStartRow(int sfRow, bool fileSelectionChanged, QString src)
{
/*
    Must use QMetaObject::invokeMethod when calling this function
*/
    QString fun = "MetaRead::setStartRow";
    if (G::isLogger || G::isFlowLogger) {
        QString running;
        metaReadThread.isRunning() ? running = "true" : running = "false";
        QString s = "row = " + QString::number(sfRow) +
                    " fileSelectionChanged = " + QVariant(fileSelectionChanged).toString() +
                    " src = " + src + " isRunning = " + running;
        G::log(fun, s);
    }

    // could be called by a scroll event, then no file selection change
    this->fileSelectionChanged = fileSelectionChanged;

    // has metadata already been read for this row
    if (fileSelectionChanged) {
        if (dm->isMetadataAttempted(sfRow)) {
            // return;
        }
    }

    if (isDebug)
    {
        qDebug().noquote()
            << fun.leftJustified(col0Width)
            << "startRow =" << startRow
            << "isDispatching =" << isDispatching
            << "imageCacheTriggered =" << imageCacheTriggered
            << "fileSelectionChanged =" << fileSelectionChanged
            << "G::allMetadataLoaded =" << G::allMetadataLoaded
            << "G::iconChunkLoaded =" << G::iconChunkLoaded
            << "src =" << src
            ;
    }

    QMutexLocker locker(&mutex);
    abort = false;

    // prevent redundant calls
    if (sfRow >= 0 && sfRow < sfRowCount) startRow = sfRow;
    else startRow = 0;
    // if (instance == dm->instance && prevStartRow == startRow) return;
    prevStartRow = startRow;

    // load datamodel with metadata and icons
    this->src = src;
    sfRowCount = dm->sf->rowCount();
    lastRow = sfRowCount - 1;
    headStartCount = 0;
    imageCacheTriggered = false;
    aIsDone = false;
    bIsDone = false;
    if (startRow == 0) bIsDone = true;
    isDone = false;
    success = false;                        // used to update statusbar
    // set icon range. G::iconChunkLoaded is set in MW::loadCurrent
    firstIconRow = dm->startIconRange;      // just use dm->startIconRange ?  RGH
    lastIconRow = dm->endIconRange;

    if (isDebug)
    {
    qDebug().noquote()
             << "\n"
             << fun.leftJustified(col0Width)
             << "startRow =" << startRow
             << "firstIconRow =" << firstIconRow
             << "lastIconRow =" << lastIconRow
             << "iconChunkSize =" << dm->iconChunkSize
             << "fileSelectionChanged =" << fileSelectionChanged
             << "G::allMetadataLoaded =" << G::allMetadataLoaded
             << "G::iconChunkLoaded =" << G::iconChunkLoaded
             << "isRunning =" << metaReadThread.isRunning()
             << "isDispatching =" << isDispatching
             << "isDone =" << isDone
             << "instance =" << instance << "/" << dm->instance
            ;
    }

    if (G::useUpdateStatus) emit runStatus(true, true, false, fun);
                                        // isRunning, show, success, source

    if (instance == dm->instance) {
        isNewStartRowWhileDispatching = isDispatching;
        if (!isDispatching) {
            a = startRow;
            b = startRow - 1;
        }
    }
    // instance change (new datamodel)
    else {
        instance = dm->instance;
        isNewStartRowWhileDispatching = false;
        a = startRow;
        b = startRow - 1;
        if (isDebug)
        {
            qDebug().noquote()
                << fun.leftJustified(col0Width)
                << "isDispatching = true"
                // << "isAhead =" << QVariant(isAhead).toString().leftJustified(5, ' ')
                // << "aIsDone =" << QVariant(aIsDone).toString().leftJustified(5, ' ')
                // << "bIsDone =" << QVariant(bIsDone).toString().leftJustified(5, ' ')
                // << "a =" << QString::number(a).leftJustified(4, ' ')
                // << "b =" << QString::number(b).leftJustified(4, ' ')
                ;
        }
    }

    dispatchReaders();
}

void MetaRead::stopReaders()
/*
    MetaRead::run dispatches all the readers and returns immediately.  The isDispatched flag
    indicates that dispatching is still running.
*/
{
    QString fun = "MetaRead::stopReaders";
    if (/*debugLog && */(G::isLogger || G::isFlowLogger)) G::log(fun);
    if (isDebug)
    {
        qDebug().noquote()
            << fun.leftJustified(col0Width)
            << "instance / dm->instance =" << instance << "/" << dm->instance;
    }

    // stop all readers
    for (int id = 0; id < readerCount; ++id) {
        readers[id]->stop();
    }
}
void MetaRead::abortReaders()
{
/*
    Set the abort flag to cancel the current read operation, before changing folders
*/
    abort = true;
    // abort all readers
    for (int id = 0; id < readerCount; ++id) {
        readers[id]->abortProcessing();
    }
}

bool MetaRead::allReadersCycling()
{
    for (bool isCycling : cycling) {
        if (!isCycling) return false;
    }
    return true;
}

void MetaRead::initialize()
/*
    Called when change folders.
    Must use QMetaObject::invokeMethod when calling this function
*/
{
    if (G::isLogger || G::isFlowLogger)
    {
        G::log("MetaRead::initialize", dm->primaryFolderPath());
    }
    if (isDebug)
    {
        QString fun = "MetaRead::initialize     initialize";
        qDebug().noquote()
            << "\n"
            << fun.leftJustified(col0Width)
            ;
    }

    abort = true;
    for (Reader *r : readers) r->abortProcessing();
    dmRowCount = dm->rowCount();
    metaReadCount = 0;
    metaReadItems = dmRowCount;
    prevStartRow = -1;
    headStart = readerCount * 2;
    if (headStart > dmRowCount) headStart = dmRowCount;
    isAhead = true;
    aIsDone = false;
    bIsDone = false;
    isDone = false;
    success = false;
    quitAfterTimeoutInitiated = false;
    redoCount = 0;
    redoMax = 5;
    err.clear();
    cycling.fill(false);
}

void MetaRead::syncInstance()
{
    if (G::isLogger) G::log("MetaRead::syncInstance");
    instance = dm->instance;
}

void MetaRead::test()
{
    for (bool val : cycling) qDebug() << val;
}

QString MetaRead::msElapsed()
{
    static QDateTime prevDateTime = QDateTime::currentDateTime();
    QDateTime currentDateTime = QDateTime::currentDateTime();

    qint64 elapsedMs = prevDateTime.msecsTo(currentDateTime);
    prevDateTime = currentDateTime;

    return QString::number(elapsedMs).leftJustified(10);
}

QString MetaRead::diagnostics()
{
    if (isDebug || G::isLogger) G::log("MetaRead::diagnostics");

    QString reportString;
    QTextStream rpt;
    rpt.setString(&reportString);
    rpt << Utilities::centeredRptHdr('=', objectName() + " MetaRead Diagnostics");
    rpt << "\n" ;
    rpt << "\n" << "instance:                   " << instance;
    rpt << "\n";
    rpt << "\n" << "expansionFactor:            " << expansionFactor;
    rpt << "\n";
    rpt << "\n" << "sfRowCount:                 " << sfRowCount;
    rpt << "\n" << "dm->currentSfRow:           " << dm->currentSfRow;
    rpt << "\n";
    rpt << "\n" << "defaultIconChunkSize:       " << dm->defaultIconChunkSize;
    rpt << "\n" << "iconChunkSize:              " << dm->iconChunkSize;
    rpt << "\n" << "iconLimit:                  " << iconLimit;
    rpt << "\n";
    rpt << "\n" << "firstVisibleIcon:           " << dm->firstVisibleIcon;
    rpt << "\n" << "lastVisibleIcon:            " << dm->lastVisibleIcon;
    rpt << "\n" << "visibleIcons:               " << dm->visibleIcons;
    rpt << "\n";
    rpt << "\n" << "firstIconRow:               " << firstIconRow;
    rpt << "\n" << "lastIconRow:                " << lastIconRow;
    rpt << "\n" << "dm->startIconRange:         " << dm->startIconRange;
    rpt << "\n" << "dm->endIconRange:           " << dm->endIconRange;
    rpt << "\n" << "dm->iconCount:              " << dm->iconCount();
    rpt << "\n";
    QStringList s;
    for (int i : dm->metadataNotLoaded()) s << QString::number(i);
    rpt << "\n" << "dm->isAllMetadataAttempted: " << QVariant(dm->isAllMetadataAttempted()).toString();
    if (s.size()) rpt << "  Rows not loaded: " << s.join(",");
    rpt << "\n" << "G::allMetadataLoaded:       " << QVariant(G::allMetadataLoaded).toString();
    rpt << "\n" << "G::iconChunkLoaded:         " << QVariant(G::iconChunkLoaded).toString();
    rpt << "\n" << "G::maxIconChunk:            " << QVariant(G::maxIconChunk).toString();
    rpt << "\n";
    rpt << "\n" << "isDone:                     " << QVariant(isDone).toString();
    rpt << "\n" << "aIsDone && bIsDone:         " << QVariant(aIsDone && bIsDone).toString();
    rpt << "\n";
    rpt << "\n" << "abort:                      " << QVariant(abort).toString();
    rpt << "\n" << "isRunning:                  " << QVariant(metaReadThread.isRunning()).toString();
    rpt << "\n" << "isRunloop:                  " << QVariant(runloop.isRunning()).toString();
    rpt << "\n" << "isDispatching:              " << QVariant(isDispatching).toString();
    rpt << "\n";
    rpt << "\n" << "redoCount:                  " << QVariant(redoCount).toString();
    rpt << "\n" << "redoMax:                    " << QVariant(redoMax).toString();
    rpt << "\n";

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
    rpt << "\n";
    rpt << "  ID    Cycling   Pending     Status";
    for (int id = 0; id < readerCount; id++) {
        rpt.setFieldAlignment(QTextStream::AlignRight);
        rpt.setFieldWidth(4);
        rpt << "\n" << id;
        rpt.setFieldWidth(11);
        rpt << QVariant(cycling.at(id)).toString();
        rpt.setFieldWidth(10);
        rpt << QVariant(readers[id]->pending).toString();
        rpt.setFieldWidth(5);
        rpt << " ";
        rpt.setFieldAlignment(QTextStream::AlignLeft);
        rpt.setFieldWidth(30);
        rpt << readers[id]->statusText.at(readers[id]->status);
    }

    // rows with icons
    // /*
    rpt << "\n";
    rpt << "\n";
    rpt.setFieldAlignment(QTextStream::AlignLeft);
    rpt << "Empty icon rows in datamodel:";
    rpt.setFieldAlignment(QTextStream::AlignRight);
    rpt.setFieldWidth(9);
    for (int i = 0; i < dm->sf->rowCount(); i++) {
        if (dm->iconLoaded(i, instance)) continue;
        rpt << "\n" << i;
    }
    //*/
    rpt << "\n\n" ;

    return reportString;
}

QString MetaRead::reportMetaCache()
{
    if (isDebug || G::isLogger) G::log("MetaRead::reportMetaCache");
    QString reportString;
    QTextStream rpt;
    rpt.flush();
    reportString = "MetaCache";
    rpt.setString(&reportString);

    rpt << "\n";
    return reportString;
}

void MetaRead::cleanupIcons()
{
/*
    Remove icons not in icon range after start row change, iconChunkSize change or
    MW::deleteFiles.

    The icon range is the lesser of iconChunkSize and dm->sf->rowCount(), centered on
    the current datamodel row dm->currentSfRow.
*/
    QString fun = "MetaRead::cleanupIcons";
    if (G::isLogger) G::log(fun);
    if (isDebug)
        qDebug().noquote()
            << fun.leftJustified(col0Width)
            << "firstIconRow =" << firstIconRow
            << "lastIconRow =" << lastIconRow
            ;

    // check if datamodel size is less than assigned icon cache chunk size
    if (dm->iconChunkSize >= sfRowCount) {
        return;
    }

    for (int i = 0; i < dm->startIconRange; i++) {
        if (!dm->index(i, 0).data(Qt::DecorationRole).isNull()) {
            dm->setData(dm->index(i, 0), QVariant(), Qt::DecorationRole);
        }
    }
    for (int i = dm->endIconRange + 1; i < sfRowCount; i++) {
        if (!dm->index(i, 0).data(Qt::DecorationRole).isNull()) {
            dm->setData(dm->index(i, 0), QVariant(), Qt::DecorationRole);
        }
    }
}

inline bool MetaRead::needToRead(int row)
/*
    Returns true if either the metadata or icon (thumbnail) has not been loaded.
*/
{
    if (!dm->sf->index(row, G::MetadataAttemptedColumn).data().toBool()) {
        return true;
    }
    if (!dm->sf->index(row, G::IconLoadedColumn).data().toBool()) {
        return true;
    }
    return false;
}

bool MetaRead::nextA()
{
    // find next a
    if (aIsDone) return false;
    // use dm->sf->rowCount() in case more folders have been added
    for (int i = a; i < dm->sf->rowCount(); i++) {
        if (needToRead(i)) {
            nextRow = i;
            a = i + 1;
            if (a == sfRowCount) aIsDone = true;
            if (!bIsDone) isAhead = false;
            return true;
        }
    }
    a = sfRowCount;
    aIsDone = true;
    return false;
}

bool MetaRead::nextB()
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

bool MetaRead::nextRowToRead()
/*
    Alternate searching ahead/behind for the next datamodel row missing metadata or icon.
    Only search for missing icons within the iconChunk range.

    a (ahead row) or b (behind row) is updated.
*/
{
    // ahead
    if (isAhead) {
        /*
        qDebug().noquote()
            << "MetaRead::dispatch     nextRowToRead       "
            << "id =   "
            << "row =" << QString::number(nextRow).leftJustified(4, ' ')
            << "isReadIcon =      "
            << "isAhead =" << QVariant(isAhead).toString().leftJustified(5, ' ')
            << "aIsDone =" << QVariant(aIsDone).toString().leftJustified(5, ' ')
            << "bIsDone =" << QVariant(bIsDone).toString().leftJustified(5, ' ')
            << "a =" << QString::number(a).leftJustified(4, ' ')
            << "b =" << QString::number(b).leftJustified(4, ' ')
            << "startRow =" << startRow
            << "firstIconRow =" << firstIconRow
            << "lastIconRow =" << lastIconRow
                ; //*/
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

int MetaRead::pending()
/*
    Return the number of Readers that have been dispatched and not signalled back
    that they are done.
*/
{
    QString fun = "MetaRead::pending";
    int pendingCount = 0;
    for (int id = 0; id < readerCount; ++id) {
        if (readers[id]->pending) {
            pendingCount ++;
            if (isDebug)
            {
                qDebug().noquote()
                    << fun.leftJustified(col0Width)
                    << "id =" << QString::number(id).leftJustified(2, ' ')
                    << "row =" << QString::number(readers[id]->dmIdx.row()).leftJustified(4, ' ')
                    << "allMetadataLoaded =" << G::allMetadataLoaded
                    ;
            }
        }
    }
    return pendingCount;
}

bool MetaRead::allMetaIconLoaded()
{
/*
    Has the datamodel been fully loaded?
*/
    return dm->isAllMetadataAttempted() && dm->isIconRangeLoaded();
}

void MetaRead::redo()
/*
    If not all metadata or icons were successfully read so try again.
*/
{
    QString fun = "MetaRead::redo";
    if (debugLog && (G::isLogger || G::isFlowLogger))
    {
        G::log(fun, "count = " + QString::number(redoCount));
    }
    if (isDebug)
    {
        qDebug().noquote()
            << fun.leftJustified(col0Width)
            << "count =" << redoCount;
    }
    redoCount++;
    aIsDone = false;
    bIsDone = false;
    a = startRow;
    b = startRow - 1;
    // cycling.fill(false);
    dispatchReaders();
}

void MetaRead::emitFileSelectionChangeWithDelay(const QModelIndex &sfIdx, int msDelay)
{
    // Emit the signal after a delay using QTimer::singleShot and a lambda
    QModelIndex idx2 = QModelIndex();
    bool clearSelection = false;
    QString src = "MetaRead::dispatch";
    QTimer::singleShot(msDelay, this, [this, sfIdx, idx2, clearSelection, src]() {
        /*
        qDebug() << "MetaRead::emitFileSelectionChangeWithDelay            "
                 << "row =" << sfIdx.row()
                 << "   instance =" << instance
                 << "dm->instance =" << dm->instance
            ; //*/
        if (instance == dm->instance) {
            emit fileSelectionChange(sfIdx, idx2, clearSelection, src);
        }
    });
}

void MetaRead::dispatch(int id)
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
    readers signal back to dispatch when done

    - if reader returning
        - status update
        - deplete toRead list (decremented)
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
    QString fun = "MetaRead::dispatch";
    // qDebug() << "MetaRead::dispatch id =" << id;

    // terse pointer to readers[id]
    r = readers[id];
    r->pending = false;

    if (abort) {
        if (isDebug)
        {
            QString fun1 = fun + " abort";
            qDebug().noquote()
                << fun1.leftJustified(col0Width)
                << "reader =" << id
                << "abort =" << abort
                << "isDone =" << isDone
                << "dm instance =" << dm->instance
                << "instance =" << instance
                << "reader instance =" << readers[id]->instance
                   ;
        }
        r->status = Reader::Status::Ready;
        r->fPath = "";
        return;
    }

    // New reader and less rows (images to read) than readers - do not dispatch
    if (r->fPath == "" && id >= dmRowCount) {
        if (isDebug)
        {
            qDebug().noquote()
                << fun.leftJustified(col0Width)
                << "id =" << id
                << "dmRowCount =" << dmRowCount
                << "decode id exceeds rowCount, return";
        }
        return;
    }

    if (isDebug)
    {
        QString  row;
        r->fPath == "" ? row = "-1" : row = QString::number(r->dmIdx.row());
        QString fun1;
        if (r->fPath == "") fun1 = fun + " reader starting";
        else fun1 = fun + " reader returning";
        qDebug().noquote()
            << fun1.leftJustified(col0Width)
            << "id =" << QString::number(id).leftJustified(2, ' ')
            << "row =" << row.leftJustified(4, ' ')
            //<< "isAhead =" << QVariant(isAhead).toString().leftJustified(5, ' ')
            << "aIsDone =" << QVariant(aIsDone).toString().leftJustified(5, ' ')
            << "bIsDone =" << QVariant(bIsDone).toString().leftJustified(5, ' ')
            //<< "allRead =" << QVariant(allDone).toString().leftJustified(5, ' ')
            << "a =" << QString::number(a).leftJustified(4, ' ')
            << "b =" << QString::number(b).leftJustified(4, ' ')
            << "isGUI =" << G::isGuiThread()
            ;
    }

    if (debugLog || (G::isLogger || G::isFlowLogger))
    {
        QString  row;
        r->fPath == "" ? row = "-1" : row = QString::number(r->dmIdx.row());
        QString from;
        r->fPath == "" ? from = "start reader" : from = "return from reader";
        QString s = "id = " + QString::number(id).rightJustified(2, ' ');
        s += " row = " + row.rightJustified(4, ' ');
        G::log("MetaRead::dispatch: " + from, s);
    }

    // RETURNING READER
    if (r->fPath != "" && r->instance == dm->instance) {

        /*
        - report read failures
        - trigger fileSelectionChange if more than headStart rows read
        - update progress
        - if readers have been dispatched for all rows
             - check if still pending
             - check if all metadata and necessary icons loaded
             - if all checks out signal done
        */

        int dmRow = r->dmIdx.row();

        // progress counter
        metaReadCount++;
        headStartCount++;

        // it is not ok to select while the datamodel is being built.
        if (metaReadCount == 1) emit okToSelect(true);

        // report read failure
        if (!(r->status == r->Status::Success /*|| r->status == r->Status::Ready*/)) {
            QString error = "row " + QString::number(dmRow).rightJustified(5) + " " +
                            r->statusText.at(r->status).leftJustified(10) + " " + r->fPath;
            err.append(error);
            if (isDebug)
            {
                QString fun1 = fun + " Failure";
                qDebug().noquote()
                     << fun1.leftJustified(col0Width)
                     << "id =" << QString::number(id).leftJustified(2, ' ')
                     << "row =" << dmRow
                     << "status =" << r->statusText.at(r->status)
                     << r->fPath;
            }
        }

        // /*trigger MW::fileSelectionChange which starts ImageCache
        if (fileSelectionChanged &&
            !imageCacheTriggered &&
            instance == dm->instance &&
            dmRow == startRow
            // (headStartCount > headStart || (aIsDone && bIsDone))
            )
        {
            imageCacheTriggered = true;
            // QModelIndex sfIdx = dm->proxyIndexFromModelIndex(r->dmIdx);  // crash 2025-03-08
            // QModelIndex sfIdx = dm->sf->mapFromSource(r->dmIdx); // crash
            // model and proxy rows the same in metaRead
            QModelIndex sfIdx = dm->sf->index(r->dmIdx.row(),0);
            bool clearSelection = false;
            QString src = "MetaRead::dispatch";
            emit fileSelectionChange(sfIdx, QModelIndex(), clearSelection, src);
            // qDebug() << src << "emit fileSelectionChange" << startRow;
        }
        //*/

        /* trigger fileSelectionChange which starts ImageCache after time delay
        if (fileSelectionChanged && dmRow == startRow) {
            QModelIndex sfIdx = dm->proxyIndexFromModelIndex(r->dmIdx);  // rghZ already a filter??

            if (isDebug)  // returning reader, row has been processed by reader
            {
                qDebug().noquote()
                    << "MetaRead::dispatch     startRow         "
                    << "dmRow =" << dmRow
                    << "startRow =" << startRow
                    << "r->dmIdx =" << r->dmIdx
                    << "sfIdx =" << sfIdx
                    << r->fPath
                    ;
            }

            if (debugLog && (G::isLogger || G::isFlowLogger))
            {
                G::log("MetaRead::dispatch", "fileSelectionChange row = " + QString::number(dmRow));
            }
            // if (isDebug) // fileSelectionChange
            {
                bool isMetaLoaded = dm->isMetadataLoaded(sfIdx.row());
                qDebug().noquote()
                    << "\nMetaRead::dispatch     fileSelectionChange "
                    << "id =" << QString::number(id).leftJustified(2, ' ')
                    << "row =" << QString::number(dmRow).leftJustified(4, ' ')
                    << "metaLoaded =" << QVariant(isMetaLoaded).toString().leftJustified(5)
                    << r->fPath;
            }
            if (!abort) {
                // emit fileSelectionChange(r->dmIdx);
                int msDelay = 500;
                emitFileSelectionChangeWithDelay(r->dmIdx, msDelay);
            }
        }
        //*/

        if (isDebug)  // returning reader, row has been processed by reader
        {
        QString ms = msElapsed();
            // bool allLoaded = (dm->isAllMetadataAttempted() && dm->allIconChunkLoaded(firstIconRow, lastIconRow));
            bool allLoaded = (dm->isAllMetadataAttempted() && dm->isAllIconsLoaded());
            QString fun = "MetaRead::dispatch processed";
            qDebug().noquote()
                << fun.leftJustified(col0Width)
                << "id =" << QString::number(id).leftJustified(2, ' ')
                //<< "startRow =" << QString::number(startRow).leftJustified(4, ' ')
                << "row =" << QString::number(dmRow).leftJustified(4, ' ')
                // << "isRunning =" << QVariant(r->isRunning()).toString().leftJustified(5)
                // << "allLoaded =" << QVariant(allLoaded).toString().leftJustified(5)
                // << "iconChunkLoaded =" << QVariant(dm->isAllIconChunkLoaded(firstIconRow, lastIconRow)).toString().leftJustified(5)
                // //<< "toRead =" << toRead.size()
                // << "rowCount =" << dm->rowCount()
                // << "a =" << QString::number(a).leftJustified(4, ' ')
                // << "b =" << QString::number(b).leftJustified(4, ' ')
                // << "startRow =" << QString::number(startRow).leftJustified(4, ' ')
                // << "isReadIcon =" << QVariant(r->isReadIcon).toString().leftJustified(5)
                // << "iconLoaded =" << QVariant(dm->iconLoaded(dmRow, instance)).toString().leftJustified(5)
                // //<< "isRunning =" << QVariant(isRunning()).toString().leftJustified(5)
                ;
        }

        // report progress in statusbar and top of filter dock
        if (showProgressInStatusbar) {
            emit updateProgressInStatusbar(dmRow, dm->rowCount());
            int progress = 1.0 * metaReadCount / dm->rowCount() * 100;
            emit updateProgressInFilter(progress);
        }

        // if readers have been dispatched for all rows
        if (aIsDone && bIsDone) {

            // all metadata and icons been loaded into datamodel?
            bool metaAttempted;
            QMetaObject::invokeMethod(
                dm,
                "isAllMetadataAttempted",
                Qt::BlockingQueuedConnection,
                Q_RETURN_ARG(bool, metaAttempted)
                );
            // qDebug() << fun + "isAllMetadataAttempted";
            bool iconAttempted;
            QMetaObject::invokeMethod(
                dm,
                "isAllIconChunkLoaded",
                Qt::BlockingQueuedConnection,
                Q_RETURN_ARG(bool, iconAttempted),
                Q_ARG(int, dm->startIconRange),
                Q_ARG(int, dm->endIconRange)
            );
            // qDebug() << fun + "isAllIconChunkLoaded";

            // bool metaAttempted = dm->isAllMetadataAttempted();
            // bool iconAttempted = dm->isAllIconChunkLoaded(dm->startIconRange, dm->endIconRange);

            bool allAttempted = metaAttempted && iconAttempted;

            if (isDebug)  // dispatch for all rows completed
            {
                QString fun1 = fun + " aIsDone && bIsDone";
                qDebug().noquote()
                    << fun1.leftJustified(col0Width)
                    << "id =" << QString::number(id).leftJustified(2, ' ')
                    << "row =" << QString::number(readers[id]->dmIdx.row()).leftJustified(4, ' ')
                    << "G::allMetadataLoaded =" << G::allMetadataLoaded
                    << "allAttempted =" << QVariant(allAttempted).toString().leftJustified(5)
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
                    cycling[id] = false;
                    return;
                }
                // if all readers finished but not all loaded, then redo
                else {
                    if (redoCount < redoMax) {
                        if (isDebug)
                        {
                            QString fun1 = fun + " redo";
                            qDebug().noquote()
                                << "MetaRead::dispatch     Redo                "
                                << "redoCount =" << redoCount
                                << "redoMax =" << redoMax
                                ;
                        }
                        // try to read again
                        metaReadThread.msleep(50);
                        if (!abort) {
                            redo();
                        }
                    }
                    else {
                        qWarning() << "REDO MAXED OUT";
                        dispatchFinished("WeAreDone");
                    }
                }
            }

            // Now we are done
            if (allAttempted && !isDone) {
                if (isDebug)
                {
                    QString fun1 = fun + " We are done";
                    qDebug().noquote()
                        << fun1.leftJustified(col0Width)
                        << QString::number(G::t.elapsed()).rightJustified((5)) << "ms"
                        << "G::allMetadataLoaded =" << G::allMetadataLoaded
                        //<< "toRead =" << toRead
                        << "pending =" << pending()
                        ;
                }
                dispatchFinished("WeAreDone");
            }

            r->status = Reader::Status::Ready;
            r->fPath = "";
            cycling[id] = false;
            return;

        } // end aIsDone && bIsDone

    } // end returning reader

    // DISPATCH READER

    /* check if new start row while dispatch reading all metadata.  The user may have jumped
       to the end of a large folder while metadata is being read.  Also could have scrolled. */
    if (isNewStartRowWhileDispatching) {
        a = startRow;
        b = startRow - 1;
        if (fileSelectionChanged) headStartCount = 0;
        isNewStartRowWhileDispatching = false;
        if (isDebug)
        {
            QString fun1 = fun + " isNewStartRowWhileStillReading";
            qDebug().noquote()
                << fun1.leftJustified(col0Width)
                << "startRow =" << QString::number(startRow).leftJustified(4, ' ')
                << "a =" << QString::number(a).leftJustified(4, ' ')
                << "b =" << QString::number(b).leftJustified(4, ' ')
                << "isAhead =" << QVariant(isAhead).toString()
                ;
        }
    }

    // assign either a or b as the next row to read in the datamodel
    if (nextRowToRead()) {
        QModelIndex sfIdx = dm->sf->index(nextRow, 0);
        QModelIndex dmIdx = dm->modelIndexFromProxyIndex(sfIdx);
        // QString fPath = dmIdx.data(G::PathRole).toString();
        QString fPath = sfIdx.data(G::PathRole).toString();
        // only read icons within the icon chunk range
        bool okReadIcon = (nextRow >= firstIconRow && nextRow <= lastIconRow);
        if (isDebug)
        {
            QString fun1 = fun + " invoke reader";
            // QString ms = msElapsed();
            // if (ms.trimmed().toInt() > 100)
            qDebug().noquote()
                // << ms
                << fun.leftJustified(col0Width)
                << "id =" << QString::number(id).leftJustified(2, ' ')
                << "row =" << QString::number(sfIdx.row()).leftJustified(4, ' ')
                // << "nextRow =" << QString::number(nextRow).leftJustified(4, ' ')
                // << "isReadIcon =" << QVariant(okReadIcon).toString().leftJustified(5, ' ')
                // << "isAhead =" << QVariant(isAhead).toString().leftJustified(5, ' ')
                // << "aIsDone =" << QVariant(aIsDone).toString().leftJustified(5, ' ')
                // << "bIsDone =" << QVariant(bIsDone).toString().leftJustified(5, ' ')
                // << "a =" << QString::number(a).leftJustified(4, ' ')
                // << "b =" << QString::number(b).leftJustified(4, ' ')
                // << fPath
                ;
        }

        /* read the image file metadata and icon.  When the reader is finished, it will
           signal dispatch (this function) to loop through to read another file...  */
        if (!abort) {
            QMetaObject::invokeMethod(readers.at(id), "read", Qt::QueuedConnection,
                                      Q_ARG(QModelIndex, dmIdx),
                                      Q_ARG(QString, fPath),
                                      Q_ARG(int, dm->instance),
                                      Q_ARG(bool, okReadIcon)
                                      );
        }
    }
    // nothing to read, we're finished
    // else {
    //     cycling[id] = false;
    //     if (isDebug)
    //     {
    //         qDebug().noquote()
    //             << "MetaRead::dispatch     NO launch reader    "
    //             << "id =" << QString::number(id).leftJustified(2, ' ')
    //             << "row =" << QString::number(nextRow).leftJustified(4, ' ')
    //             << "nextRowToRead = FALSE"
    //             << "isCycling =" << QVariant(cycling.at(id)).toString().leftJustified(5, ' ')
    //             << "isAhead =" << QVariant(isAhead).toString().leftJustified(5, ' ')
    //             << "aIsDone =" << QVariant(aIsDone).toString().leftJustified(5, ' ')
    //             << "bIsDone =" << QVariant(bIsDone).toString().leftJustified(5, ' ')
    //             << "a =" << QString::number(a).leftJustified(4, ' ')
    //             << "b =" << QString::number(b).leftJustified(4, ' ')
    //             ;
    //     }
    // }


    // // if done in both directions fire delay to quit in case isDone fails
    // if (aIsDone && bIsDone && !isDone) {
    //     if (!quitAfterTimeoutInitiated) {
    //         if (isDebug)
    //         {
    //             qDebug().noquote()
    //                 << "MetaRead::dispatch     aIsDone && bIsDone "
    //                 << QString::number(G::t.elapsed()).rightJustified((5)) << "ms"; G::t.restart();}
    //         quitAfterTimeoutInitiated = true;
    //         // isDispatching = false;
    //         // if pending readers not all processed in delay ms then quit anyway
    //         int delay = 1000;
    //         // if (isDebug)
    //         {
    //             qDebug()
    //                 << "MetaRead::dispatch     aIsDone && bIsDone  quitAfterTimeoutInitiated in"
    //                 << delay << "ms";
    //         }
    //         QTimer::singleShot(delay, this, &MetaRead::quitAfterTimeout);
    //     }
    //     r->status = Reader::Status::Ready;
    //     cycling[id] = false;
    //     return;
    // }
}

void MetaRead::dispatchReaders()
{
    QString fun = "MetaRead::dispatchReaders";
    if (debugLog && (G::isLogger || G::isFlowLogger)) G::log(fun);
    if (isDebug)
    {
    qDebug().noquote()
             << fun.leftJustified(col0Width)
             << "readerCount =" << readerCount
                ;
    }

    isDispatching = true;

    for (int id = 0; id < readers.count(); id++) {
        if (isDebug)
        {
            qDebug().noquote()
            << fun.leftJustified(col0Width)
            << "id =" << id
            << "cycling =" << cycling.at(id)
            ;
        }
        if (!cycling.at(id)) {
            readers[id]->status = Reader::Status::Ready;
            readers[id]->fPath = "";
            readers[id]->instance = dm->instance;
            cycling[id] = true;
            if (!abort) dispatch(id);
            if (isDebug)
            {
                qDebug().noquote()
                << fun.leftJustified(col0Width)
                << "reader =" << id << "dispatched"
                    ;
            }
        }

        if (abort) return;
    }
}

void::MetaRead::quitAfterTimeout()
{
    QString fun = "MetaRead::quitAfterTimeout";
    if (isDebug)
    {
        qDebug().noquote()
        << fun.leftJustified(col0Width)
            ;
    }
    if (redoCount < redoMax) {
        if (!abort && !allMetaIconLoaded()) {
            redo();
        }
    }
    dispatchFinished("QuitAfterTimeout");

    if (isDebug)
    {
        QString fun1 = fun + " We are done";
        qDebug().noquote()
             << fun1.leftJustified(col0Width)
             << QString::number(G::t.elapsed()).rightJustified((5)) << "ms"
             ;
    }
}

void MetaRead::dispatchFinished(QString src)
{
    QString fun = "MetaRead::dispatchFinished";
    if (debugLog && (G::isLogger || G::isFlowLogger))  G::log(fun, src);
    if (isDebug)
    {
        qDebug().noquote()
            << fun.leftJustified(col0Width)
            << "src =" << src
            << "G::allMetadataLoaded =" << G::allMetadataLoaded
            ;
    }

    bool running = false;
    bool show = true;
    success = allMetaIconLoaded();
    if (G::useUpdateStatus) emit runStatus(running, show, success, fun);
    cleanupIcons();
    isDone = true;
    isDispatching = false;

    // do not emit done if only updated icon loading
    if (!G::allMetadataLoaded) {
        G::allMetadataLoaded = true;
        // G::iconChunkLoaded = true;
        // signal MW::loadDone
        emit done();
    }
    G::iconChunkLoaded = true;      // rgh change to = dm->allIconChunkLoaded(first, last) ??
}
