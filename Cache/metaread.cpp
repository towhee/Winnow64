#include "metaread.h"
#include "Main/global.h"

/*
    MetaRead, running in a separate thread, dispatches readers to load the metadata and
    icons into the datamodel (dm), which will already include the file information,
    loaded by DataModel::addFileData. Icons will be loaded up to iconChunkSize, which
    defaults to 3000, and can be customized in settings.

    The idealThreadCount number of readers are created.  Readers read the metadata/icons
    and update the DataModel and ImageCache.

    Steps:

        • Call setStartRow.  The start row can be any row in the datamodel.

        • Dispatch is called for each reader.  Each time dispatch is called it iterates
          through the datamodel in a ahead/behind order.

        • Start the ImageCache when startRow has been read.

        • When each reader is finished it signals dispatch to be assigned a new file.

        • When all files have been read, clean up excess icons and signal
          MW::folderChangeCompleted.

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

    Icon Chunks



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
                   ImageCache *imageCache)
    : QObject(nullptr),
      quitTimer(new QTimer(this))

{
    if (isDebug || G::isLogger) G::log("MetaRead::MetaRead");

    moveToThread(&metaReadThread);

    this->dm = dm;
    this->metadata = metadata;
    this->imageCache = imageCache;

    readerCount = QThread::idealThreadCount();
    for (int id = 0; id < readerCount; ++id) {
        Reader *reader = new Reader(id, dm, imageCache);
        QThread *thread = new QThread;
        reader->readerThread = thread;
        reader->moveToThread(thread);
        connect(reader, &Reader::done, this, &MetaRead::dispatch);
        thread->start();
        readers.append(reader);
        readerThreads.append(thread);
        cycling.append(false);
    }

    quitTimer->setSingleShot(true);

    connect(quitTimer, &QTimer::timeout, this, [this]() {
        dispatchFinished("QuitAfterTimeout");
    });

    isDispatching = false;
    instance = 0;
    abort = false;
    isDebug = false;
    debugLog = false;
}

MetaRead::~MetaRead()
{
    if (isDebug) qDebug() << "MetaRead::~MetaRead";
    // delete thumb;
}

void MetaRead::setStartRow(int sfRow, bool fileSelectionChanged, QString src)
{
/*
    Must use QMetaObject::invokeMethod when calling this function.  Invoked by
    MW::updateChange().
*/
    setBusy();

    QString fun = "MetaRead::setStartRow";
    if (G::isLogger || G::isFlowLogger)
    {
        QString running;
        metaReadThread.isRunning() ? running = "true" : running = "false";
        QString s = "row = " + QString::number(sfRow) +
                    " fileSelectionChanged = " + QVariant(fileSelectionChanged).toString() +
                    " src = " + src + " isRunning = " + running;
        G::log(fun, s);
    }

    // could be called by a scroll event, then no file selection change
    this->fileSelectionChanged = fileSelectionChanged;

    if (isDebug)
    {
        qDebug().noquote()
            << fun.leftJustified(col0Width)
            << "startRow =" << sfRow
            << "sfRowCount =" << sfRowCount
            << "imageCacheTriggered =" << imageCacheTriggered
            << "fileSelectionChanged =" << fileSelectionChanged
            << "G::allMetadataLoaded =" << G::allMetadataLoaded
            << "G::iconChunkLoaded =" << G::iconChunkLoaded
            << "src =" << src
            ;
    }

    QMutexLocker locker(&mutex);

    // reset
    abort = false;
    startRow = sfRow;
    this->src = src;
    sfRowCount = dm->sf->rowCount();
    lastRow = sfRowCount - 1;
    imageCacheTriggered = false;
    aIsDone = false;
    bIsDone = false;
    if (startRow == 0) bIsDone = true;
    isDone = false;
    success = false;                        // used to update statusbar
    // set icon range. G::iconChunkLoaded is set in MW::updateChange
    firstIconRow = dm->startIconRange;
    lastIconRow = dm->endIconRange;

    if (isDebug)
    {
        qDebug() << " ";
        qDebug().noquote()
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

    if (G::useUpdateStatus && !G::allMetadataLoaded) {
        // runStatus: isRunning, show, success, source
        emit runStatus(true, true, true, fun);
    }

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

    // check if nothing dispatched
    if (noReadersCycling()) {
        setIdle();
    }
}

void MetaRead::stop()
{
    QString fun = "MetaRead::stop";
    if (G::isLogger || G::isFlowLogger) G::log(fun);

    abort = true;
    stopReaders();

    metaReadThread.quit();
    metaReadThread.wait();
}

void MetaRead::stopReaders()
/*
    MetaRead::run dispatches all the readers and returns immediately.  The isDispatched flag
    indicates that dispatching is still running.
*/
{
    QString fun = "MetaRead::stopReaders";
    if (G::isLogger || G::isFlowLogger) G::log(fun);
    if (isDebug)
    {
        qDebug().noquote()
            << fun.leftJustified(col0Width)
            << "instance / dm->instance =" << instance << "/" << dm->instance;
    }

    // stop all readers
    for (int id = 0; id < readerCount; ++id) {
        QMetaObject::invokeMethod(readers[id], "stop", Qt::BlockingQueuedConnection);
    }
}

void MetaRead::abortProcessing()
{
/*
    Set the abort flag to cancel the current read operation, before changing folders
*/
    if (G::isLogger || G::isFlowLogger)
        G::log("MetaRead::abortProcessing", "starting");

    abort = true;

    // abort all readers
    for (int id = 0; id < readerCount; ++id) {
        if (readers[id]->pending) readers[id]->abortProcessing();
    }

    if (G::isLogger || G::isFlowLogger)
        G::log("MetaRead::abortProcessing", "emit stopped");

    emit stopped("MetaRead");
}

void MetaRead::setIdle()
{
    QMutexLocker lock(&mutex);
    idle = true;
}

void MetaRead::setBusy()
{
    QMutexLocker lock(&mutex);
    idle = false;
}

bool MetaRead::isIdle()
{
    QMutexLocker lock(&mutex);
    return idle;
}

bool MetaRead::isBusy()
{
    QMutexLocker lock(&mutex);
    return !idle;
}

bool MetaRead::allReadersCycling()
{
    for (bool isCycling : cycling) {
        if (!isCycling) return false;
    }
    return true;
}

bool MetaRead::noReadersCycling()
{
    for (bool isCycling : cycling) {
        if (isCycling) return false;
    }
    return true;
}

void MetaRead::initialize(QString src)
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
            << "src =" << src
            ;
    }

    // abort = true;
    // for (Reader *r : readers) r->abortProcessing();
    // abortReaders();
    dmRowCount = dm->rowCount();
    metaReadCount = 0;
    metaReadItems = dmRowCount;
    isAhead = true;
    aIsDone = false;
    bIsDone = false;
    isDone = false;
    success = false;
    quitAfterTimeoutInitiated = false;
    if (quitTimer->isActive()) quitTimer->stop();
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
    rpt.setFieldWidth(0);
    rpt << "\n";
    rpt << "\n";
    rpt.setFieldAlignment(QTextStream::AlignLeft);
    rpt << "Empty icon rows in datamodel:";
    rpt.setFieldAlignment(QTextStream::AlignRight);
    rpt.setFieldWidth(6);
    for (int i = 0; i < dm->sf->rowCount(); i++) {
        if (dm->iconLoaded(i, instance)) continue;
        rpt << "\nrow" << i;
    }
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

// void MetaRead::cleanupIcons() replaced by emit cleanupIcons() signals DataModel::clearIconsOutsideChunkRange
// {
// /*
//     Remove icons not in icon range after start row change, iconChunkSize change or
//     MW::deleteFiles.

//     The icon range is the lesser of iconChunkSize and dm->sf->rowCount(), centered on
//     the current datamodel row dm->currentSfRow.
// */
//     QString fun = "MetaRead::cleanupIcons";
//     if (G::isLogger) G::log(fun);
//     if (isDebug)
//         qDebug().noquote()
//             << fun.leftJustified(col0Width)
//             << "firstIconRow =" << firstIconRow
//             << "lastIconRow =" << lastIconRow
//             ;

//     // check if datamodel size is less than assigned icon cache chunk size
//     if (dm->iconChunkSize >= sfRowCount) {
//         return;
//     }

//     for (int i = 0; i < dm->startIconRange; i++) {
//         if (abort) return;
//         if (!dm->sf->index(i, 0).data(Qt::DecorationRole).isNull()) {
//             dm->sf->setData(dm->sf->index(i, 0), QVariant(), Qt::DecorationRole);
//             dm->sf->setData(dm->sf->index(i, G::IconLoadedColumn), false);
//         }
//     }
//     for (int i = dm->endIconRange + 1; i < sfRowCount; i++) {
//         if (abort) return;
//         if (!dm->sf->index(i, 0).data(Qt::DecorationRole).isNull()) {
//             dm->sf->setData(dm->sf->index(i, 0), QVariant(), Qt::DecorationRole);
//             dm->sf->setData(dm->sf->index(i, G::IconLoadedColumn), false);
//         }
//     }
// }

inline bool MetaRead::needToRead(int sfRow)
/*
    Returns true if either the metadata or icon (thumbnail) has not been loaded and
    not already reading.
*/
{
    needIcon = false;
    needMeta = false;

    bool isReading = dm->sf->index(sfRow, G::MetadataReadingColumn).data().toBool();
    bool isIcon = dm->sf->index(sfRow, G::IconLoadedColumn).data().toBool();
    bool isMeta = dm->sf->index(sfRow, G::MetadataAttemptedColumn).data().toBool();

    // already reading this item?
    // if (isReading || isIcon) {
    //     return false;
    // }
    // }
    // }
    // else {
    //     QModelIndex sfReadingIdx = dm->sf->index(sfRow, G::MetadataReadingColumn);
    //     dm->sf->setData(sfReadingIdx, true);
    // }

    if (isMeta && isIcon) {
        return false;
    }
    else {
        QModelIndex sfReadingIdx = dm->sf->index(sfRow, G::MetadataReadingColumn);
        dm->sf->setData(sfReadingIdx, true);
    }

    if (!isMeta) {
        needMeta = true;
    }

    if (!isIcon) {
        if (sfRow >= firstIconRow && sfRow <= lastIconRow) {
            needIcon = true;
        }
    }

    return needMeta || needIcon;
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
        }
    }
    return pendingCount;
}

void MetaRead::setCycling(bool isCycling)
{
    for (bool v : cycling) {
        v = isCycling;
    }
}

bool MetaRead::allMetaIconLoaded()
{
/*
    Has the datamodel been fully loaded?
*/
    // return dm->isAllMetadataAttempted() && dm->isIconRangeLoaded();

    // all metadata attempted and icons loaded into datamodel?
    bool metaAttempted;
    QMetaObject::invokeMethod(
        dm,
        "isAllMetadataAttempted",
        Qt::BlockingQueuedConnection,
        Q_RETURN_ARG(bool, metaAttempted)
        );

    bool iconAttempted;
    QMetaObject::invokeMethod(
        dm,
        "isAllIconChunkLoaded",
        Qt::BlockingQueuedConnection,
        Q_RETURN_ARG(bool, iconAttempted),
        Q_ARG(int, dm->startIconRange),
        Q_ARG(int, dm->endIconRange)
        );

    return metaAttempted && iconAttempted;
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

void MetaRead::processReturningReader(int id, Reader *r)
{
/*
    - report read failures
    - trigger fileSelectionChange if more than headStart rows read
    - update progress
    - if readers have been dispatched for all rows
         - check if still pending
         - check if all metadata and necessary icons loaded
         - if all checks out signal done
*/

    QString fun = "MetaRead::processReturningReader";
    int dmRow = r->dmRow;

    // progress counter
    metaReadCount++;

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

    // trigger MW::fileSelectionChange which starts ImageCache
    if (fileSelectionChanged &&
        !imageCacheTriggered &&
        instance == dm->instance &&
        dmRow == startRow
        )
    {
        imageCacheTriggered = true;
        // model and proxy rows the same in metaRead
        QModelIndex sfIdx = dm->sf->index(r->dmRow,0);
        bool clearSelection = true;
        QString src = "MetaRead::dispatch";
        // select the current index row in thumbView, gridView and tableView
        emit select(sfIdx, clearSelection);
        // notify file selection changed
        emit fileSelectionChange(sfIdx, QModelIndex(), clearSelection, src);
    }

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
    if (showProgressInStatusbar && !G::allMetadataLoaded) {
        emit updateProgressInStatusbar(dmRow, dm->rowCount(), darkRed);
        int progress = 1.0 * metaReadCount / dm->rowCount() * 100;
        emit updateProgressInFilter(progress);
    }

    // if readers have been dispatched for all rows
    if (aIsDone && bIsDone) {
        if (pending()) return;

        if (!allMetaIconLoaded()) {
            // if all readers finished but not all loaded, then redo
            if (redoCount < redoMax) {
                // try to read again
                QThread::msleep(50);
                if (!abort) {
                    redo();
                }
            }
            else {
                qWarning() << "REDO MAXED OUT";
            }
        }
        // Now we are done
        if (isDebug)
        {
            qDebug().noquote()
                << fun.leftJustified(col0Width)
                <<  "We are done"
                << QString::number(G::t.elapsed()).rightJustified((5)) << "ms"
                << "G::allMetadataLoaded =" << G::allMetadataLoaded
                //<< "toRead =" << toRead
                << "pending =" << pending()
                ;
        }

        dispatchFinished("WeAreDone");
    }
}

void MetaRead::dispatch(int id, bool isReturning)
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
    if (isDebug)
    {
        qDebug() << "MetaRead::dispatch id =" << id
             << "isReturning =" << isReturning
             << "dmRowCount =" << dmRowCount
        ;
    }

    // terse pointer to readers[id]
    r = readers[id];
    // r->pending = false;

    if (abort) {
        r->status = Reader::Status::Ready;
        return;
    }

    // New reader and less rows (images to read) than readers - do not dispatch
    if (!isReturning && id >= dmRowCount) {
        return;
    }

    if (isDebug)
    {
        QString  row;
        r->fPath == "" ? row = "-1" : row = QString::number(r->dmRow);
        QString fun1;
        if (isReturning) fun1 = fun + " reader returning";
        else fun1 = fun + " reader starting";
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
            << "r->instance =" << QString::number(r->instance).leftJustified(4, ' ')
            << "dm->instance =" << QString::number(dm->instance).leftJustified(4, ' ')
            << "isGUI =" << G::isGuiThread()
            ;
    }

    if (debugLog || (G::isLogger || G::isFlowLogger))
    {
        QString  row;
        r->fPath == "" ? row = "-1" : row = QString::number(r->dmRow);
        QString from;
        r->fPath == "" ? from = "start reader" : from = "return from reader";
        QString s = "id = " + QString::number(id).rightJustified(2, ' ');
        s += " row = " + row.rightJustified(4, ' ');
        G::log("MetaRead::dispatch: " + from, s);
    }

    // RETURNING READER
    if (isReturning && r->instance == dm->instance) {
        processReturningReader(id, r);

    }

    // DISPATCH READER

    /* check if new start row while dispatch reading all metadata. The user may have
    jumped to the end of a large folder while metadata is being read. Also could have
    scrolled. */

    if (isNewStartRowWhileDispatching) {
        a = startRow;
        b = startRow - 1;
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
        int dmRow = dmIdx.row();
        QString fPath = sfIdx.data(G::PathRole).toString();

        if (fPath.isEmpty())
            qWarning() << fun << "Warning:" << "row"
                       << nextRow << "fPath is empty";

        if (isDebug)
        {
            QString fun1 = fun + " invoke reader";
            // QString ms = msElapsed();
            // if (ms.trimmed().toInt() > 100)
            qDebug().noquote()
                // << ms
                << fun1.leftJustified(col0Width)
                << "id =" << QString::number(id).leftJustified(2, ' ')
                << "redo =" << QString::number(redoCount).leftJustified(2, ' ')
                << "dmRow =" << QString::number(dmIdx.row()).leftJustified(4, ' ')
                << "nextRow =" << QString::number(nextRow).leftJustified(4, ' ')
                << "okReadMeta =" << QVariant(needMeta).toString().leftJustified(5, ' ')
                << "okReadIcon =" << QVariant(needIcon).toString().leftJustified(5, ' ')
                << "isAhead =" << QVariant(isAhead).toString().leftJustified(5, ' ')
                << "aIsDone =" << QVariant(aIsDone).toString().leftJustified(5, ' ')
                << "bIsDone =" << QVariant(bIsDone).toString().leftJustified(5, ' ')
                << "a =" << QString::number(a).leftJustified(4, ' ')
                << "b =" << QString::number(b).leftJustified(4, ' ')
                << fPath
                ;
        }

        /* Read the image file metadata and icon.  When the reader is finished, it will
           signal dispatch (this function) to loop through to read another file and so
           on.  Must use invoke to prevent crash.  */
        if (!abort) {
            QMetaObject::invokeMethod(readers.at(id), "read", Qt::QueuedConnection,
                                      Q_ARG(int, dmRow),
                                      Q_ARG(QString, fPath),
                                      Q_ARG(int, dm->instance),
                                      Q_ARG(bool, needMeta),
                                      Q_ARG(bool, needIcon)
                                      );
        }
    }
    else {
        QString fun1 = fun + " no next row";
        if (isDebug)
        {
        qDebug().noquote()
            << fun1.leftJustified(col0Width)
            << "id =" << QString::number(id).leftJustified(2, ' ')
            << "redo =" << QString::number(redoCount).leftJustified(2, ' ')
            // << "dmRow =" << QString::number(dmIdx.row()).leftJustified(4, ' ')
            << "nextRow =" << QString::number(nextRow).leftJustified(4, ' ')
            << "okReadMeta =" << QVariant(needMeta).toString().leftJustified(5, ' ')
            << "okReadIcon =" << QVariant(needIcon).toString().leftJustified(5, ' ')
            << "isAhead =" << QVariant(isAhead).toString().leftJustified(5, ' ')
            << "aIsDone =" << QVariant(aIsDone).toString().leftJustified(5, ' ')
            << "bIsDone =" << QVariant(bIsDone).toString().leftJustified(5, ' ')
            << "a =" << QString::number(a).leftJustified(4, ' ')
            << "b =" << QString::number(b).leftJustified(4, ' ')
            ;
        }
            cycling[id] = false;
    }

    // if done in both directions fire delay to quit in case isDone fails
    if (aIsDone && bIsDone && !isDone) {
        quitAfterTimeout();
    }
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
    bool isReturning = false;

    for (int id = 0; id < readers.count(); id++) {
        if (isDebug)
        {
            qDebug().noquote()
            << fun.leftJustified(col0Width)
            << "id =" << id
            << "cycling =" << cycling.at(id)
            ;
        }

        // dispatch(id, isReturning);
        // continue;

        // if (!reader[id]->pending) {
        if (!cycling.at(id)) {
            // readers[id]->status = Reader::Status::Ready;
            // readers[id]->fPath = "";
            // readers[id]->instance = dm->instance;
            cycling[id] = true;
            if (!abort) dispatch(id, isReturning);
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

void MetaRead::quitAfterTimeout()
{
    // if pending readers not all processed in delay ms then quit anyway

    QString fun = "MetaRead::quitAfterTimeout";
    if (isDebug)
    {
        qDebug().noquote()
        << fun.leftJustified(col0Width)
            ;
    }

    if (!quitAfterTimeoutInitiated) {
        if (isDebug)
        {
            qDebug().noquote()
            << "MetaRead::dispatch     aIsDone && bIsDone "
            << QString::number(G::t.elapsed()).rightJustified((5)) << "ms"; G::t.restart();}
        quitAfterTimeoutInitiated = true;
        if (debugLog && (G::isLogger || G::isFlowLogger))
        {
            G::log("MetaRead::dispatch", "aIsDone && bIsDone  quitAfterTimeoutInitiated in 1000 ms");
        }
        quitTimer->start(2000);
    }

    return;
}

void MetaRead::dispatchFinished(QString src)
{
    if (quitTimer->isActive()) quitTimer->stop();
    isDone = true;

    QString fun = "MetaRead::dispatchFinished";
    if (debugLog && (G::isLogger || G::isFlowLogger))
        G::log(fun, src);
    if (isDebug)
    {
        qDebug().noquote()
            << fun.leftJustified(col0Width)
            << "src =" << src
            << "G::allMetadataLoaded =" << G::allMetadataLoaded
            ;
    }

    bool running = false;
    setCycling(false);
    bool show = true;
    success = allMetaIconLoaded();
    if (G::useUpdateStatus && !G::allMetadataLoaded) {
        emit runStatus(running, show, success, fun);
    }
    emit cleanupIcons();
    isDispatching = false;

    G::iconChunkLoaded = dm->isIconRangeLoaded();

    // do not emit done if only updated icon loading
    if (!G::allMetadataLoaded) {
        G::allMetadataLoaded = true;
        // signal MW::folderChangeCompleted
        emit done();
    }

    setIdle();
}
