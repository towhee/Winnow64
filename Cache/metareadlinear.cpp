#include "metareadlinear.h"

MetaReadLinear::MetaReadLinear(QObject *parent,
                               DataModel *dm,
                               Metadata *metadata,
                               FrameDecoder *frameDecoder
                              )
{
    if (G::isLogger) G::log("MetadataCache::MetadataCache");
    this->dm = dm;
    this->metadata = metadata;
    this->frameDecoder = frameDecoder;
    thumb = new Thumb(dm, metadata, frameDecoder);

    abort = false;
}

MetaReadLinear::~MetaReadLinear(void)
{
    delete thumb;
}

void MetaReadLinear::stop()
{
    if (G::isLogger) G::log("MetaReadLinear::iconMax");
    abort = true;
}

void MetaReadLinear::iconMax(QPixmap &thumb)
{
    if (G::isLogger) G::log("MetaReadLinear::iconMax");
    if (G::iconWMax == G::maxIconSize && G::iconHMax == G::maxIconSize) return;

    // for best aspect calc
    int w = thumb.width();
    int h = thumb.height();
    if (w > G::iconWMax) G::iconWMax = w;
    if (h > G::iconHMax) G::iconHMax = h;
}

bool MetaReadLinear::loadIcon(int sfRow)
{
    if (G::isLogger) G::log("MetaReadLinear::loadIcon");
    QModelIndex sfIdx = dm->sf->index(sfRow,0);
    QModelIndex dmIdx = dm->sf->mapToSource(sfIdx);
    if (!dmIdx.isValid()) return false;
    if (!dm->isIconCaching(sfIdx, instance) && !dm->iconLoaded(sfRow, instance)) {
        qDebug() << "MetaReadLinear::loadIcon" << sfRow;
//        emit setIconCaching(sfRow, true);
        dm->setIconCaching(sfRow, true);
        int dmRow = dmIdx.row();
        bool isVideo = dm->index(dmRow, G::VideoColumn).data().toBool();
        QImage image;
        QString fPath = dmIdx.data(G::PathRole).toString();
        QPixmap pm;
        bool thumbLoaded = thumb->loadThumb(fPath, image, instance, "MetaReadLinear::loadIcon");
        if (isVideo) {
            iconsCached.append(dmRow);
            return true;
        }
        if (thumbLoaded) {
            pm = QPixmap::fromImage(image.scaled(G::maxIconSize, G::maxIconSize, Qt::KeepAspectRatio));
//            emit setIcon(dmIdx, pm, instance, "MetadataCache::loadIcon");
            dm->setIcon(dmIdx, pm, instance, "MetaReadLinear::loadIcon");
            iconMax(pm);
            iconsCached.append(dmRow);
        }
        else {
            pm = QPixmap(":/images/error_image.png");
            qWarning() << "WARNING" << "MetaReadLinear::loadIcon" << "Failed to load thumbnail." << fPath;
        }
    }
    return true;
}

void MetaReadLinear::updateIconLoadingProgress(int count, int end)
{
    if (G::isLogger) G::log("MetaReadLinear::updateIconLoadingProgress");
    // show progress
    if (!G::isNewFolderLoaded) {
        if (count % countInterval == 0) {
            QString msg = "Loading thumbnails: ";
            msg += QString::number(count) + " of " + QString::number(end);
            emit showCacheStatus(msg);
        }
    }
}

void MetaReadLinear::readIconChunk()
{
/*
    Load the thumb (icon) for all the image files in the target range.  This is called after a
    sort/filter change and all metadata has been loaded, but the icons visible have changed.
*/
    if (G::isLogger || G::isFlowLogger) G::log("MetaReadLinear::readIconChunk");
    int start = startRow;
    int end = endRow;
    if (end > dm->sf->rowCount()) end = dm->sf->rowCount();
    if (cacheAllIcons) {
        start = 0;
        end = dm->sf->rowCount();
    }
    int count = 0;
    /*
    qDebug() << "MetaReadLinear::readIconChunk" << "start =" << start << "end =" << end
             << "firstIconVisible =" << firstIconVisible
             << "lastIconVisible =" << lastIconVisible
             << "rowCount =" << dm->sf->rowCount()
             << "action =" << action;
//    */

    // process visible icons first
    for (int row = firstIconVisible; row <= lastIconVisible; ++row) {
        if (abort) {
            emit updateIsRunning(false, true, "MetaReadLinear::readIconChunk");
            abort = false;
            return;
        }
        loadIcon(row);
        updateIconLoadingProgress(count++, end);
    }

    // process icons before visible range
    if (start < firstIconVisible) {
        for (int row = start; row < firstIconVisible; ++row) {
            if (abort) {
                emit updateIsRunning(false, true, "MetaReadLinear::readIconChunk");
                abort = false;
                return;
            }
            loadIcon(row);
            updateIconLoadingProgress(count++, end);
        }
    }

    // process icons after visible range
    if (end > lastIconVisible + 1) {
        for (int row = lastIconVisible = 1; row < end; ++row) {
            if (abort) {
                emit updateIsRunning(false, true, "MetaReadLinear::readIconChunk");
                abort = false;
                return;
            }
            loadIcon(row);
            updateIconLoadingProgress(count++, end);
        }
    }
}
