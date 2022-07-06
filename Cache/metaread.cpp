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

MetaRead::MetaRead(QObject *parent, DataModel *dm)
{
    if (G::isLogger) G::log(CLASSFUNCTION);
    this->dm = dm;
    metadata = new Metadata;
    thumb = new Thumb(dm, metadata);
    abort = false;
    debugCaching = false;
}

MetaRead::~MetaRead()
{
    stop();
    delete metadata;
    delete thumb;
}

void MetaRead::stop()
{
    abort = true;
}

void MetaRead::initialize()
{
    if (G::isLogger || G::isFlowLogger) G::log(CLASSFUNCTION);
    iconsLoaded.clear();
    visibleIcons.clear();
    priorityQueue.clear();
    imageCachingStarted = false;
}

QString MetaRead::diagnostics()
{
    if (G::isLogger) G::log(CLASSFUNCTION);
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
    rpt << "\n" ;
    rpt << reportMetaCache();

    rpt << "\n\n" ;
    return reportString;
}

QString MetaRead::reportMetaCache()
{
    if (G::isLogger) G::log(CLASSFUNCTION);
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
    if (G::isLogger) G::log(CLASSFUNCTION);
    if (G::iconWMax == G::maxIconSize && G::iconHMax == G::maxIconSize) return;

    // for best aspect calc
    int w = thumb.width();
    int h = thumb.height();
    if (w > G::iconWMax) G::iconWMax = w;
    if (h > G::iconHMax) G::iconHMax = h;
}

/*
void MetaRead::iconCleanup()
{
    When the icon targets (the rows that we want to load icons) change we need to clean up any
    other rows that have icons previously loaded in order to minimize memory consumption. Rows
    that have icons are tracked in the list iconsLoaded as the dm row (not dm->sf proxy).

    We need to keep the total icons loaded under iconChunkSize.  We want the icons to be loaded
    in the top of the priorityQueue, which is the list of sfRows, first by visible, and then
    alternating front and back for all of sf.

    Assuming the most complicated case, where the dataset has been filtered (sf), then the
    remainder of the dataset is non_sf. We first remove icons from non_sf and then in sf,
    at the end of the priorityQueue, until we have removed enough.

    - all items in datamodel (dm)
    i items with icon loaded (30 = iconChunkSize)
    f filtered items (21 with 9 icon loaded, need 21 - 9 = 12 icons)
    v visible items
    x icons to remove (12)
    + icons to add (12)
    o outcome after unnecessary icons removed and new ones added

    --iiiiii----------iiiiiiii-------------iiiiiiiiiii-----ii--iii-- 30
    ---------fffffff---ffffff---ffff------ffff---------------------- 21
    ---------vvvvvvv------------------------------------------------  6
    --xxxxxx----------x------x----------------xxx------------------- 12
    ---------+++++++------------++++------+------------------------- 12
    ---------ooooooo---oooooo---oooo------oooo-ooooooo-----oo--ooo-- 30

    if (G::isLogger) G::log(CLASSFUNCTION);

//    QPixmap null0Pm;
//    for (int row = 0; row < dm->rowCount(); ++row) {
//        QStandardItem *item = dm->itemFromIndex(dm->index(row, 0));
//        item->setIcon(null0Pm);
//    }
    dm->clearAllIcons();
    iconsLoaded.clear();
    return;

    // cleanup extra icons, first count icons loaded in dm->sf
    int allIconsLoaded = iconsLoaded.size() ;
    int sfIconsLoaded = 0;
    for (int sfRow : priorityQueue) {
        int dmRow = dm->modelRowFromProxyRow(sfRow);
        if (iconsLoaded.contains(dmRow)) sfIconsLoaded++;
    }
    // icons loaded but not in dm->sf
    int iconsLoadedNonSF = allIconsLoaded - sfIconsLoaded;
    // icons in dm->sf not loaded
    int sfIconsNotLoaded = sfRowCount - sfIconsLoaded;
//
//    qDebug() << CLASSFUNCTION
//             << "allIconsLoaded =" << allIconsLoaded
//             << "sfIconsLoaded =" << sfIconsLoaded
//             << "iconsLoadedNonSF =" << iconsLoadedNonSF
//             << "sfIconsNotLoaded =" << sfIconsNotLoaded
//             << "iconChunkSize =" << iconChunkSize
//             << "priorityQueue.size() =" << priorityQueue.size()
//                ;
    // is cleanup required
    if (allIconsLoaded + sfIconsNotLoaded > adjIconChunkSize) {
        int iconsToCleanup = allIconsLoaded + sfIconsNotLoaded - adjIconChunkSize;
        QPixmap nullPm;
        int iconsRemoved = 0;
        // remove iconsToCleanup (iconsLoaded are dmRow)
        for (int i : iconsLoaded) {
            int dmRow = iconsLoaded.at(i);
            qDebug() << CLASSFUNCTION << dmRow;
            int sfRow = dm->proxyRowFromModelRow(dmRow);
            // not in dm->sf
            if (sfRow == -1) {
                // remove
                dm->itemFromIndex(dm->index(dmRow, 0))->setIcon(nullPm);
                #if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
                iconsLoaded.remove(i);
                #endif
                iconsRemoved++;
            }
            if (iconsToCleanup >= iconsRemoved) return;
        }
        // still need to remove more icons from back of priorityQueue
        for (int p = priorityQueue.size(); p > 0; --p) {
            int sfRow = priorityQueue.at(p);
            int dmRow = dm->modelRowFromProxyRow(sfRow);
            int i = iconsLoaded.indexOf(dmRow);
            if (i > -1) {
                // remove
                dm->itemFromIndex(dm->index(dmRow, 0))->setIcon(nullPm);
                #if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
                iconsLoaded.remove(i);
                #endif
                iconsRemoved++;
            }
            if (iconsToCleanup == iconsRemoved) return;
        }
    }
}
*/

bool MetaRead::isNotLoaded(int sfRow)
{
    /* not used  */

    QModelIndex sfIdx = dm->sf->index(sfRow, G::MetadataLoadedColumn);
    if (!sfIdx.data().toBool()) return true;
    if (!dm->iconLoaded(sfRow)) return true;
    return false;
}

bool MetaRead::okToLoadIcon(int sfRow) {
    if (sfRow >= dm->startIconRange && sfRow <= dm->endIconRange) return true;
    else return false;
}

void MetaRead::dmRowRemoved(int dmRow)
{
    if (G::isLogger) G::log(CLASSFUNCTION);
//    if (abort) return;
    int idx = iconsLoaded.indexOf(dmRow);
    iconsLoaded.removeAt(idx);
}

void MetaRead::cleanupIcons()
{
/*
    Remove icons not in priority queue after iconChunkSize
*/
    if (G::isLogger) G::log(CLASSFUNCTION);
    // cleanup non-visible icons
    if (debugCaching) {
        qDebug().noquote() << CLASSFUNCTION
                           << "start"
                              ;
    }

    // check if datamodel size is less than assigned icon cache chunk size
    if (G::loadOnlyVisibleIcons && visibleIconCount >= sfRowCount) return;
    if (iconChunkSize >= sfRowCount) return;

    firstIconRow = dm->startIconRange;
    lastIconRow = dm->endIconRange;
    QPixmap nullPm;

//    goto B;

    //  OPTION A: search dm outside icon range
    A:
    // dm rows outside icon range
    for (int dmRow = 0; dmRow < dm->rowCount(); ++dmRow) {
        if (abort) return;
        int sfRow = dm->proxyRowFromModelRow(dmRow);
        // in range?
        if (sfRow >= firstIconRow && sfRow <= lastIconRow) continue;
        // icon not loaded?
        QModelIndex dmIdx = dm->index(dmRow, 0);
        if (dm->itemFromIndex(dmIdx)->icon().isNull()) continue;
        // remove unwanted icon
        emit setIcon(dmIdx, nullPm, dmInstance);
    }
    return;

    // OPTION B: search iconsLoaded for icons outside range
    B:
    int n = iconsLoaded.size();
//    QPixmap nullPm;
    for (int i = 0; i < iconsLoaded.size(); ++i) {
        if (abort) {
            return;
        }
        int dmRow = iconsLoaded.at(i);
        // check if row has been deleted
        if (dmRow >= dm->rowCount()) {
            /*
            qDebug() << CLASSFUNCTION
                     << "dmRow =" << dmRow
                     << "rowCount =" << dm->rowCount()
                        ;
                        //*/
            #if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
            if (!abort) iconsLoaded.remove(i);
            #endif
            continue;
        }
        QModelIndex dmIdx = dm->index(dmRow, 0);
        int sfRow = dm->proxyRowFromModelRow(dmRow);
        /*
        qDebug() << CLASSFUNCTION
                 << "i =" << i
                 << "iconsLoaded.at(i) = dmRow =" << dmRow
                 << "sfRow =" << sfRow
                 << "dm->firstVisibleRow =" << dm->firstVisibleRow
                 << "dm->lastVisibleRow =" << dm->lastVisibleRow
                 << "isVisible(sfRow) =" << isVisible(sfRow)
                    ;
                    //*/
        if (okToLoadIcon(sfRow)) continue;
        emit setIcon(dmIdx, nullPm, dmInstance);
        #if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
        iconsLoaded.remove(i);
        #endif
    }
    if (debugCaching) {
        qDebug().noquote() << CLASSFUNCTION
                           << "done"
                              ;
    }
}

void MetaRead::updateIcons()    // not used
{
    if (G::isLogger) G::log(CLASSFUNCTION);
    if (debugCaching) {
        qDebug().noquote() << CLASSFUNCTION
                           << "start"
                              ;
    }

    // load any missing visible icons
//    for (int sfRow = dm->firstVisibleRow; sfRow <= dm->lastVisibleRow; ++sfRow) {
    for (int sfRow = dm->startIconRange; sfRow <= dm->endIconRange; ++sfRow) {
        int dmRow = dm->modelRowFromProxyRow(sfRow);
        QModelIndex dmIdx = dm->index(dmRow, 0);
        if (!dm->iconLoaded(sfRow)) {
            QImage image;
            QString fPath = dmIdx.data(G::PathRole).toString();
            bool thumbLoaded = thumb->loadThumb(fPath, image, "MetaRead::readIcon");
            if (thumbLoaded && !abort) {
                QPixmap pm = QPixmap::fromImage(image.scaled(G::maxIconSize, G::maxIconSize, Qt::KeepAspectRatio));
                emit setIcon(dmIdx, pm, dmInstance);
                iconMax(pm);
                iconsLoaded.append(dmRow);
            }
        }
    }

    cleanupIcons();
    if (debugCaching) {
        qDebug().noquote() << CLASSFUNCTION
                           << "done"
                              ;
    }
}

void MetaRead::buildMetadataPriorityQueue(int sfRow)
{
    if (G::isLogger) G::log(CLASSFUNCTION);
    priorityQueue.clear();
    firstIconRow = dm->startIconRange;
    lastIconRow = dm->endIconRange;

    if (debugCaching) {
        qDebug().noquote() << CLASSFUNCTION
                           << "start"
                           << "firstVisible =" << firstIconRow
                           << "lastVisible =" << lastIconRow
                           << "priorityQueue.size() =" << priorityQueue.size()
                           << "sfRowCount =" << sfRowCount
                              ;
    }
    // icon cleanup all icons no longer visible

    // priority alternate ahead/behind until finished
    int behind = sfRow;
    int ahead = sfRow + 1;
    while (behind >= 0 || ahead < sfRowCount) {
        if (behind >= 0) priorityQueue.append(behind--);
        if (ahead < sfRowCount) priorityQueue.append(ahead++);
        if (abort) return;
    }
    if (debugCaching) {
        qDebug().noquote() << CLASSFUNCTION
                           << "start"
                           << "firstVisible =" << firstIconRow
                           << "lastVisible =" << lastIconRow
                           << "priorityQueue.size() =" << priorityQueue.size()
                           << "sfRowCount =" << sfRowCount
                              ;
    }
}

void MetaRead::readMetadata(QModelIndex sfIdx, QString fPath)
{
//    if (abort) return;
    if (G::isLogger) G::log(CLASSFUNCTION);
    int dmRow = dm->sf->mapToSource(sfIdx).row();
    if (debugCaching) qDebug().noquote() << CLASSFUNCTION << "start  row =" << sfIdx.row();
    QFileInfo fileInfo(fPath);
    if (metadata->loadImageMetadata(fileInfo, true, true, false, true, CLASSFUNCTION)) {
        metadata->m.row = dmRow;
        metadata->m.dmInstance = dmInstance;
//        qDebug() << CLASSFUNCTION << "addToDatamodel: start  row =" << sfIdx.row();
//        if (debugCaching) qDebug().noquote() << CLASSFUNCTION << "start  addToDatamodel"
//                                             << "abort =" << abort
//                                                ;
        if (!abort) emit addToDatamodel(metadata->m);
//        if (debugCaching) qDebug().noquote() << CLASSFUNCTION << "done   addToDatamodel";
//        qDebug() << CLASSFUNCTION << "addToDatamodel: done   row =" << sfIdx.row();
//        dm->addMetadataForItem(metadata->m);
    }
    if (debugCaching) qDebug().noquote() << CLASSFUNCTION << "done row =" << sfIdx.row();
}

void MetaRead::readIcon(QModelIndex sfIdx, QString fPath)
{
    if (G::isLogger) G::log(CLASSFUNCTION);
    if (debugCaching) {
        qDebug().noquote() << CLASSFUNCTION
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
        iconsLoaded.append(dmRow);
        return;
    }
    if (thumbLoaded) {
        QPixmap pm = QPixmap::fromImage(image.scaled(G::maxIconSize, G::maxIconSize, Qt::KeepAspectRatio));
        if (!abort) emit setIcon(dmIdx, pm, dmInstance);
        iconMax(pm);
        iconsLoaded.append(dmRow);
    }
    if (debugCaching) {
        qDebug().noquote() << CLASSFUNCTION
                           << "done   row =" << sfIdx.row()
                              ;
    }
}

void MetaRead::readRow(int sfRow)
{
    if (G::isLogger) G::log(CLASSFUNCTION);
    if (debugCaching) {
        qDebug().noquote() << CLASSFUNCTION
                           << "start  row =" << sfRow
                              ;
    }
    if (sfRow >= dm->sf->rowCount()) return;
    QModelIndex sfIdx = dm->sf->index(sfRow, 0);
    if (!sfIdx.isValid()) return;
    QString fPath = sfIdx.data(G::PathRole).toString();
    if (!G::allMetadataLoaded) {
        bool metaLoaded = dm->sf->index(sfRow, G::MetadataLoadedColumn).data().toBool();
        if (!metaLoaded && !abort) {
            readMetadata(sfIdx, fPath);
        }
    }

    // load icon
    /*
    qDebug() << CLASSFUNCTION
             << "sfRow =" << sfRow
             << "iconsLoaded.size() =" << iconsLoaded.size()
             << "iconChunkSize =" << iconChunkSize
             << "adjIconChunkSize =" << adjIconChunkSize
             ;
    //*/
    if (okToLoadIcon(sfRow) && !abort) {
        readIcon(sfIdx, fPath);
    }

    // update the imageCache item data
    if (!abort) emit addToImageCache(metadata->m);
    if (debugCaching) {
        qDebug().noquote() << CLASSFUNCTION
                           << "done   row =" << sfRow
                              ;
    }
}

void MetaRead::read(/*Action action, */int sfRow, QString src)
{
    if (G::isLogger || G::isFlowLogger) G::log(CLASSFUNCTION, src + " action = " + QString::number(action));

    if (isRunning) {
        G::log(CLASSFUNCTION, "ROW: " + QString::number(sfRow) + " running");
        mutex.lock();
        abort = true;
        mutex.unlock();
        while (isRunning) {}
        G::log(CLASSFUNCTION, "ROW: " + QString::number(sfRow) + " aborted");
    }

    abort = false;
    isRunning = true;
    sfRowCount = dm->sf->rowCount();
    if (debugCaching) {
        qDebug().noquote() << CLASSFUNCTION
                           << "src =" << src
                           << " action =" << QString::number(action)
                           << "sfRowCount =" << sfRowCount
                              ;
    }
    if (sfRow >= sfRowCount) return;

    sfStart = sfRow;
    dmInstance = dm->instance;
    emit runStatus(true, true, "MetaRead::read");

    // build priority queue for reading metadata and icons
//    G::log(CLASSFUNCTION, "Build priority queue");
    buildMetadataPriorityQueue(sfStart);

    // cleanup unneeded icons
//    G::log(CLASSFUNCTION, "Cleanup icons");
    if (!abort) cleanupIcons();

    G::log(CLASSFUNCTION, "Read metadata and icons");
    int n = static_cast<int>(priorityQueue.size());
    for (int i = 0; i < n; i++) {
        if (abort) {
            break;
        }
//        if (G::isLogger || G::isFlowLogger) G::log(CLASSFUNCTION, "i =" + QString::number(i));
        readRow(priorityQueue.at(i));
        if (!G::allMetadataLoaded && !imageCachingStarted && !abort) {
            if (i == (n - 1) || i == 50) {
                if (!abort) emit delayedStartImageCache();
                imageCachingStarted = true;
            }
        }
    }


    emit runStatus(false, true, "MetaRead::read");
    if (abort) {
        G::log(CLASSFUNCTION, "aborted");
        abort = false;
        isRunning = false;
        return;
    }
    G::log(CLASSFUNCTION, "Finished without abort");
    if (!abort) emit updateIconBestFit();
    if (!abort) emit done();
    isRunning = false;
}
