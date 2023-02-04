#include "metaread.h"

/*
    MetaRead, runing in a separate thread, loads the metadata and icons into the
    datamodel (dm) for a folder. All the metadata will be loaded, and icons will be
    loaded up to either the iconChunkSize or visibleIcons, depending on the preferences.

    MetaRead::setCurrentRow is called when:

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

MetaRead::MetaRead(QObject *parent,
                   DataModel *dm,
                   Metadata *metadata,
                   FrameDecoder *frameDecoder)
    : QThread(parent)
{
    if (G::isLogger) G::log("MetaRead::MetaRead");
    this->dm = dm;
    this->metadata = metadata;
    this->frameDecoder = frameDecoder;
    thumb = new Thumb(dm, metadata, frameDecoder);
    instance = 0;
    abort = false;
    debugCaching = false;
}

MetaRead::~MetaRead()
{
    if (debugCaching) qDebug() << "MetaRead::~MetaRead";
    delete thumb;
}

void MetaRead::setCurrentRow(int row, QString src)
{
    if (G::isLogger || G::isFlowLogger) G::log("MetaRead::start");
    if (debugCaching)
    {
        qDebug() << "MetaRead::setCurrentRow"
                 << "src =" << src
                 << "row =" << row
                 << "isrunning" << isRunning();
    }

    this->src = src;
    mutex.lock();
    iconChunkSize = dm->iconChunkSize;
    if (row >= 0 && row < dm->sf->rowCount()) startRow = row;
    else startRow = 0;
    changeStartRowWhileRunning = isRunning();
    mutex.unlock();
    if (!isRunning()) {
        start();
    }
}

void MetaRead::stop()
{
    if (G::isLogger || G::isFlowLogger) G::log("MetaRead::stop");
    if (debugCaching)
    qDebug() << "MetaRead::stop"
             << "src =" << src
             << "startRow =" << startRow
             << "abort =" << abort
                ;
    static int count = 0;

    if (isRunning()) {
        mutex.lock();
        abort = true;
//        qDebug() << "MetaRead::stop   Set abort = true";
        condition.wakeOne();
        mutex.unlock();
//        qDebug() << "MetaRead::stop   starting wait()  isRunning() =" << isRunning()
//                 << "count =" << count;
//        wait();
        abort = false;
//        qDebug() << "MetaRead::stop   STOPPED" << count++;
    }
    // signal MW all stopped if a folder change
//    if (G::stop) emit stopped("MetaRead");
    return;
}

int MetaRead::interrupt()
{
    // is this being used?

    if (G::isLogger || G::isFlowLogger) G::log("MetaRead::pause");
    G::stop = false;
    qDebug() << "MetaRead::interrupt";
    mutex.lock();
    interrupted = true;
    mutex.unlock();
    stop();
//    qDebug() << "MetaRead::interrupt after stop  interruptRow =" << interruptedRow;
    return interruptedRow;
}

void MetaRead::initialize()
{
    if (G::isLogger || G::isFlowLogger) G::log("MetaRead::initialize");
    rowsWithIcon.clear();
    imageCachingStarted = false;
    instance = dm->instance;
    dmRowCount = dm->rowCount();
    metaReadCount = 0;
}

QString MetaRead::diagnostics()
{
    if (G::isLogger) G::log("MetaRead::diagnostics");

    QString reportString;
    QTextStream rpt;
    rpt.setString(&reportString);
    rpt << Utilities::centeredRptHdr('=', objectName() + " MetaRead Diagnostics");
    rpt << "\n" ;
    rpt << "\n" << "Load algorithm:        " << (G::isLinearLoading == true ? "Linear" : "Concurrent");
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
    rpt << "\n";
    rpt << "rowsWithIcon in datamodel:";
    rpt.setFieldAlignment(QTextStream::AlignRight);
    rpt.setFieldWidth(9);
    for (int i = 0; i < dm->rowCount(); i++) {
//        if (dm->index(i,0).data(Qt::DecorationRole).isNull()) continue;
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

QString MetaRead::reportMetaCache()
{
    if (G::isLogger) G::log("MetaRead::reportMetaCache");
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
    if (G::isLogger) G::log("MetaRead::cleanupIcons");

    // check if datamodel size is less than assigned icon cache chunk size
    if (iconChunkSize >= sfRowCount) return;

    int i = 0;
    while (rowsWithIcon.size() > iconChunkSize) {
        if (i >= rowsWithIcon.size()) break;
        mutex.lock();
        bool abortCleanup = changeStartRowWhileRunning;
        mutex.unlock();
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
    if (G::isLogger) G::log("MetaRead::readMetadata");
    if (debugCaching)
    {
        qDebug() << "MetaRead::readMetadata"
                 << "instance =" << instance
                 << "dm->instance" << dm->instance
                 << "row =" << sfIdx.row()
                 << "abort =" << abort
                 << fPath;
    }
    if (instance != dm->instance) {
        qWarning() << "WARNING MetaRead::readMetadata Instance Clash"
                   << "row =" << sfIdx.row();
        abort = true;
        return false;
    }
    int dmRow = dm->sf->mapToSource(sfIdx).row();
    QFileInfo fileInfo(fPath);
    metadata->loadImageMetadata(fileInfo, instance, true, true, false, true, "MetaRead::readMetadata");
    metadata->m.row = dmRow;
    metadata->m.instance = instance;
    metaReadCount++;
    int progress = 1.0 * metaReadCount / dmRowCount * 100;
    emit updateProgress(progress);

    if (debugCaching)
    {
        qDebug() << "MetaRead::readMetadata"
                 << "added row =" << sfIdx.row()
                 << "abort =" << abort
                 ;
    }
    emit addToDatamodel(metadata->m, "MetaRead::readMetadata");
    if (debugCaching)
    {
        qDebug() << "MetaRead::readMetadata"
                 << "addToDatamodel row =" << sfIdx.row()
                 << "abort =" << abort
                 ;
    }
    if (G::useImageCache) {
        emit addToImageCache(metadata->m);
    }
    if (debugCaching) {
        qDebug() << "MetaRead::readMetadata"
                 << "addToImageCache row =" << sfIdx.row()
                 << "abort =" << abort
                 ;
    }
    return true;
}

void MetaRead::readIcon(QModelIndex sfIdx, QString fPath)
{
    if (G::isLogger) G::log("MetaRead::readIcon");
    if (debugCaching) {
        qDebug().noquote() << "MetaRead::readIcon"
                           << "start  row =" << sfIdx.row()
                              ;
    }
    if (!sfIdx.isValid()) {
        qWarning() << "WARNING" << "MetaRead::readIcon"
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
    thumbLoaded = thumb->loadThumb(fPath, image, instance, "MetaRead::readIcon");
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
    emit setIcon(dmIdx, pm, instance, "MetaRead::readIcon");
    rowsWithIcon.append(dmRow);

    if (debugCaching)
    {
        qDebug().noquote() << "MetaRead::readIcon"
                           << "done   row =" << sfIdx.row()
                              ;
    }
}

void MetaRead::readRow(int sfRow)
{
    if (G::isLogger) G::log("MetaRead::readRow");
    if (debugCaching)
    {
        qDebug().noquote() << "MetaRead::readRow"
                           << "start  row =" << sfRow
                           << "dm->sf->rowCount()=" << dm->sf->rowCount()
                              ;
    }

    // range check
    if (sfRow >= dm->sf->rowCount()) return;
    // valid index check
    QModelIndex sfIdx = dm->sf->index(sfRow, 0);
    if (!sfIdx.isValid()) {
        if (debugCaching) {
            qDebug().noquote() << "MetaRead::readRow  "
                               << "invalid sfidx =" << sfIdx
                                  ;
        }
        return;
    }

    // load metadata
    QString fPath = sfIdx.data(G::PathRole).toString();
    if (!G::allMetadataLoaded && G::useReadMetadata) {
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

    if (debugCaching)
    {
        qDebug().noquote() << "MetaRead::readRow"
                           << "done   row =" << sfRow
                              ;
    }
}

void MetaRead::run()
{
/*
    Loads the metadata and icons into the datamodel (dm) for a folder.  The iteration
    proceeds from the start row in an ahead/behind progression.
*/
    if (G::isLogger || G::isFlowLogger) G::log("MetaRead::read", src);

    folderPath = dm->currentFolderPath;
//    iconLimit = iconChunkSize * 1.2;
    sfRowCount = dm->sf->rowCount();

    if (debugCaching)
    {
        qDebug().noquote() << "MetaRead::run"
                           << "src =" << src
                           << "startRow =" << startRow
                           << "sfRowCount =" << sfRowCount
                              ;
    }
    if (startRow >= sfRowCount) return;
//    int row = startRow;
//    startRow = -1;

    if (G::useUpdateStatus) emit runStatus(true, true, "MetaRead::run");

    // range of datamodel rows to load icons
//    firstIconRow = startRow - iconChunkSize / 2;
//    if (firstIconRow < 0) firstIconRow = 0;
//    lastIconRow = firstIconRow + iconChunkSize;
//    qDebug() << "MetaRead::run" << firstIconRow << lastIconRow
//             << rowsWithIcon.size() << folderPath;

//    if (rowsWithIcon.size() > iconChunkSize) {
////        cleanupIcons();
//    }

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
            qDebug() << "MetaRead::run ** ABORT ** Returning out of loop at row" << row;
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
            // icon range
            iconChunkSize = dm->iconChunkSize;              // in case changes ie thumb size change
            iconLimit = static_cast<int>(iconChunkSize * 1.2);
            firstIconRow = startRow - iconChunkSize / 2;
            if (firstIconRow < 0) firstIconRow = 0;
            lastIconRow = firstIconRow + iconChunkSize - 1;
            if (lastIconRow > lastRow) lastIconRow = lastRow;
            if (rowsWithIcon.size() > iconChunkSize) {
                cleanupIcons();
            }
//            qDebug() << "MetaRead::run" << "row =" << row;
            startRow = -1;
        }

//        // do something with row
//        qDebug() << "MetaRead::run" << "row =" << row;

        readRow(row);
//        if (G::isFileLogger) Utilities::log("MetaRead::run", "row = " + QString::number(row));
//        if (abort) return;

        // delayed start ImageCache
        if (!G::allMetadataLoaded && !imageCachingStarted && !abort) {
            count++;
            if (count == sfRowCount - 1 || count == imageCacheTriggerCount) {
                // start image caching thread after head start
                emit triggerImageCache("Initial");
                imageCachingStarted = true;
//                if (G::isFileLogger) Utilities::log("MetaRead::run", "start image caching");
            }
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
    } // end loop processing rows

    if (debugCaching) qDebug() << "MetaRead::run  Completed loop";    // finished or aborted

    if (G::useUpdateStatus) emit runStatus(false, true, "MetaRead::run");

    G::allMetadataLoaded = true;
    cleanupIcons();

    emit updateIconBestFit();
    // refresh image cache in case not up-to-date (usually issue is target range)
    if (sfRowCount > imageCacheTriggerCount) {
//        emit triggerImageCache("Final");
    }
    emit done();

    if (debugCaching) qDebug() << "MetaRead::run  Done.";
}
