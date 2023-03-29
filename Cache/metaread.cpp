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
        • Set the start position in the DataModel.
        • Start iterating the DataModel Proxy rows, starting with the current row and
          alternating one ahead and one behind, loading the metadata and icons.
        • Part way through the iteration start the ImageCache.
        • Cleanup (remove) icons exceeding amount set in preferences.  If loadOnlyVisibleIcons
          then remove all icons except those visible in either thumbView or gridView.  Other-
          wise, remove icons exceeding iconChunkSize based on the priorityQueue.
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
    if (isDebug || G::isLogger) G::log("MetaRead::MetaRead");
    this->dm = dm;
    this->metadata = metadata;
    this->frameDecoder = frameDecoder;
    thumb = new Thumb(dm, metadata, frameDecoder);
    instance = 0;
    isDebug = true;
}

MetaRead::~MetaRead()
{
    if (isDebug) qDebug() << "MetaRead::~MetaRead";
    delete thumb;
}

void MetaRead::setCurrentRow(int row, bool scrollOnly, QString src)
{
    if (isDebug || G::isLogger || G::isFlowLogger) {
        QString running;
        isRunning() ? running = "true" : running = "false";
        QString s = "row = " + QString::number(row) + " src = " + src + " isRunning = " + running;
        G::log("MetaRead::setCurrentRow", s);
    }
    //qDebug() << "MetaRead::setCurrentRow row =" << row << "src =" << src;
    this->src = src;

    mutex.lock();
    iconChunkSize = dm->iconChunkSize;
    if (row >= 0 && row < dm->sf->rowCount()) startRow = row;
    else startRow = 0;
    targetRow = startRow;
    startPath = dm->sf->index(startRow, 0).data(G::PathRole).toString();
    this->scrollOnly = scrollOnly;
    count = 0;
    abortCleanup = isRunning();
    mutex.unlock();
    /*
    qDebug() << "MetaRead::setCurrentRow targetRow =" << targetRow
             << "isRunning =" << isRunning()
             << "startPath =" << startPath;
             //*/

    if (!isRunning()) {
        start();
    }
}

bool MetaRead::stop()
{
    if (isDebug || G::isLogger || G::isFlowLogger) G::log("MetaRead::stop");
    if (isDebug)
    {
    qDebug() << "MetaRead::stop"
             << "startRow =" << startRow
             << "debugRow =" << debugRow
             << "instance =" << instance
             << "dmInstance =" << dm->instance
                ;
    }

//    qDebug() << "MetaRead::about to stop   isRunning =" << isRunning();
//    abort = true;
    if (isRunning()) {
        mutex.lock();
        abort = true;
//        qDebug() << "MetaRead::stopping set abort:   abort =" << abort;
        condition.wakeOne();
        mutex.unlock();
        qDebug() << "MetaRead::stopping before wait()   isRunning =" << isRunning();
        if (!wait(1000)) {
            qDebug() << "MetaRead::stopping wait() FAILED";
            return false;
        }
    }
    abort = false;
    // signal MW all stopped if a folder change
    //if (G::stop) emit stopped("MetaRead");
    //qDebug() << "MetaRead::stopped  isRunning =" << isRunning();
    return true;
}

int MetaRead::interrupt()
{
    // is this being used?

    if (isDebug || G::isLogger || G::isFlowLogger) G::log("MetaRead::pause");
//    G::stop = false;
    qDebug() << "MetaRead::interrupt";
    mutex.lock();
    interrupted = true;
    mutex.unlock();
    stop();
//    qDebug() << "MetaRead::interrupt after stop  interruptRow =" << interruptedRow;
    return interruptedRow;
}

void MetaRead::initialize()
/*
    Called when change folders.
*/
{
    if (isDebug || G::isLogger) G::log("MetaRead::initialize");
    imageCacheTriggerCount =  QThread::idealThreadCount();
    if (isDebug || G::isLogger || G::isFlowLogger)
        G::log("MetaRead::initialize",
               "imageCacheTriggerCount = " + QString::number(imageCacheTriggerCount));
    rowsWithIcon.clear();
    imageCachingStarted = false;
    instance = dm->instance;
    dmRowCount = dm->rowCount();
    metaReadCount = 0;
//    qDebug() << "MetaRead::initialize  isRunning =" << isRunning()
//             << "Folder = " << dm->currentFolderPath;
}

QString MetaRead::diagnostics()
{
    if (isDebug || G::isLogger) G::log("MetaRead::diagnostics");

    QString reportString;
    QTextStream rpt;
    rpt.setString(&reportString);
    rpt << Utilities::centeredRptHdr('=', objectName() + " MetaRead Diagnostics");
    rpt << "\n" ;
    rpt << "\n" << "Load algorithm:         " << (G::isLoadLinear == true ? "Linear" : "Concurrent");
    rpt << "\n" << "instance:               " << instance;
    rpt << "\n" << "abort:                  " << (abort ? "true" : "false");
    rpt << "\n" << "isRunning:              " << (isRunning() ? "true" : "false");
    rpt << "\n";
    rpt << "\n" << "imageCacheTriggerCount: " << imageCacheTriggerCount;
    rpt << "\n" << "expansionFactor:        " << expansionFactor;
    rpt << "\n";
    rpt << "\n" << "folder path:            " << folderPath;
    rpt << "\n" << "sfRowCount:             " << sfRowCount;
    rpt << "\n" << "dm->currentSfRow:       " << dm->currentSfRow;
    rpt << "\n";
    rpt << "\n" << "defaultIconChunkSize:   " << dm->defaultIconChunkSize;
    rpt << "\n" << "iconChunkSize:          " << iconChunkSize;
    rpt << "\n" << "iconLimit:              " << iconLimit;
    rpt << "\n" << "firstIconRow:           " << firstIconRow;
    rpt << "\n" << "lastIconRow:            " << lastIconRow;
    rpt << "\n" << "rowsWithIcon:           " << rowsWithIcon.size();
    rpt << "\n" << "dm->iconCount:          " << dm->iconCount();
    rpt << "\n";
//    rpt << "rowsWithIcon in datamodel:";
//    rpt.setFieldAlignment(QTextStream::AlignRight);
//    rpt.setFieldWidth(9);
//    for (int i = 0; i < dm->rowCount(); i++) {
//        if (dm->itemFromIndex(dm->index(i,0))->icon().isNull()) continue;
//        rpt << "\n" << i;
//    }

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
    For efficiency, the currently cached icons are tracked in the QList rowsWithIcon.
    Each item in rowsWithIcon is the dmRow in the datamodel.
    If a new startRow is sent to setStartPosition while running then the icon
    cleanup is interrupted.
*/
    if (isDebug || G::isLogger) G::log("MetaRead::cleanupIcons");

    // check if datamodel size is less than assigned icon cache chunk size
    if (iconChunkSize >= sfRowCount) return;

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
        emit setIcon(dmIdx, QPixmap(), instance, "MetaRead::cleanupIcons");
        rowsWithIcon.remove(i);
    }
}

bool MetaRead::readMetadata(QModelIndex sfIdx, QString fPath)
{
    if (isDebug || G::isLogger) G::log("MetaRead::readMetadata");
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
        qWarning() << "WARNING MetaRead::readMetadata Instance Clash"
                   << "row =" << sfIdx.row();
        abort = true;
        return false;
    }


    // read metadata from file into metadata->m
    int dmRow = dm->sf->mapToSource(sfIdx).row();
    QFileInfo fileInfo(fPath);
    bool isMetaLoaded = metadata->loadImageMetadata(fileInfo, instance, true, true, false, true, "MetaRead::readMetadata");
    if (!isMetaLoaded) {
        qDebug() << "MetaRead::readMetadata"
                 << "row =" << sfIdx.row()
                 << "Load meta failed"
                 ;
    }

    metadata->m.row = dmRow;
    metadata->m.instance = instance;

    // update progress in case filters panel activated before all metadata loaded
    metaReadCount++;
    int progress = 1.0 * metaReadCount / dmRowCount * 100;
    emit updateProgress(progress);

    if (isDebug)
    {
        qDebug() << "MetaRead::readMetadata"
                 << "added row =" << sfIdx.row()
                 << "abort =" << abort
                 ;
    }
    /*
    qDebug() << "MetaRead::readMetadata"
             << "row =" << sfIdx.row()
             << "metadata->m.type =" << metadata->m.type
             << "metadata->m.offsetThumb =" << metadata->m.offsetThumb
             << "metadata->m.lengthThumb =" << metadata->m.lengthThumb
                ; //*/

    // add metadata->m to DataModel dm
    emit addToDatamodel(metadata->m, "MetaRead::readMetadata");
    if (isDebug)
    {
        qDebug() << "MetaRead::readMetadata"
                 << "addToDatamodel row =" << sfIdx.row()
                 << "abort =" << abort
                 ;
    }

    // add to ImageCache icd->cacheItemList (used to manage image cache)
    if (G::useImageCache) {
        emit addToImageCache(metadata->m);
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
    if (isDebug || G::isLogger) G::log("MetaRead::readIcon");
    if (isDebug) {
        qDebug().noquote() << "MetaRead::readIcon"
                           << "start  row =" << sfIdx.row()
                              ;
    }
    if (!sfIdx.isValid()) {
        if (G::isWarningLogger)
        qWarning() << "WARNING" << "MetaRead::readIcon"
                   << "sfIdx.isValid() =" << sfIdx.isValid() << sfIdx;
        return;
    }

    int dmRow = dm->rowFromPath(fPath);  // dm->sf->mapToSource(sfIdx) EXC_BAD_ACCESS crash when rapid folder changes
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
    if (isDebug)
    {
        qDebug().noquote() << "MetaRead::readIcon"
                           << "loaded row =" << sfIdx.row()
                              ;
    }
    if (isVideo && !abort) {
        rowsWithIcon.append(dmRow);
        return;
    }
    if (abort) return;
    QPixmap pm;
    if (thumbLoaded && !abort) {
        pm = QPixmap::fromImage(image.scaled(G::maxIconSize, G::maxIconSize, Qt::KeepAspectRatio));
    }
    else {
        pm = QPixmap(":/images/error_image256.png");
        if (G::isWarningLogger)
        qWarning() << "WARNING" << "MetadataCache::loadIcon" << "Failed to load thumbnail." << fPath;
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
    if (isDebug || G::isLogger) G::log("MetaRead::readRow");
    if (isDebug)
    {
        qDebug().noquote() << "MetaRead::readRow"
                           << "start  row =" << sfRow
                           << "dm->sf->rowCount() =" << dm->sf->rowCount()
                              ;
    }

    if (abort) return;

    // IconView scroll signal can be delayed
    emit updateScroll();

    // range check
    if (sfRow >= dm->sf->rowCount()) {
        if (G::isWarningLogger)
        qWarning() << "WARNING MetaRead::readRow"
                   << "abort =" << abort
//                   << "instance =" << instance << "dm->instance =" << dm->instance
                   << "dm->sf->rowCount() =" << dm->sf->rowCount()
                   << "row =" << sfRow << "FAILED RANGE CHECK"
                      ;
        return;
    }
    // valid index check
    QModelIndex sfIdx = dm->sf->index(sfRow, 0);
    if (!sfIdx.isValid()) {
        if (G::isWarningLogger)
        qWarning().noquote() << "WARNING MetaRead::readRow  "
                             << "invalid sfidx =" << sfIdx
                                 ;
        return;
    }
//    bool isVideo = dm->sf->index(sfRow, G::VideoColumn).data().toBool();

    // load metadata
    QString fPath = sfIdx.data(G::PathRole).toString();
    if (!G::allMetadataLoaded /*&& !isVideo*/ && G::useReadMetadata) {
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
    if (abort) return;

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

    if (isDebug)
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
    if (isDebug || G::isLogger || G::isFlowLogger) G::log("MetaRead::run", src);

    folderPath = dm->currentFolderPath;
    sfRowCount = dm->sf->rowCount();

    if (isDebug)
    {
        qDebug().noquote() << "MetaRead::run"
                           << "src =" << src
                           << "startRow =" << startRow
                           << "sfRowCount =" << sfRowCount
                              ;
    }
    if (startRow >= sfRowCount) return;

    if (G::useUpdateStatus) emit runStatus(true, true, "MetaRead::run");

    int row = startRow;
    count = -1;                     // used to delay start ImageCache
    bool ahead = true;
    int lastRow = sfRowCount - 1;
    bool moreAhead = row < lastRow;
    bool moreBehind = row >= 0;
    int rowAhead = row;
    int rowBehind = row;

    while (moreAhead || moreBehind) {

        // check if start row has changed while iterating
        if (startRow != -1) {
            row = startRow;
            moreAhead = row < lastRow;
            moreBehind = row > 0;
            rowAhead = row;
            rowBehind = row;
            moreAhead ? ahead = true : ahead = false;

            // icon range in case changes ie thumb size change
            iconChunkSize = dm->iconChunkSize;
            iconLimit = static_cast<int>(iconChunkSize * 1.2);
            firstIconRow = startRow - iconChunkSize / 2;
            if (firstIconRow < 0) firstIconRow = 0;
            lastIconRow = firstIconRow + iconChunkSize - 1;
            if (lastIconRow > lastRow) lastIconRow = lastRow;
            // qDebug() << "MetaRead::run" << "row =" << row;
            abortCleanup = false;
            startRow = -1;
        }

        // abort?
        if (row >= dm->sf->rowCount()) {
            qDebug() << "MetaRead::run ** ABORT ** row" << row
                     << ">= dm->sf->rowCount()" << dm->sf->rowCount();
            abort = true;
        }
//        else {
//            QModelIndex idx = dm->sf->index(row, G::PathColumn);
//            if (!idx.isValid()) continue;
//            QString fPath = idx.data(G::PathRole).toString();
//            QString thisFolder = fPath.first(fPath.lastIndexOf("/"));
//            //qDebug() << "MetaRead::run fPath =" << fPath << thisFolder;
//            if (thisFolder != dm->currentFolderPath) {
//                qDebug() << "MetaRead::run ** ABORT ** thisFolder" << thisFolder
//                         << "!=" << dm->currentFolderPath;
//                abort = true;
//            }
//        }
        if (abort) {
            qDebug() << "MetaRead::run ** ABORT ** Returning out of loop at row" << row;
            break;
        }

        // do something with row
        qDebug() << "MetaRead::run row =" << row;
        debugRow = row;
        readRow(row);

        // delayed start ImageCache
        if (!scrollOnly && !abort) {
            count++;
            if (count == lastRow || count == imageCacheTriggerCount) {
                // start image caching thread after head start
                if (isDebug || G::isLogger || G::isFlowLogger)
                    G::log("MetaRead::run", "emit fileSelectionChange " + startPath);
                QModelIndex current = dm->sf->index(targetRow, 0);
                emit fileSelectionChange(current, QModelIndex(), true, "MetaRead::run");
                // bm1
//                emit triggerImageCache(startPath, "MetaRead::run imageCacheTriggerCount");
                imageCachingStarted = true;
                //if (G::isFileLogger) Utilities::log("MetaRead::run", "start image caching");
            }
        }

        // next row to process
        if (ahead) {
            if (moreBehind) ahead = false;
            if (moreAhead) {
                row = ++rowAhead;
                moreAhead = rowAhead < sfRowCount;
            }
        }
        else {
            if (moreAhead) ahead = true;
            if (moreBehind) {
                row = --rowBehind;
                moreBehind = row > -1;
            }
        }
    } // end loop processing rows

    if (isDebug)
        qDebug() << "MetaRead::run  Completed loop";    // finished or aborted

    if (!abort)if (G::useUpdateStatus) emit runStatus(false, true, "MetaRead::run");
    if (!abort)G::allMetadataLoaded = true;
    if (!abort)cleanupIcons();
    if (!abort)emit done();
    if (isDebug) qDebug() << "MetaRead::run  Done.";

    abort = false;
}
