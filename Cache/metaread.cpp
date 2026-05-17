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

    // Single shared FrameDecoder on a dedicated thread. Every Reader (and its
    // Thumb) emits videoFrameDecode to this one instance, so at most one
    // QMediaPlayer is alive at a time — eliminates the AVFoundation
    // heap-pressure window that the memoryOverrunFlag path in
    // FrameDecoder::cleanupPlayer was working around.
    frameDecoder = new FrameDecoder();
    frameDecoderThread = new QThread;
    frameDecoder->moveToThread(frameDecoderThread);
    frameDecoderThread->start();
    connect(frameDecoder, &FrameDecoder::setFrameIcon,
            dm, &DataModel::setIconFromVideoFrame);
    connect(frameDecoder, &FrameDecoder::videoFrameFailed,
            dm, &DataModel::clearVideoReadingFlag);
    connect(frameDecoder, &FrameDecoder::setValDm,
            dm, &DataModel::setValDm);

    readerCount = QThread::idealThreadCount() - 2;
    for (int id = 0; id < readerCount; ++id) {
        Reader *reader = new Reader(id, dm, imageCache, frameDecoder);
        QThread *thread = new QThread;
        reader->readerThread = thread;
        reader->moveToThread(thread);
        connect(reader, &Reader::done, this, &MetaRead::dispatch);
        connect(thread, &QThread::finished, reader, &QObject::deleteLater);
        connect(thread, &QThread::finished, thread, &QObject::deleteLater);
        thread->start(QThread::LowPriority);
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

    memoryProbeTimer.start();
}

bool MetaRead::checkMemoryFootprint()
{
/*
    Cheap periodic probe of the process's resident footprint. Returns true
    if the cap was breached and abort/notification has been triggered.

    Probe rate is throttled so the underlying syscall (Mac: task_info,
    Win: GetProcessMemoryInfo) never dominates dispatch. Every 32
    dispatches OR every 250 ms — whichever comes first — keeps the
    response latency < 0.5 s on typical M-series hardware while costing
    < 0.1 % of dispatch CPU.

    Once tripped, G::memoryOverrunFlag latches so other subsystems can
    short-circuit cheaply without re-probing.
*/
    if (G::memoryOverrunFlag.load(std::memory_order_relaxed)) return true;

    /* Higher caps (≥ 8 GB) leave very little headroom on a 24 GB Mac, so
       probe aggressively. task_info on macOS is microseconds — even at
       every 4 dispatches the overhead is < 0.1 % of dispatch CPU. */
    constexpr int kProbeEveryN = 4;
    constexpr qint64 kProbeEveryMs = 50;
    if (++memoryProbeCounter < kProbeEveryN
        && memoryProbeTimer.elapsed() < kProbeEveryMs) {
        return false;
    }
    memoryProbeCounter = 0;
    memoryProbeTimer.restart();

    const quint64 cap = G::memoryAbortMB;
    if (cap == 0) return false;
    const quint64 footprintMB = G::processFootprintMB();
    if (footprintMB == 0 || footprintMB < cap) return false;

    // Latch and trigger abort. Only the thread that wins the CAS emits;
    // this prevents MW::onMemoryOverrun from being signalled twice when
    // another subsystem (DataModel, watchdog) trips concurrently.
    abort = true;
    if (dm) dm->abort = true;
    bool expected = false;
    if (G::memoryOverrunFlag.compare_exchange_strong(
            expected, true,
            std::memory_order_acq_rel, std::memory_order_relaxed)) {
        emit memoryOverrun(footprintMB, cap);
    }
    return true;
}

MetaRead::~MetaRead()
{
    if (isDebug) qDebug() << "MetaRead::~MetaRead";
    // Reader threads quit via their own stop() path; once they're done no
    // Reader can emit videoFrameDecode, so it's safe to bring down the
    // shared FrameDecoder thread next.
    if (frameDecoderThread) {
        frameDecoderThread->quit();
        frameDecoderThread->wait();
        delete frameDecoder;
        delete frameDecoderThread;
        frameDecoder = nullptr;
        frameDecoderThread = nullptr;
    }
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

    // QMutexLocker locker(&mutex);  // freeze when FocusStack has just run

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
        // Only clear the per-cycle set when the instance actually changes.
        // Within-instance setStartRow calls (scroll, file selection) must
        // preserve the set, otherwise rows already returned by readers
        // become eligible for re-dispatch while their setIcon is still in
        // flight on the main thread.
        readSuccessThisCycle.clear();
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
        // t.start();
    }

    dispatchReaders();

    // check if nothing dispatched
    if (noReadersCycling()) {
        setIdle();
    }
}

void MetaRead::stop()
{
    QString srcFun = "MetaRead::stop";
    if (G::isLogger || G::isFlowLogger) G::log(srcFun);

    abort = true;
    // Flush any queued video work on the shared FrameDecoder. Per-Reader
    // abort no longer touches FrameDecoder (would clobber other Readers'
    // pending items), so the global flush happens here.
    if (frameDecoder) {
        QMetaObject::invokeMethod(frameDecoder, "stop", Qt::QueuedConnection);
    }
    stopReaders();

    metaReadThread.quit();
    // metaReadThread.wait();

    if (isIdle()) {
        emit stopped(srcFun);
    }
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

    // Flush the shared FrameDecoder's queue. Per-Reader signalAbort no
    // longer touches FrameDecoder, so this is the single choke point for
    // folder-change aborts.
    if (frameDecoder) {
        QMetaObject::invokeMethod(frameDecoder, "stop", Qt::QueuedConnection);
    }

    // Signal all readers to abort simultaneously (non-blocking)
    for (int id = 0; id < readerCount; ++id) {
        readers[id]->signalAbort();
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
    // QMutexLocker lock(&mutex); //
    idle = false;
}

bool MetaRead::isIdle()
{
    // QMutexLocker lock(&mutex); //
    return idle;
}

bool MetaRead::isBusy()
{
    // QMutexLocker lock(&mutex); //
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

void MetaRead::debugRunStatus()
{
    for (int id = 0; id < readerCount; ++id) {
        auto reader = readers.at(id);
        qDebug() << "    - Reader " << id << ":" << (reader->readerThread->isRunning() ? "RUNNING" : "Stopped");
                 // << " | isIdle:" << reader->isIdle();
    }
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
    allFinishedFired = false;
    success = false;
    quitAfterTimeoutInitiated = false;
    if (quitTimer->isActive()) quitTimer->stop();
    redoCount = 0;
    redoMax = 5;
    err.clear();
    cycling.fill(false);
    readSuccessThisCycle.clear();

    // reset diagnostic lifetime counters
    readsSuccessCount.store(0, std::memory_order_relaxed);
    readsFailedCount.store(0, std::memory_order_relaxed);
    redosTriggeredCount.store(0, std::memory_order_relaxed);
    dispatchCycleCount.store(0, std::memory_order_relaxed);

    t.start();
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
    rpt << "\n\n";

    // Health checks first, then lifetime counters, then state, then tables.
    rpt << reportHealthChecks();
    rpt << reportLifetimeCounters();

    const int dmInst = dm->instance.load();

    // Key/value scalar block. Single 28-char label column for consistency.
    auto kv = [&rpt](const QString &label, const QString &value) {
        rpt.setFieldAlignment(QTextStream::AlignLeft);
        rpt.setFieldWidth(28);
        rpt << label;
        rpt.setFieldWidth(0);
        rpt << value << "\n";
    };

    rpt << "Instance / range:\n";
    kv("instance",                   QString::number(instance));
    kv("dm->instance",               QString::number(dmInst));
    kv("readerCount",                QString::number(readerCount));
    kv("expansionFactor",            QString::number(expansionFactor));
    kv("sfRowCount",                 QString::number(sfRowCount));
    kv("dmRowCount",                 QString::number(dmRowCount));
    kv("dm->currentSfRow",           QString::number(dm->currentSfRow));

    rpt << "\nIcon chunk:\n";
    kv("defaultIconChunkSize",       QString::number(dm->defaultIconChunkSize));
    kv("iconChunkSize",              QString::number(dm->iconChunkSize));
    kv("iconLimit",                  QString::number(iconLimit));
    kv("firstVisibleIcon",           QString::number(dm->firstVisibleIcon));
    kv("lastVisibleIcon",            QString::number(dm->lastVisibleIcon));
    kv("visibleIcons",               QString::number(dm->visibleIcons));
    kv("firstIconRow",               QString::number(firstIconRow));
    kv("lastIconRow",                QString::number(lastIconRow));
    kv("dm->startIconRange",         QString::number(dm->startIconRange));
    kv("dm->endIconRange",           QString::number(dm->endIconRange));
    kv("dm->iconCount",              QString::number(dm->iconCount()));

    rpt << "\nDispatch state:\n";
    kv("isDispatching",              QVariant(isDispatching).toString());
    kv("isDone",                     QVariant(isDone).toString());
    kv("aIsDone",                    QVariant(aIsDone).toString());
    kv("bIsDone",                    QVariant(bIsDone).toString());
    kv("abort",                      QVariant(abort).toString());
    kv("isRunning (thread)",         QVariant(metaReadThread.isRunning()).toString());
    kv("isRunloop",                  QVariant(runloop.isRunning()).toString());
    kv("startRow",                   QString::number(startRow));
    kv("lastRow",                    QString::number(lastRow));
    kv("nextRow",                    QString::number(nextRow));
    kv("a (ahead)",                  QString::number(a));
    kv("b (behind)",                 QString::number(b));
    kv("isAhead",                    QVariant(isAhead).toString());
    kv("fileSelectionChanged",       QVariant(fileSelectionChanged).toString());
    kv("isNewStartRowWhileDisp.",    QVariant(isNewStartRowWhileDispatching).toString());
    kv("imageCacheTriggered",        QVariant(imageCacheTriggered).toString());
    kv("success",                    QVariant(success).toString());
    kv("quitAfterTimeoutInitiated",  QVariant(quitAfterTimeoutInitiated).toString());
    kv("readSuccessThisCycle (size)",QString::number(readSuccessThisCycle.size()));
    kv("redoCount / redoMax",        QString::number(redoCount) + " / " + QString::number(redoMax));

    rpt << "\nGlobals:\n";
    kv("dm->isAllMetadataAttempted", QVariant(dm->isAllMetadataAttempted()).toString());
    kv("G::allMetadataLoaded",       QVariant(G::allMetadataLoaded).toString());
    kv("G::iconChunkLoaded",         QVariant(G::iconChunkLoaded).toString());
    kv("G::maxIconChunk",            QString::number(G::maxIconChunk));
    kv("G::memoryAbortMB",           QString::number(G::memoryAbortMB));

    rpt << "\nTimers / probe:\n";
    kv("memoryProbeCounter",         QString::number(memoryProbeCounter));
    kv("memoryProbeTimer.elapsed",   QString::number(memoryProbeTimer.elapsed()) + " ms");
    kv("t.elapsed (dispatch start)", QString::number(t.isValid() ? t.elapsed() : -1) + " ms");

    // Reader status table — header and body use matching field widths so they
    // actually align (the prior version had a spaced-string header that drifted
    // out of sync with the body widths).
    rpt << "\nReader status: (dm->instance = " << dmInst << ")\n";
    auto rhdr = [&rpt](int w, const QString &s) {
        rpt.setFieldWidth(w); rpt << s;
    };
    rpt.setFieldAlignment(QTextStream::AlignRight);
    rhdr(4,  "ID");
    rhdr(10, "Cycling");
    rhdr(10, "Pending");
    rhdr(12, "decInst");
    rhdr(8,  "dmRow");
    rpt.setFieldAlignment(QTextStream::AlignLeft);
    rhdr(2,  "  ");
    rhdr(16, "Status");
    rpt.setFieldWidth(0);
    rpt << "errMsg\n";
    for (int id = 0; id < readerCount; ++id) {
        const int decInst = readers[id]->instance.load();
        rpt.setFieldAlignment(QTextStream::AlignRight);
        rpt.setFieldWidth(4);  rpt << id;
        rpt.setFieldWidth(10); rpt << QVariant(cycling.at(id)).toString();
        rpt.setFieldWidth(10); rpt << QVariant(readers[id]->isPending()).toString();
        rpt.setFieldWidth(12);
        rpt << (decInst == dmInst
                ? QString::number(decInst)
                : QString::number(decInst) + "!");
        rpt.setFieldWidth(8);  rpt << readers[id]->dmRow.load();
        rpt.setFieldAlignment(QTextStream::AlignLeft);
        rpt.setFieldWidth(2);  rpt << "  ";
        rpt.setFieldWidth(16); rpt << readers[id]->statusText.at(readers[id]->status);
        rpt.setFieldWidth(0);
        rpt << readers[id]->errMsg << "\n";
    }

    // Errors. Keep the field width reset around the loop so it can't leak.
    rpt.setFieldWidth(0);
    rpt << "\nErrors: " << err.length() << " items\n";
    for (int i = 0; i < err.length(); i++) {
        rpt << "  " << err.at(i) << "\n";
    }

    // Row-list summaries. Both "metadata not loaded" and "icons not loaded"
    // can be folder-wide huge; the user-visible failure is in the *visible*
    // range. Lead with that, follow with a capped folder-wide count.
    auto summariseRows = [&rpt](const QString &label, const QList<int> &rows,
                                int firstVisible, int lastVisible) {
        int visMiss = 0;
        QList<int> visSample;
        for (int r : rows) {
            if (r >= firstVisible && r <= lastVisible) {
                ++visMiss;
                if (visSample.size() < 10) visSample.append(r);
            }
        }
        rpt << label << ": " << rows.size() << " total";
        if (lastVisible >= firstVisible) {
            const int visCount = lastVisible - firstVisible + 1;
            rpt << " (" << visMiss << " of " << visCount << " visible)";
        }
        rpt << "\n";
        if (!visSample.isEmpty()) {
            QStringList s;
            for (int r : visSample) s << QString::number(r);
            rpt << "  visible sample: " << s.join(", ");
            if (visMiss > visSample.size()) rpt << "  (+" << (visMiss - visSample.size()) << " more)";
            rpt << "\n";
        }
    };

    rpt << "\n";
    summariseRows("Metadata rows not loaded",
                  dm->metadataNotLoaded(),
                  dm->firstVisibleIcon, dm->lastVisibleIcon);

    // Build the empty-icon list once, then summarise.
    {
        QList<int> emptyIcons;
        const int nRows = dm->sf->rowCount();
        for (int i = 0; i < nRows; ++i) {
            if (!dm->iconLoaded(i, instance)) emptyIcons.append(i);
        }
        summariseRows("Icon rows not loaded",
                      emptyIcons,
                      dm->firstVisibleIcon, dm->lastVisibleIcon);
    }

    rpt << "\n";
    return reportString;
}

QString MetaRead::reportHealthChecks()
{
/*
    Invariant checks printed at the top of the diagnostic. Each check prints
    [OK] or [WARN] so anomalies surface before the reader scrolls through
    state and tables. Mirrors ImageCache::reportHealthChecks.
*/
    if (G::isLogger) G::log("MetaRead::reportHealthChecks");

    QString reportString;
    QTextStream rpt;
    rpt.setString(&reportString);

    auto line = [&](const QString &ok, const QString &name, const QString &detail) {
        rpt << "[" << ok.leftJustified(4, ' ') << "] "
            << name.leftJustified(28, ' ') << ": " << detail << "\n";
    };

    rpt << "Health checks:\n";

    // 1. Thread state.
    if (metaReadThread.isRunning()) {
        line("OK", "thread state",
             QString("running, abort=%1").arg(abort ? "true" : "false"));
    } else if (abort) {
        line("OK", "thread state", "stopped (abort=true)");
    } else {
        line("WARN", "thread state", "thread not running but abort=false");
    }

    // 2. Dispatch / runloop coherence.
    if (isDispatching && !runloop.isRunning()) {
        line("WARN", "dispatch coherence",
             "isDispatching=true but runloop not running");
    } else {
        line("OK", "dispatch coherence",
             QString("isDispatching=%1 runloop=%2")
                 .arg(isDispatching ? "true" : "false")
                 .arg(runloop.isRunning() ? "true" : "false"));
    }

    // 3. Done flags coherence.
    {
        const bool both = aIsDone && bIsDone;
        if (isDone != both) {
            line("WARN", "done flags",
                 QString("isDone=%1 but aIsDone && bIsDone=%2")
                     .arg(isDone ? "true" : "false")
                     .arg(both ? "true" : "false"));
        } else {
            line("OK", "done flags",
                 QString("isDone=%1").arg(isDone ? "true" : "false"));
        }
    }

    // 4. Per-reader DM instance.
    {
        const int dmInst = dm->instance.load();
        QStringList stale;
        for (int id = 0; id < readerCount; ++id) {
            const int dec = readers[id]->instance.load();
            if (dec != dmInst) stale << QString("%1(dec=%2)").arg(id).arg(dec);
        }
        if (stale.isEmpty()) {
            line("OK", "reader DM instance",
                 QString("all at dm->instance=%1").arg(dmInst));
        } else {
            line("WARN", "reader DM instance",
                 QString("dm->instance=%1, stale: [%2]")
                     .arg(dmInst).arg(stale.join(",")));
        }
    }

    // 5. Visible-range starvation. Rows currently visible but icon not loaded
    //    are the user-visible failure mode.
    {
        const int first = dm->firstVisibleIcon;
        const int last  = dm->lastVisibleIcon;
        int missing = 0;
        QList<int> samples;
        if (last >= first) {
            const int nRows = dm->sf->rowCount();
            const int hi = qMin(last, nRows - 1);
            for (int r = qMax(0, first); r <= hi; ++r) {
                if (!dm->iconLoaded(r, instance)) {
                    ++missing;
                    if (samples.size() < 5) samples.append(r);
                }
            }
        }
        if (missing == 0) {
            line("OK", "visible icons", "all loaded");
        } else {
            QStringList s;
            for (int r : samples) s << QString::number(r);
            line("WARN", "visible icons",
                 QString("%1 of %2 visible rows unloaded (e.g. %3)")
                     .arg(missing)
                     .arg(last - first + 1)
                     .arg(s.join(", ")));
        }
    }

    // 6. Post-redo race guard: rows in readSuccessThisCycle should not also
    //    show !MetadataAttempted (the very window the set was added to bridge).
    {
        int overlap = 0;
        for (int r : readSuccessThisCycle) {
            if (!dm->sf->index(r, G::MetadataAttemptedColumn).data().toBool()) {
                ++overlap;
            }
        }
        if (overlap == 0) {
            line("OK", "post-redo race guard",
                 QString("%1 rows in set, no race window observed").arg(readSuccessThisCycle.size()));
        } else {
            line("WARN", "post-redo race guard",
                 QString("%1 rows in set, %2 still flagged !MetadataAttempted")
                     .arg(readSuccessThisCycle.size()).arg(overlap));
        }
    }

    // 7. Probe activity.
    if (isDispatching && memoryProbeTimer.isValid()
            && memoryProbeTimer.elapsed() > 60000) {
        line("WARN", "memory probe",
             QString("last fire %1 ms ago (isDispatching=true)")
                 .arg(memoryProbeTimer.elapsed()));
    } else {
        line("OK", "memory probe",
             QString("counter=%1, last fire %2 ms ago")
                 .arg(memoryProbeCounter)
                 .arg(memoryProbeTimer.isValid() ? memoryProbeTimer.elapsed() : -1));
    }

    rpt << "\n";
    return reportString;
}

QString MetaRead::reportLifetimeCounters()
{
/*
    Counters reset on each initialize(). Useful for spotting trim/decode
    races and unbounded retry loops that aren't visible in a single snapshot.
*/
    if (G::isLogger) G::log("MetaRead::reportLifetimeCounters");

    QString reportString;
    QTextStream rpt;
    rpt.setString(&reportString);
    rpt << "Lifetime counters (since folder load):\n";
    rpt << "  reads successful       : "
        << Utilities::fitNumber(readsSuccessCount.load(),  12) << "\n";
    rpt << "  reads failed (terminal): "
        << Utilities::fitNumber(readsFailedCount.load(),   12) << "\n";
    rpt << "  redos triggered        : "
        << Utilities::fitNumber(redosTriggeredCount.load(),12) << "\n";
    rpt << "  dispatch cycles        : "
        << Utilities::fitNumber(dispatchCycleCount.load(), 12) << "\n";
    rpt << "\n";
    return reportString;
}

inline bool MetaRead::needToRead(int sfRow)
/*
    Returns true if either the metadata or icon (thumbnail) has not been loaded and
    not already reading.
*/
{
    needIcon = false;
    needMeta = false;

    QModelIndex sfReadingIdx = dm->sf->index(sfRow, G::MetadataReadingColumn);
    bool isReading = sfReadingIdx.data().toBool();
    // bool isReading = dm->sf->index(sfRow, G::MetadataReadingColumn).data().toBool();
    bool isIcon = dm->sf->index(sfRow, G::IconLoadedColumn).data().toBool();
    bool isMeta = dm->sf->index(sfRow, G::MetadataAttemptedColumn).data().toBool();

    // if (isDebug)
    //     qDebug() << "MetaRead::needToRead"
    //              << "sfRow =" << sfRow
    //              << "isReading =" << isReading
    //              << "isIcon =" << isIcon
    //              << "isMeta =" << isMeta
    //         ;

    // already reading this item?
    if (isReading || isIcon) {
        return false;
    }

    /* Already returned Success in this dispatch cycle.
       Guards against the post-redo race where the proxy's
       IconLoadedColumn for the last-completing row hasn't yet been
       published by the main thread, so isIcon above looks false even
       though the Reader successfully loaded the icon. */
    if (readSuccessThisCycle.contains(sfRow)) {
        return false;
    }

    if (isMeta && isIcon) {
        dm->sf->setData(sfReadingIdx, false);
        return false;
    }
    else {
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
        if (readers[id]->isPending()) {
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

    Reads atomic flags maintained by DataModel on the main thread:
    - G::allMetadataLoaded — published in DataModel::addMetadataForItem
    - G::iconChunkLoaded   — published in DataModel::setIcon, setIcon1, setIconRange

    The previous implementation used Qt::BlockingQueuedConnection to dispatch
    isAllMetadataAttempted/isAllIconChunkLoaded onto the main thread. That
    blocked this worker for an event-loop turn each call and risked deadlock
    if the main thread was waiting on us. The atomics are slightly behind the
    truth (one queued-slot turn) but the dispatch redo loop catches up.
*/
    return G::allMetadataLoaded && G::iconChunkLoaded;
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
    redosTriggeredCount.fetch_add(1, std::memory_order_relaxed);
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
    // Snapshot Reader's QString state under its mutex to avoid a race with a
    // re-dispatched Reader::read() concurrently overwriting fPath.
    QString rfPath = r->fPathSnapshot();

    // For video rows, FrameDecoder finishes asynchronously and
    // DataModel::setIconFromVideoFrame is what clears MetadataReadingColumn.
    // Clearing it here would let dispatch re-pick the same video before its
    // frame is decoded, spawning concurrent QMediaPlayers on the same file
    // (AVFoundation heap corruption).
    if (!dm->index(dmRow, G::VideoColumn).data().toBool()) {
        dm->sf->setData(dm->sf->index(dmRow, G::MetadataReadingColumn), false);
    }

    /* Record that this row has been processed in the current cycle so
       needToRead won't pick it again before its setIcon has drained on
       the main thread. Insert regardless of Reader::status — even a
       failed read got far enough to emit an error-icon setIcon, which
       will land IconLoadedColumn=true on the main thread eventually.
       Aborted reads are handled by setStartRow/initialize clearing the
       set when a new cycle begins. */
    {
        QModelIndex dmIdx = dm->index(dmRow, 0);
        QModelIndex sfIdx = dm->sf->mapFromSource(dmIdx);
        if (sfIdx.isValid()) readSuccessThisCycle.insert(sfIdx.row());
    }

    // progress counter
    metaReadCount++;

    // it is not ok to select while the datamodel is being built.
    if (metaReadCount == 1) emit okToSelect(true);

    // lifetime counters: success vs terminal failure (Aborted/Ready are transient)
    {
        const Reader::Status st = r->status.load();
        if (st == Reader::Status::Success) {
            readsSuccessCount.fetch_add(1, std::memory_order_relaxed);
        }
        else if (st == Reader::Status::MetaFailed ||
                 st == Reader::Status::IconFailed ||
                 st == Reader::Status::MetaIconFailed) {
            readsFailedCount.fetch_add(1, std::memory_order_relaxed);
        }
    }

    // report read failure
    if (!(r->status == r->Status::Success /*|| r->status == r->Status::Ready*/)) {
        QString error = "row " + QString::number(dmRow).rightJustified(5) + " " +
                        r->statusText.at(r->status).leftJustified(10) + " " + rfPath;
        err.append(error);
        if (isDebug)
        {
            QString fun1 = fun + " Failure";
            qDebug().noquote()
                << fun1.leftJustified(col0Width)
                << "id =" << QString::number(id).leftJustified(2, ' ')
                << "row =" << dmRow
                << "status =" << r->statusText.at(r->status)
                << rfPath;
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
    if (!isDone && showProgressInStatusbar && !G::allMetadataLoaded) {
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
    dispatchCycleCount.fetch_add(1, std::memory_order_relaxed);
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

    if (abort || dm->sf->isSuspended()) {
        r->status = Reader::Status::Ready;
        return;
    }

    // New reader and less rows (images to read) than readers - do not dispatch
    if (!isReturning && id >= dmRowCount) {
        return;
    }

    if (isDebug || debugLog || (G::isLogger || G::isFlowLogger))
    {
        // Snapshot Reader's QString state for this debug/log scope only.
        QString rfPath = r->fPathSnapshot();
        if (isDebug)
        {
        QString  row;
        rfPath == "" ? row = "-1" : row = QString::number(r->dmRow);
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
            << rfPath
            // << "r->instance =" << QString::number(r->instance).leftJustified(4, ' ')
            // << "dm->instance =" << QString::number(dm->instance).leftJustified(4, ' ')
            // << "isGUI =" << G::isGuiThread()
            // << "firstIconRow =" << firstIconRow
            // << "lastIconRow =" << lastIconRow
            ;
        }

    if (debugLog || (G::isLogger || G::isFlowLogger))
    {
        QString  row;
        rfPath == "" ? row = "-1" : row = QString::number(r->dmRow);
        QString from;
        rfPath == "" ? from = "start reader" : from = "return from reader";
        QString s = "id = " + QString::number(id).rightJustified(2, ' ');
        s += " row = " + row.rightJustified(4, ' ');
        G::log("MetaRead::dispatch: " + from, s);
    }
    } // end debug/log scope

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

    /* Memory-overrun guardrail: probe phys_footprint periodically and
       abort cleanly if it exceeds G::memoryAbortMB. checkMemoryFootprint
       throttles the underlying syscall so this is essentially free. */
    if (checkMemoryFootprint()) {
        return;
    }

    /* Backpressure: if the GUI thread is falling behind processing queued
       Reader emits, defer this dispatch instead of piling more onto the
       queue. Cap is generous (4 × readerCount in flight) so steady-state
       throughput is unaffected; only pathological producer/consumer
       imbalance (e.g. recursing an Apple .photoslibrary, where readers
       chew through small JPG derivatives faster than the GUI can drain
       addToDatamodel events) triggers the gate. nextRowToRead has not yet
       been called, so the next-row pointer is preserved across the wait. */
    if (!abort) {
        const int kQueueCap = 4 * readerCount;
        if (dm->queuedReaderEvents.load(std::memory_order_relaxed) >= kQueueCap) {
            QTimer::singleShot(20, this, [this, id]() {
                if (!abort) dispatch(id, false);
            });
            return;
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
                // << "redo =" << QString::number(redoCount).leftJustified(2, ' ')
                << "dmRow =" << QString::number(dmIdx.row()).leftJustified(4, ' ')
                // << "nextRow =" << QString::number(nextRow).leftJustified(4, ' ')
                // << "okReadMeta =" << QVariant(needMeta).toString().leftJustified(5, ' ')
                // << "okReadIcon =" << QVariant(needIcon).toString().leftJustified(5, ' ')
                << "needMeta =" << QVariant(needMeta).toString().leftJustified(5, ' ')
                << "needIcon =" << QVariant(needIcon).toString().leftJustified(5, ' ')
                // << "isAhead =" << QVariant(isAhead).toString().leftJustified(5, ' ')
                // << "aIsDone =" << QVariant(aIsDone).toString().leftJustified(5, ' ')
                // << "bIsDone =" << QVariant(bIsDone).toString().leftJustified(5, ' ')
                // << "a =" << QString::number(a).leftJustified(4, ' ')
                // << "b =" << QString::number(b).leftJustified(4, ' ')
                << "memMB =" << G::processFootprintMB()
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
            << "firstIconRow =" << firstIconRow
            << "lastIconRow =" << lastIconRow
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

        // Stall snapshot: we're giving up on the dispatch cycle but the
        // metadata/icon state isn't fully loaded. Throttled to once per 60s.
        if (autoLogStalls && !allMetaIconLoaded()) {
            const qint64 nowMsec = QDateTime::currentMSecsSinceEpoch();
            if (nowMsec - lastStallSnapshotMs > 60000) {
                lastStallSnapshotMs = nowMsec;
                QString path = QStandardPaths::writableLocation(
                                    QStandardPaths::AppDataLocation) + "/Log";
                QDir().mkpath(path);
                QFile f(path + "/metaread_stall.txt");
                if (f.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
                    QTextStream out(&f);
                    out << "\n\n===== MetaRead stall snapshot @ "
                        << QDateTime::currentDateTime().toString(Qt::ISODate)
                        << " =====\n";
                    out << diagnostics();
                }
            }
        }

        quitTimer->start(2000);
    }

    dispatchFinished(fun);

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

    setCycling(false);
    emit cleanupIcons(instance);
    isDispatching = false;

    allFinished(src);
}

void MetaRead::allFinished(QString src)
{
    // Runs once per folder load, after the last row has been read and saved
    // into the DataModel. Reset in initialize() only — setStartRow is called
    // for in-folder navigation and must not re-arm this guard.
    if (allFinishedFired) return;
    allFinishedFired = true;

    QString fun = "MetaRead::allFinished";
    if (debugLog && (G::isLogger || G::isFlowLogger))
        G::log(fun, src);

    G::allMetadataLoaded = true;
    emit runStatus(false, true, true, fun); // running, show, success, src
    emit done();                            // signal MW::folderChangeCompleted

    qint64 ms = t.elapsed();
    int n = dm->rowCount();
    int msPerImage = ms / n;
    qDebug() << fun << "Elapsed ms =" << ms
             << "ms per image =" << msPerImage
             << " for" << n << "images";
    setIdle();
}
