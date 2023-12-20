#include "reader3.h"

Reader3::Reader3(QObject *parent,
                 QString fPath,
                 int instance,
                 bool isReadIcon,
                 DataModel *dm,
                 ImageCache *imageCache) :

                 fPath(fPath),
                 instance(instance),
                 dm(dm),
                 imageCache(imageCache)
{
}

inline bool Reader3::instanceOk()
{
    return instance == G::dmInstance;
}

bool Reader3::readMetadata()
{
    if (G::isLogger) G::log("Reader::readMetadata");
    if (isDebug)
    {
        qDebug().noquote()
            << "Reader::readMetadata                        "
            //<< "id =" << QString::number(threadId).leftJustified(2, ' ')
            << "row =" << QString::number(dmIdx.row()).leftJustified(4, ' ')
            //<< fPath
            ;
    }
    // read metadata from file into metadata->m
    int dmRow = dmIdx.row();
    QFileInfo fileInfo(fPath);
    bool isMetaLoaded = metadata->loadImageMetadata(fileInfo, instance, true, true, false, true, "Reader::readMetadata");
    if (abort) return false;
    if (isMetaLoaded) {
        metadata->m.row = dmRow;
        metadata->m.instance = instance;

        if (!abort) emit addToDatamodel(metadata->m, "Reader::readMetadata");
        //if (abort) quit();

        if (!dm->isMetadataLoaded(dmRow)) {
            //status = Status::MetaFailed;
            qWarning() << "WARNING" << "MetadataCache::readMetadata  row =" << dmRow << "Failed - emit addToDatamodel." << fPath;
        }

        if (!abort) emit addToImageCache(metadata->m, instance);
        //if (abort) quit();

    }
    else {
        //status = Status::MetaFailed;
        qWarning() << "WARNING" << "MetadataCache::readMetadata  row =" << dmRow << "Failed - metadata not loaded." << fPath;
    }
    return isMetaLoaded;
}

void Reader3::readIcon()
{
    if (G::isLogger) G::log("Reader::readIcon");
    if (isDebug)
    {
        qDebug().noquote()
            << "Reader::readIcon                            "
            //<< "id =" << QString::number(threadId).leftJustified(2, ' ')
            << "row =" << QString::number(dmIdx.row()).leftJustified(4, ' ')
            //<< fPath
            ;
    }

    int dmRow = dmIdx.row();
    QImage image;
    bool thumbLoaded = thumb->loadThumb(fPath, image, instance, "MetaRead::readIcon");
    if (abort) return;
    if (isVideo) return;
    if (thumbLoaded) {
        pm = QPixmap::fromImage(image.scaled(G::maxIconSize, G::maxIconSize, Qt::KeepAspectRatio));
    }
    else {
        pm = QPixmap(":/images/error_image256.png");
        //if (status == Status::MetaFailed) status = Status::MetaIconFailed;
        //else status = Status::IconFailed;
        qWarning() << "WARNING" << "MetadataCache::loadIcon  row =" << dmRow << "Failed to load thumbnail." << fPath;
    }
    if (pm.isNull()) {
        //if (status == Status::MetaFailed) status = Status::MetaIconFailed;
        //else status = Status::IconFailed;
        qWarning() << "WARNING" << "MetadataCache::loadIcon  row =" << dmRow << "Failed - null icon." << fPath;
        return;
    }

    if (!abort) emit setIcon(dmIdx, pm, instance, "MetaRead::readIcon");
    //if (abort) quit();


    if (!dm->iconLoaded(dmRow, instance)) {
        //if (status == Status::MetaFailed) status = Status::MetaIconFailed;
        //else status = Status::IconFailed;
        qWarning() << "WARNING" << "MetadataCache::loadIcon  row =" << dmRow << "Failed - emit setIcon." << fPath;
    }
    else {
        loadedIcon = true;
    }
}

void Reader3::run()
{
    if (!abort && !G::allMetadataLoaded && instanceOk()) readMetadata();
    if (!abort && isReadIcon && instanceOk()) readIcon();
    //if (!abort && instanceOk()) emit done(threadId);
}


