#include "metaread.h"

/*
    MetaRead loads the metadata and icons into the datamodel (dm) for a folder.  All the
    metadata will be loaded, and icons will be loaded up to either the iconChunkSize or
    visibleIcons, depending on the preferences.

    MetaRead::read is called when:

        • a new folder is selected
        • a new image is selected
        • thumbnails are scrolled
        • the gridView or thumbView are resized

    Steps:

        • Build the priority queue.  The queue lists the dm->sf rows, starting with the
          current row and alternating one ahead and one behind.
        • Cleanup (remove) icons exceeding preferences.  If loadOnlyVisibleIcons then
          remove all icons except those visible in either thumbView or gridView.  Other-
          wise, remove icons exceeding iconChunkSize based on the priorityQueue.
        • Iterate through the priorityQueue, loading the metadata and icons.



    to do:
        iconChunkSize
*/

MetaRead::MetaRead(QObject *parent, DataModel *dm, Metadata *metadata)
{
    if (G::isLogger) G::log("MetaRead::MetaRead");
    this->dm = dm;
    this->metadata = metadata;
//    this->metadata = new Metadata;
    thumb = new Thumb(dm, metadata);
    abort = false;
    debugCaching = false;
}

MetaRead::~MetaRead()
{
    delete thumb;
}

void MetaRead::start(int row, QString src)
{
//    qDebug() << "MetaRead::start" << row << isRunning;
    if (isRunning) {
        mutex.lock();
        abort = true;
        newStartRow = row;
        mutex.unlock();
    }
    else {
        read(row);
    }
}

void MetaRead::stop()
{
    if (G::isLogger || G::isFlowLogger) G::log("MetaRead::stop");
    mutex.lock();
    if (isRunning) {
        abort = true;
    }
    mutex.unlock();
}

void MetaRead::initialize()
{
    if (G::isLogger || G::isFlowLogger) G::log("MetaRead::initialize");
    rowsWithIcon.clear();
    visibleIcons.clear();
    imageCachingStarted = false;
}

QString MetaRead::diagnostics()
{
    if (G::isLogger) G::log("MetaRead::diagnostics");

    QString reportString;
    QTextStream rpt;
    rpt.setString(&reportString);
    rpt << Utilities::centeredRptHdr('=', objectName() + " MetaRead Diagnostics");
    rpt << "\n" ;
    rpt << "\n" << "Load algorithm:     " << (G::useLinearLoading == true ? "Linear" : "Concurrent");
    rpt << "\n" << "dmInstance:         " << dmInstance;
    rpt << "\n" << "sfRowCount:         " << sfRowCount;
    rpt << "\n" << "visibleIconCount:   " << visibleIconCount;
    rpt << "\n" << "iconChunkSize:      " << iconChunkSize;
    rpt << "\n" << "firstIconRow:       " << firstIconRow;
    rpt << "\n" << "lastIconRow:        " << lastIconRow;
    rpt << "\n" << "rowsWithIcon:       " << rowsWithIcon.size();
    rpt << "\n" << "dm->iconCount:      " << dm->iconCount();
    rpt << "\n" ;
    rpt << "\n" << "abort:              " << (abort ? "true" : "false");
    rpt << "\n" << "isRunning:          " << (isRunning ? "true" : "false");
    rpt << "\n" ;
    rpt << "rowsWithIcon:";
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

void MetaRead::iconMax(QPixmap &thumb)
{
    if (G::isLogger) G::log("MetaRead::iconMax");
    if (G::iconWMax == G::maxIconSize && G::iconHMax == G::maxIconSize) return;

    // for best aspect calc
    int w = thumb.width();
    int h = thumb.height();
    if (w > G::iconWMax) G::iconWMax = w;
    if (h > G::iconHMax) G::iconHMax = h;
}

bool MetaRead::isNotLoaded(int sfRow)
{
    /* not used  */

    QModelIndex sfIdx = dm->sf->index(sfRow, G::MetadataLoadedColumn);
    if (!sfIdx.data().toBool()) return true;
    if (!dm->iconLoaded(sfRow)) return true;
    return false;
}

bool MetaRead::inIconRange(int row) {
    if (row >= firstIconRow && row <= lastIconRow) return true;
    else return false;
}

void MetaRead::dmRowRemoved(int dmRow)
{
    if (G::isLogger) G::log(CLASSFUNCTION);
    int idx = rowsWithIcon.indexOf(dmRow);
    rowsWithIcon.removeAt(idx);
}

void MetaRead::cleanupIcons()
{
/*
    Remove icons not in priority queue after iconChunkSize
*/
    if (G::isLogger) G::log("MetaRead::cleanupIcons");

    // check if datamodel size is less than assigned icon cache chunk size
//    if (G::loadOnlyVisibleIcons && visibleIconCount >= sfRowCount) return;
    if (iconChunkSize >= sfRowCount) return;

    int i = 0;
    while (rowsWithIcon.size() > iconChunkSize) {
        if (i >= rowsWithIcon.size()) break;
        int dmRow = rowsWithIcon.at(i);
        QModelIndex dmIdx = dm->index(dmRow, 0);
        int sfRow = dm->sf->mapFromSource(dmIdx).row();
        // if row in icon chunk range then skip
        if (sfRow >= firstIconRow && sfRow <= lastIconRow) {
            ++i;
            continue;
        }
        // remove icon
        qDebug() << "MetaRead::cleanupIcons   REMOVING" << dmRow;
        QStandardItem *item = dm->itemFromIndex(dmIdx);
        if (item->icon().isNull()) {
            i++;
            continue;
        }
        item->setIcon(QIcon());
        rowsWithIcon.remove(i);
    }
    /* cleanup by iterating entire model
    for (int row = 0; row < dm->rowCount(); ++row) {
        if (row >= firstIconRow && row <= lastIconRow) {
            continue;
        }
        QModelIndex dmIdx = dm->index(row, 0);
        QStandardItem *item = dm->itemFromIndex(dmIdx);
        if (item->icon().isNull()) {
            continue;
        }
        // emit setIcon(dmIdx, nullPm, dmInstance, "MetaRead::clearIcons"); causes
        // deallocation crash
        item->setIcon(QIcon());
    }
    */
}

void MetaRead::readMetadata(QModelIndex sfIdx, QString fPath)
{
//    if (abort) return;
    if (G::isLogger) G::log("MetaRead::readMetadata");
    int dmRow = dm->sf->mapToSource(sfIdx).row();
    if (debugCaching) qDebug().noquote() << "MetaRead::readMetadata" << "start  row =" << sfIdx.row();
    QFileInfo fileInfo(fPath);
    if (metadata->loadImageMetadata(fileInfo, true, true, false, true, CLASSFUNCTION)) {
        metadata->m.row = dmRow;
        metadata->m.dmInstance = dmInstance;
        if (!abort) emit addToDatamodel(metadata->m);
    }
    if (debugCaching) qDebug().noquote() << "MetaRead::readMetadata" << "done row =" << sfIdx.row();
}

void MetaRead::readIcon(QModelIndex sfIdx, QString fPath)
{
    if (G::isLogger) G::log("MetaRead::readIcon");
    if (debugCaching) {
        qDebug().noquote() << "MetaRead::readIcon"
                           << "start  row =" << sfIdx.row()
                              ;
    }

    // check if already caching icon (video icons)
    if (dm->isIconCaching(sfIdx.row())) return;

    QModelIndex dmIdx = dm->sf->mapToSource(sfIdx);
    int dmRow = dmIdx.row();
    bool isVideo = dm->index(dmRow, G::VideoColumn).data().toBool();

    // get thumbnail
    QImage image;
    bool thumbLoaded = thumb->loadThumb(fPath, image, "MetaRead::readIcon");
    if (isVideo) {
//        iconsLoaded.append(dmRow);
//        rowsWithIcon.append(dmRow);
        return;
    }
    if (thumbLoaded) {
        QPixmap pm = QPixmap::fromImage(image.scaled(G::maxIconSize, G::maxIconSize, Qt::KeepAspectRatio));
//        emit setIcon(dmIdx, pm, dmInstance, "MetaRead::readIcon");  // also works
        dm->setIcon(dmIdx, pm, dmInstance);
        rowsWithIcon.append(dmRow);
        iconMax(pm);
//        qDebug() << "MetaRead::readIcon   dmRow =" << dmRow << rowsWithIcon.size();
    }
    if (debugCaching) {
        qDebug().noquote() << "MetaRead::readIcon"
                           << "done   row =" << sfIdx.row()
                              ;
    }
}

void MetaRead::readRow(int sfRow)
{
    if (G::isLogger) G::log("MetaRead::readRow");
    if (debugCaching) {
        qDebug().noquote() << "MetaRead::readRow"
                           << "start  row =" << sfRow
                              ;
    }
    // range check
    if (sfRow >= dm->sf->rowCount()) return;
    // index valid?
    QModelIndex sfIdx = dm->sf->index(sfRow, 0);
    if (!sfIdx.isValid()) return;

    // load metadata
    QString fPath = sfIdx.data(G::PathRole).toString();
    if (!G::allMetadataLoaded) {
        bool metaLoaded = dm->sf->index(sfRow, G::MetadataLoadedColumn).data().toBool();
        if (!metaLoaded && !abort) {
            readMetadata(sfIdx, fPath);
        }
    }

    if (abort) return;

    // load icon
    /*
    qDebug() << CLASSFUNCTION
             << "sfRow =" << sfRow
             << "iconsLoaded.size() =" << iconsLoaded.size()
             << "iconChunkSize =" << iconChunkSize
             << "adjIconChunkSize =" << adjIconChunkSize
             ;
    //*/
    if (inIconRange(sfRow)) {
        if (!dm->iconLoaded(sfRow)) {
            readIcon(sfIdx, fPath);
            if (rowsWithIcon.size() > iconLimit) {
                cleanupIcons();
            }
        }
    }

    // update the imageCache item data
    if (!abort) emit addToImageCache(metadata->m);
    if (debugCaching) {
        qDebug().noquote() << "MetaRead::readRow"
                           << "done   row =" << sfRow
                              ;
    }
}

void MetaRead::read(/*Action action, */int startRow, QString src)
{
/*
    Loads the metadata and icons into the datamodel (dm) for a folder.  The iteration
    proceeds from the start row in an ahead/behind progression.
*/
    if (G::isLogger || G::isFlowLogger) G::log("MetaRead::read", src);

    abort = false;
    isRunning = true;
    iconLimit = iconChunkSize * 1.2;
    newStartRow = -1;
    sfRowCount = dm->sf->rowCount();
    if (debugCaching) {
        qDebug().noquote() << CLASSFUNCTION
                           << "src =" << src
                           << "sfRowCount =" << sfRowCount
                              ;
    }
    if (startRow >= sfRowCount) return;

    dmInstance = dm->instance;
    emit runStatus(true, true, "MetaRead::read");

    // range of datamodel rows to load icons
    firstIconRow = startRow - iconChunkSize / 2;
    if (firstIconRow < 0) firstIconRow = 0;
    lastIconRow = firstIconRow + iconChunkSize;
//    qDebug() << "MetaRead::read" << sfRow << firstIconRow << lastIconRow
//             << rowsWithIcon.size();

    if (rowsWithIcon.size() > iconChunkSize) {
//        cleanupIcons();
    }

    int i = 0;
    int row = startRow;
    bool ahead = true;
    int lastRow = sfRowCount - 1;
    bool moreAhead = row < lastRow;
    bool moreBehind = row >= 0;
    int rowAhead = row;
    int rowBehind = row;

    while (i++ <= sfRowCount) {
        if (abort) break;
        // do something with row
        readRow(row);
        if (!G::allMetadataLoaded && !imageCachingStarted && !abort) {
            if (i == lastRow || i == 50) {
                // start image caching thread after head start
                emit startImageCache();
                imageCachingStarted = true;
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
    }

    emit runStatus(false, true, "MetaRead::read");
    if (abort) {
        abort = false;
        isRunning = false;
        return;
    }
    G::allMetadataLoaded = true;


    cleanupIcons();

    emit updateIconBestFit();
//    if (!abort) emit done();
    isRunning = false;
}
