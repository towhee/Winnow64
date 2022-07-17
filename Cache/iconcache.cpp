#include "iconcache.h"

/*
    IconCache loads thumbnails/icons to the datamodel when one of the views: thumbView,
    gridView or tableView is scrolled or resized, but not when there is a file selection
    change (handled by MetaRead) even if the file selection change triggers a scroll
    event.

    The icons are loaded, starting with the visible middle icon, alternating ahead and
    behind; until the iconChunkSize is reached, all icons are loaded or another scroll or
    resize or file selection event occurs.

    For each row:
        • check if icon has been loaded already.
        • if not, check if metadata loaded.
        • if not, then load preview offset and length (minimal metadata for thumbnail).
        • read the QImage, convert to QPixmap, resize to IconMax.
        • siganl datamodel to setIcon.

    Coordinating with MetaRead: if MetaRead is running, then stop loading icons in
    MetaRead if the iconChunkSize is less than the total datamodel rows by signalling
    MetaRead doNotReadIcons.

    IconCache is intended to run in a separate thread using moveToThread instead of a
    subclass of QThread.  This is required in order to handle video thumbnails, where
    a signal/slot connection must be created in a separate thread while running.

    Issues:
        • icon cleanup.
        • set G::AllIconsLoaded
        • if no metadata read, then only read preview offset
*/

IconCache::IconCache(QObject *parent, DataModel *dm, Metadata *metadata)
{
    if (G::isLogger) G::log("MetaRead::MetaRead");
    this->dm = dm;
    this->metadata = metadata;
    thumb = new Thumb(dm, metadata);
    abort = false;
    isRunning = false;
    debugCaching = false;
}

IconCache::~IconCache()
{
    delete thumb;
}

void IconCache::start()
{
    abort = false;
}

void IconCache::stop()
{
    if (G::isLogger || G::isFlowLogger) G::log("MetaRead::stop");
    mutex.lock();
    isRestart = false;
    if (isRunning) {
        abort = true;
    }
    mutex.unlock();
}

void IconCache::iconMax(QPixmap &thumb)
{
    if (G::isLogger) G::log("IconCache::iconMax");
    if (G::iconWMax == G::maxIconSize && G::iconHMax == G::maxIconSize) return;

    // for best aspect calc
    int w = thumb.width();
    int h = thumb.height();
    if (w > G::iconWMax) G::iconWMax = w;
    if (h > G::iconHMax) G::iconHMax = h;
}

void IconCache::read(int sfRow, QString src)
{
    if (G::isLogger || G::isFlowLogger) G::log("IconCache::read", src);
    abort = false;
    isRunning = true;
    int sfRowCount = dm->sf->rowCount();
    if (sfRow >= sfRowCount) return;
    dmInstance = dm->instance;
    int i = 0;
    bool ahead = true;
    int lastRow = sfRowCount - 1;
    bool moreAhead = sfRow < lastRow;
    bool moreBehind = sfRow >= 0;
    int rowAhead = sfRow;
    int rowBehind = sfRow;
    int startIconRange = sfRow - iconChunkSize / 2;
    if (startIconRange < 0) startIconRange = 0;
    int endIconRange = startIconRange + iconChunkSize;

    /*
    qDebug() << "IconCache::read  i =" << i
             << "row =" << sfRow
             << "iconChunkSize =" << iconChunkSize
             << "startIconRange =" << startIconRange
             << "endIconRange =" << endIconRange
                ;
                //*/

    while (i++ < sfRowCount) {
        // next sfRow to process
        if (ahead && i > 1) {
            if (moreBehind) ahead = false;
            if (moreAhead) {
                ++rowAhead;
                sfRow = rowAhead;
                moreAhead = rowAhead < lastRow;
            }
        }
        else {
            if (moreAhead) ahead = true;
            if (moreBehind) {
                --rowBehind;
                sfRow = rowBehind;
                moreBehind = sfRow > 0;
            }
        }

        if (abort) break;
        // in icon range?
        if (sfRow < startIconRange || sfRow > endIconRange) continue;
        // check if icon already loaded
        if (dm->iconLoaded(sfRow)) continue;

        // read metadata if not already loaded
        QModelIndex sfIdx = dm->sf->index(sfRow, 0);
        QModelIndex dmIdx = dm->sf->mapToSource(sfIdx);
        QString fPath = sfIdx.data(G::PathRole).toString();
        int dmRow = dmIdx.row();
        bool metaLoaded = dm->sf->index(sfRow, G::MetadataLoadedColumn).data().toBool();
        if (!metaLoaded) {
            QFileInfo fileInfo(fPath);
            if (metadata->loadImageMetadata(fileInfo, true, true, false, true, CLASSFUNCTION)) {
                metadata->m.row = dmRow;
                metadata->m.dmInstance = dmInstance;
                if (!abort) emit addToDatamodel(metadata->m);
            }
            else {
                // metadata could not be read
                continue;
            }
        }

        // check if already caching icon (video icons)
        if (dm->isIconCaching(sfRow)) continue;
        // might be a video file
        bool isVideo = dm->index(dmRow, G::VideoColumn).data().toBool();
        // get icon/thumbnail
        QImage image;
        bool thumbLoaded = thumb->loadThumb(fPath, image, "MetaRead::readIcon");
        if (isVideo) {
//            iconsLoaded.append(dmRow);
            continue;
        }
        if (thumbLoaded) {
            QPixmap pm = QPixmap::fromImage(image.scaled(G::maxIconSize, G::maxIconSize, Qt::KeepAspectRatio));
            if (!abort) emit setIcon(dmIdx, pm, dmInstance);
            iconMax(pm);
//            iconsLoaded.append(dmRow);
        }
    }

    if (abort) {
        abort = false;
        isRunning = false;
        return;
    }
//    if (!abort) emit updateIconBestFit();
//    if (!abort) emit done();
    isRunning = false;
}
