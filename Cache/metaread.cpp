#include "metaread.h"

/*
    MetaRead, running in a separate thread, loads the metadata and icons into the
    datamodel (dm) for a folder. All the metadata will be loaded, and icons will be
    loaded up to either the iconChunkSize or visibleIcons, depending on the preferences.
    ImageCache list data will be built.

    MetaRead::setCurrentRow is called when:
        • a new folder is selected
        • a new image is selected
        • thumbnails are scrolled
        • the gridView or thumbView are resized

    Steps:
        • Set the start position in the DataModel.
        • Start iterating the DataModel Proxy rows, starting with the current row and
          alternating one ahead and one behind, loading the metadata and icons.
        • Part way through the iteration start the ImageCache.
        • Cleanup (remove) icons exceeding amount set in preferences.  If loadOnlyVisibleIcons
          then remove all icons except those visible in either thumbView or gridView.  Other-
          wise, remove icons exceeding iconChunkSize based on the priorityQueue.
        • Abort and restart when a new setCurrentRow is called.

    Note: All data in the DataModel must be set using a queued connection.  When subsequent
    actions are dependent on the data being set use Qt::BlockingQueuedConnection.  This can
    result in a deadlock with thread wait functions, so the stop function does its own
    custom check.

    Note: This thread does not have to be stopped when a new folder is selected.  run() is
    self healing, and can adapt to new folders.  This was added before I figured out how to
    avoid deadlocks stopping the thread.
*/

MetaRead::MetaRead(QObject *parent,
                   DataModel *dm,
                   Metadata *metadata,
                   FrameDecoder *frameDecoder)
    : QThread(parent)
{
    if (isDebug || G::isLogger) G::log("MetaRead::MetaRead");
    this->dm = dm;
    this->metadata = metadata;
    this->frameDecoder = frameDecoder;
    thumb = new Thumb(dm, metadata, frameDecoder);
    imageCacheTriggerCount =  1;
//    imageCacheTriggerCount =  QThread::idealThreadCount();
    showProgressInStatusbar = true;
    isDebug = false;
}

MetaRead::~MetaRead()
{
//    if (isDebug) qDebug() << "MetaRead::~MetaRead";
//    delete thumb;
}

//void MetaRead::setStartRow(int row, bool isCurrent, QString src)
void MetaRead::setStartRow(int row, bool fileSelectionChanged, QString src)
{
/*
    Starts reading metadata and icons from row, alternating ahead and behind.

    fileSelectionChanged == false (Scroll event):

        Read metadata and icons.  Do not trigger a fileSelectioChange.

    fileSelectionChanged == true:

        The row is the current image and MW::fileSelectionChange will be signalled after
        a delay of imageCacheTriggerCount images.

    Invoked by MW::loadCurrent.
*/
    if (G::isLogger || G::isFlowLogger) G::log("MetaRead::setCurrentRow", "row = " + QString::number(row));
    if (isDebug)
    {
        qDebug() << "MetaRead::setCurrentRow"
                 << "row =" << row
                 << "fileSelectionChanged =" << fileSelectionChanged
                 << "isRunning =" << isRunning()
                 << "src =" << src
                 << "G::allMetadataLoaded =" << G::allMetadataLoaded
                 << "G::iconChunkLoaded =" << G::iconChunkLoaded
                    ;
    }

    // Nothing to read
    if (G::allMetadataLoaded && G::iconChunkLoaded) {
        if (fileSelectionChanged) {
            targetRow = row;
            triggerFileSelectionChange();
        }
        return;
    }

    t.restart();
    this->src = src;

    mutex.lock();
    //iconChunkSize = dm->iconChunkSize;
    sfRowCount = dm->sf->rowCount();
    if (row >= 0 && row < sfRowCount) startRow = row;
    else startRow = 0;

    sfRowCount = dm->sf->rowCount();
    lastRow = sfRowCount - 1;
    // icon range size in case changes ie thumb size change
    //iconChunkSize = dm->iconChunkSize;
    iconLimit = static_cast<int>(dm->iconChunkSize * 1.2);  // RGH not req'd
    // adjust icon range to startRow
    firstIconRow = startRow - dm->iconChunkSize / 2;
    if (firstIconRow < 0) firstIconRow = 0;
    lastIconRow = firstIconRow + dm->iconChunkSize - 1;
    if (lastIconRow > lastRow) lastIconRow = lastRow;
    /*
            qDebug() << "MetaRead::start"
                     << "startRow =" << startRow
                     << "firstIconRow =" << firstIconRow
                     << "lastIconRow =" << lastIconRow
                     << "iconChunkSize =" << dm->iconChunkSize
                ; //*/
    // housekeeping
    instance = dm->instance;
    abort = false;

    triggerCount = 0;
    targetRow = startRow;
    hasBeenTriggered = false;
    abortCleanup = isRunning();

    okToTrigger = fileSelectionChanged;

    mutex.unlock();

    if (!isRunning()) {
        start(QThread::HighestPriority);
    }
}

bool MetaRead::stop()
{
    if (isDebug) G::log("MetaRead::stop");

    abort = true;
    for (int i = 0; i < 100; i++) {
        G::wait(1);
        //qDebug() << "metaReadThread->isRunning =" << isRunning();
        if (!isRunning()) break;
    }
    abort = false;
    return isRunning();

    /*
    This conventional thread stop does not work, apparently because it deadlocks
    with the addToDatamodel signal connection which is Qt::BlockingQueuedConnection.

    if (isRunning()) {
        abort = true;
        mutex.lock();
        abort = true;
        condition.wakeOne();
        mutex.unlock();
        wait();
        if (!wait(5000)) {
            qDebug() << "** MetaRead::stopping wait() FAILED **   " << folderPath;
            return false;
        }
    }
    return true;
    */
}

void MetaRead::initialize()
/*
    Called when change folders.
*/
{
    if (isDebug || G::isLogger || G::isFlowLogger)
    {
        G::log("MetaRead::initialize",
               "imageCacheTriggerCount = " + QString::number(imageCacheTriggerCount));
    }
    rowsWithIcon.clear();
    dmRowCount = dm->rowCount();
    metaReadCount = 0;
    triggerCount = -1;                     // used to delay start ImageCache
    abort = false;
}

QString MetaRead::diagnostics()
{
    if (isDebug) G::log("MetaRead::diagnostics");

    QString reportString;
    QTextStream rpt;
    rpt.setString(&reportString);
    rpt << Utilities::centeredRptHdr('=', objectName() + " MetaRead Diagnostics");
    rpt << "\n" ;
    rpt << "\n" << "instance:               " << instance;
    rpt << "\n" << "abort:                  " << (abort ? "true" : "false");
    rpt << "\n" << "isRunning:              " << (isRunning() ? "true" : "false");
    rpt << "\n";
    rpt << "\n" << "imageCacheTriggerCount: " << imageCacheTriggerCount;
    rpt << "\n" << "expansionFactor:        " << expansionFactor;
    rpt << "\n";
    rpt << "\n" << "folder path:            " << dm->currentPrimaryFolderPath;
    rpt << "\n" << "sfRowCount:             " << sfRowCount;
    rpt << "\n" << "dm->currentSfRow:       " << dm->currentSfRow;
    rpt << "\n";
    rpt << "\n" << "defaultIconChunkSize:   " << dm->defaultIconChunkSize;
    rpt << "\n" << "iconChunkSize:          " << dm->iconChunkSize;
    //rpt << "\n" << "iconChunkSize:          " << iconChunkSize;
    rpt << "\n" << "iconLimit:              " << iconLimit;
    rpt << "\n" << "firstIconRow:           " << firstIconRow;
    rpt << "\n" << "lastIconRow:            " << lastIconRow;
    rpt << "\n" << "rowsWithIcon:           " << rowsWithIcon.size();
    rpt << "\n" << "dm->iconCount:          " << dm->iconCount();
    rpt << "\n";
    rpt << "\n";
    rpt << "rowsWithIcon in datamodel:";
    rpt.setFieldAlignment(QTextStream::AlignRight);
    rpt.setFieldWidth(9);
    for (int i = 0; i < dm->rowCount(); i++) {
        if (dm->itemFromIndex(dm->index(i,0))->icon().isNull()) continue;
        rpt << "\n" << i;
    }

    rpt << "\n\n" ;
    return reportString;
}

QString MetaRead::reportMetaCache()
{
    if (isDebug) G::log("MetaRead::reportMetaCache");
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

    For efficiency, the currently cached icons are tracked in the QList rowsWithIcon.
    Each item in rowsWithIcon is the dmRow in the datamodel.

    If a new startRow is sent to setStartPosition while running then the icon
    cleanup is interrupted.
*/
    if (isDebug) G::log("MetaRead::cleanupIcons");

    // check if datamodel size is less than assigned icon cache chunk size
    if (dm->iconChunkSize >= sfRowCount) return;

    int i = 0;
    while (rowsWithIcon.size() > dm->iconChunkSize) {
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
        emit setIcon(dmIdx, QPixmap(), instance, "MetaRead::cleanupIcons");
        rowsWithIcon.remove(i);
    }
}

bool MetaRead::readMetadata(QModelIndex sfIdx, QString fPath)
{
    if (G::isLogger) G::log("MetaRead::readMetadata", "row = " + QString::number(sfIdx.row()));

    if (isDebug)
    {
        qDebug() << "MetaRead::readMetadata"
                 << "instance =" << instance
                 << "dm->instance" << dm->instance
                 << "row =" << sfIdx.row()
                 << "abort =" << abort
                 << fPath;
    }
    if (instance != dm->instance) {
        QString msg = "Instance Clash.";
        G::issue("Warning", msg, "MetaRead::readMetadata", sfIdx.row(), fPath);        return false;
    }


    // read metadata from file into metadata->m

    int dmRow = dm->sf->mapToSource(sfIdx).row();
    QFileInfo fileInfo(fPath);
    bool isMetaLoaded = metadata->loadImageMetadata(fileInfo, instance, true, true, false, true, "MetaRead::readMetadata");
    if (abort) return false;
    if (!isMetaLoaded) {
        QString msg = "Metadata load failed.";
        G::issue("Warning", msg, "MetaRead::readMetadata", sfIdx.row(), fPath);
    }

    metadata->m.row = dmRow;
    metadata->m.instance = instance;

    // update progress in case filters panel activated before all metadata loaded
    metaReadCount++;
    int progress = 1.0 * metaReadCount / dmRowCount * 100;
    emit updateProgressInFilter(progress);
    if (showProgressInStatusbar) emit updateProgressInStatusbar(dmRow, dmRowCount);

    if (isDebug)
    {
        qDebug() << "MetaRead::readMetadata"
                 << "added row =" << sfIdx.row()
                 << "abort =" << abort
                 ;
    }

    if (abort) {
        if (isDebug)
        {
            qDebug() << "MetaRead::readMetadata aborted";
        }
        return false;
    }

    // add metadata->m to DataModel dm
    emit addToDatamodel(metadata->m, "MetaRead::readMetadata");
    if (isDebug)
    {
        qDebug() << "MetaRead::readMetadata"
                 << "addToDatamodel row =" << sfIdx.row()
                 << "abort =" << abort
                 ;
    }

    if (abort) {
        if (isDebug)
        {
            qDebug() << "MetaRead::readMetadata aborted";
        }
        return false;
    }

    if (isDebug)
    {
        qDebug() << "MetaRead::readMetadata"
                 << "addToImageCache row =" << sfIdx.row()
                 //<< "abort =" << abort
                 ;
    }
    return true;
}

void MetaRead::readIcon(QModelIndex sfIdx, QString fPath)
{
    if (G::isLogger) G::log("MetaRead::readIcon");
    if (isDebug)
    {
        qDebug().noquote() << "MetaRead::readIcon"
                           << "start  row =" << sfIdx.row()
                              ;
    }

    // dm->sf->mapToSource(sfIdx) EXC_BAD_ACCESS crash when rapid folder changes
    int dmRow = dm->rowFromPath(fPath);
    QModelIndex dmIdx = dm->index(dmRow, 0);

    bool isVideo = false;
    isVideo = dm->index(dmRow, G::VideoColumn).data().toBool();

    if (isDebug)
    {
        qDebug().noquote() << "MetaRead::readIcon"
                           << "load   row =" << sfIdx.row()
                              ;
    }
    // get thumbnail
    QImage image;
    bool thumbLoaded = false;
    thumbLoaded = thumb->loadThumb(fPath, image, instance, "MetaRead::readIcon");
    if (abort) return;
    if (isDebug)
    {
        qDebug().noquote() << "MetaRead::readIcon"
                           << "loaded row =" << sfIdx.row()
                              ;
    }

    if (isVideo) {
        rowsWithIcon.append(dmRow);
        if (G::renderVideoThumb) return;
    }

    QPixmap pm;
    if (thumbLoaded && !image.isNull() && !abort) {
        pm = QPixmap::fromImage(image.scaled(G::maxIconSize, G::maxIconSize, Qt::KeepAspectRatio));
    }
    else {
        pm = QPixmap(":/images/error_image256.png");
        QString msg = "Failed to load thumbnail.";
        G::issue("Warning", msg, "MetaRead::readIcon", dmRow, fPath);
    }
    if (abort) return;
    emit setIcon(dmIdx, pm, instance, "MetaRead::readIcon");
    rowsWithIcon.append(dmRow);

    if (isDebug)
    {
        qDebug().noquote() << "MetaRead::readIcon"
                           << "done   row =" << sfIdx.row()
                              ;
    }
}

void MetaRead::readRow(int sfRow)
{
    if (G::isLogger) G::log("MetaRead::readRow", "row = " + QString::number(sfRow));
    if (isDebug)
    {
        qDebug().noquote() << "MetaRead::readRow"
                           << "start  row =" << sfRow
                           << "dm->sf->rowCount() =" << dm->sf->rowCount()
                              ;
    }

    // range check
    if (sfRow >= dm->sf->rowCount()) {
       return;
    }
    // valid index check
    QModelIndex sfIdx = dm->sf->index(sfRow, 0);
    if (!sfIdx.isValid()) {
        QString msg = "Invalid sfIdx.";
        G::issue("Warning", msg, "MetaRead::readRow", sfRow);
        return;
    }

    // load metadata
    QString fPath = sfIdx.data(G::PathRole).toString();
    if (!G::allMetadataLoaded /*&& !isVideo*/ && G::useReadMetadata) {
        bool metaLoaded = dm->sf->index(sfRow, G::MetadataLoadedColumn).data().toBool();
        if (!metaLoaded) {
            if (!readMetadata(sfIdx, fPath)) {
            }
        }
    }
    if (abort) return;

    // load icon

    // can ignore if debugging
    if (!G::useReadIcons) return;

    // read icon if in icon range
    if (sfRow >= firstIconRow && sfRow <= lastIconRow) {
        if (!dm->iconLoaded(sfRow, instance)) {
            readIcon(sfIdx, fPath);
            if (abort) return;
            if (rowsWithIcon.size() > iconLimit) {
                cleanupIcons();
            }
        }
    }

    // add to ImageCache icd->cacheItemList (used to manage image cache)
    if (G::useImageCache) {
        emit addToImageCache(metadata->m, instance);
    }

    if (isDebug)
    {
        qDebug().noquote() << "MetaRead::readRow"
                           << "done   row =" << sfRow
                              ;
    }
}

void MetaRead::resetTrigger()
{
    hasBeenTriggered = false;
}

void MetaRead::triggerFileSelectionChange()
{
    // file selection change and start image caching thread after head start
    if (G::isLogger || G::isFlowLogger) G::log("MetaRead::triggerFileSelectionChange", "signal fileSelectionChange");
    //qDebug() << "MetaRead::triggerFileSelectionChange  targetRow =" << targetRow;
    QModelIndex sfIdx = dm->sf->index(targetRow, 0);
    emit fileSelectionChange(sfIdx);
    hasBeenTriggered = true;
}

void MetaRead::triggerCheck()
/*
    Signal MW::fileSelectionChange to trigger the ImageCache to rebuild.
*/
{
    if (hasBeenTriggered) return;
    if (G::allMetadataLoaded) {
        triggerFileSelectionChange();
        return;
    }
    triggerCount++;
    if (triggerCount == lastRow || triggerCount == imageCacheTriggerCount) {
            triggerFileSelectionChange();
    }
}

void MetaRead::run()
{
/*
    Loads the metadata and icons into the datamodel (dm) for a folder.  The iteration
    proceeds from the start row in an ahead/behind progression.
*/
    if (isDebug) G::log("MetaRead::run", src);
    if (isDebug)
    {
        qDebug().noquote() << "MetaRead::run"
                           << "src =" << src
                           << "startRow =" << startRow
                           << "sfRowCount =" << sfRowCount
                              ;
    }

    if (G::useUpdateStatus) emit runStatus(true, true, "MetaRead::run");

    int a = startRow;       // forward counter
    int b = startRow - 1;   // backward counter

    while (a < sfRowCount || b >= 0) {

        if (a < sfRowCount) {
            if (!abort) readRow(a);
            a++;
            if (!abort && okToTrigger) triggerCheck();
        }

        if (b >= 0) {
            if (!abort) readRow(b);
            b--;
            if (!abort && okToTrigger) triggerCheck();
        }

        // terminate loop
        if (G::allMetadataLoaded && (b < firstIconRow && a > lastIconRow)) break;

        if (abort) {
            if (isDebug)
                qDebug() << "MetaRead::run  abort = true  exit run";
            return;
        }
    }

    /* When images are inserted into the current folder (ie export from lightroom to a
    Winnet - see MW::handleStartArgs), fileSelectionChanged may not be triggered. */
    if (!hasBeenTriggered && fileSelectionChanged) {
        QModelIndex sfIdx = dm->sf->index(startRow, 0);
        emit fileSelectionChange(sfIdx);
    }

    if (instance != dm->instance) return;
    if (!abort) if (G::useUpdateStatus) emit runStatus(false, true, "MetaRead::run");
    if (!abort) G::allMetadataLoaded = true;
    if (!abort) cleanupIcons();
    if (!abort) emit done();
    if (isDebug) qDebug() << "MetaRead::run  Done.";

    abort = false;

    return;
}
