#include "Image/thumb.h"
#include "Main/global.h"

#ifdef Q_OS_MAC
// Defined in Image/thumb_mac.mm — fast HEIC/JPEG/TIFF thumbnail via ImageIO.
bool macImageIOThumbnail(const QString &fPath, int maxPixelSize, QImage &out);
#endif

/*
   Loads a thumbnail preview from a file based on metadata already extracted by
   mdCache. If the file contains a thumbnail jpg it is extracted. If not, then
   then entire image is read and scaled to thumbMax.
*/

Thumb::Thumb(DataModel *dm, FrameDecoder *frameDecoder)
{
    this->dm = dm;
    this->frameDecoder = frameDecoder;  // shared instance owned by MetaRead
    metadata = new Metadata;

    thumbMax.setWidth(G::maxIconSize);
    thumbMax.setHeight(G::maxIconSize);

    connect(this, &Thumb::setValDm, dm, &DataModel::setValDm, Qt::QueuedConnection);
    connect(this, &Thumb::setValSf, dm, &DataModel::setValSf, Qt::QueuedConnection);

    // FrameDecoder→DataModel signals are connected once in MetaRead.
    connect(this, &Thumb::videoFrameDecode, frameDecoder, &FrameDecoder::addToQueue);

    isDebug = false;
}

Thumb::~Thumb()
{
    // frameDecoder is shared and owned by MetaRead — do not delete here.
}

void Thumb::abortProcessing()
{
    QString fun = "Thumb::abortProcessing";
    if (isDebug)
    {
        qDebug() << fun;
    }
    // FrameDecoder is shared across all Thumb/Reader instances; stopping it
    // here would flush other Readers' pending video work. The global flush
    // happens in MetaRead::abortProcessing.

    // Now wait until idle or timeout
    QDeadlineTimer deadline(500);
    QMutexLocker lock(&mutex);
    abort = true;
    while (!idle) {
        const int ms = int(deadline.remainingTime());
        if (!idleCondition.wait(&mutex, ms)) break; // break on timeout
    }
    // Don't reset abort here. Let the code that *restarts* work clear it.
}

void Thumb::setIdle()
{
    QMutexLocker lock(&mutex);
    if (idle) return;
    idle = true;
    idleCondition.wakeAll();  // notify waiters
}

void Thumb::setBusy()
{
    QMutexLocker lock(&mutex);
    idle = false;
}

void Thumb::checkOrientation(QImage &image, int orientation, int rotationDegrees)
{
    QString fun = "Thumb::checkOrientation";
    if (isDebug)
        qDebug().noquote()
            << fun.leftJustified(col0Width)
            << "isGuiThread =" << G::isGuiThread()
            ;
    // check orientation and rotate if portrait
    QTransform trans;
    int degrees = 0;
    switch (orientation) {
        case 3:
            degrees = rotationDegrees + 180;
            if (degrees > 360) degrees = degrees - 360;
            trans.rotate(degrees);
            image = image.transformed(trans, Qt::SmoothTransformation);
            break;
        case 6:
            degrees = rotationDegrees + 90;
            if (degrees > 360) degrees = degrees - 360;
            trans.rotate(degrees);
            image = image.transformed(trans, Qt::SmoothTransformation);
            break;
        case 8:
            degrees = rotationDegrees + 270;
            if (degrees > 360) degrees = degrees - 360;
            trans.rotate(degrees);
            image = image.transformed(trans, Qt::SmoothTransformation);
            break;
    }

    if (isDebug)
    {
    qDebug().noquote()
             << fun.leftJustified(col0Width)
             << "orientation =" << orientation
             << "rotationDegrees   =" << rotationDegrees
             << "degrees =" << QString::number(degrees).leftJustified(3, ' ')
                ;
    }
}

void Thumb::setImageDimensions(QString &fPath, QSize size, int row)
{
    QString fun = "Thumb::setImageDimensions";
    if (isDebug)
        qDebug().noquote()
            << fun.leftJustified(col0Width)
            << "row =" << row;
    if (G::isLogger) G::log(fun, "row = " + QString::number(row));
    int w = size.width();
    int h = size.height();
    if (h == 0) {
        QString msg = "Image width and/or height = 0.";
        G::issue("Warning", msg, "Thumb::setImageDimensions", dmRow, fPath);
        return;
    }
    double ar = w * 1.0 / h;
    QString a = QString::number(ar, 'f', 2);
    int alignRight = Qt::AlignRight | Qt::AlignVCenter;
    QString d = QString::number(w) + "x" + QString::number(h);
    QString src = "Thumb::setImageDimensions";

    emit setValDm(row, G::WidthColumn, w, instance, src, Qt::EditRole, Qt::AlignCenter);
    emit setValDm(row, G::WidthPreviewColumn, w, instance, src);
    emit setValDm(row, G::HeightColumn, h, instance, src, Qt::EditRole, Qt::AlignCenter);
    emit setValDm(row, G::HeightPreviewColumn, h, instance, src);
    emit setValDm(row, G::AspectRatioColumn, a, instance, src, Qt::EditRole, alignRight);
    emit setValDm(row, G::DimensionsColumn, d, instance, src, Qt::EditRole, Qt::AlignCenter);

    emit setValDm(row, G::MetadataStatusColumn, G::MetaLoaded, instance, src);

}

void Thumb::loadFromVideo(QString &fPath, int dmRow)
{
/*
    see top of FrameDecoder.cpp for documentation
*/
    QString fun = "Thumb::loadFromVideo";
    // if (isDebug)
        qDebug().noquote()
            << fun.leftJustified(col0Width)
            << "row =" << dmRow
            << "isGUI =" << G::isGuiThread()
            << fPath
            ;
    if (G::isLogger) G::log(fun, fPath);

    if (!abort)
        emit videoFrameDecode(fPath, G::maxIconSize, "dmThumb", dmRow, dm->instance);
    // NEW: Tell MetaRead the icon is "loaded" so it doesn't get stuck in a redo loop
    // and prematurely abort before calling folderChangeCompleted()
    emit setValDm(dmRow, G::IconLoadedColumn, true, dm->instance, "Thumb::loadFromVideo");
}

Thumb::Status Thumb::loadFromEntireFile(QString &fPath, QImage &image, int row)
{
    QString fun = "Thumb::loadFromEntireFile";
    if (isDebug)
        qDebug().noquote()
            << fun.leftJustified(col0Width)
            << "row =" << row << fPath;
    if (G::isLogger)
        G::log(fun, fPath);
    if (instance != dm->instance) {
        return Status::Fail;
    }

    QFile imFile(fPath);
    if (imFile.isOpen()) {
        G::log(fun, fPath + " isAlready open");
        return Status::Open;
    }

    QImageReader qReader(fPath);
    qReader.setAutoTransform(false);
    const QSize srcSize = qReader.size();
    if (srcSize.isValid()) {
        const QSize target = srcSize.scaled(thumbMax, Qt::KeepAspectRatio);
        qReader.setScaledSize(target);
    }

    if (!abort && !qReader.read(&image)) {
        QString msg = "Could not read thumb using QImageReader::read: "
                      + qReader.errorString();
        G::issue("Warning", msg, "Thumb::loadFromEntireFile", dmRow, fPath);
        return Status::Fail;
    }

    if (!abort) setImageDimensions(fPath, srcSize.isValid() ? srcSize : image.size(), row);

    if (image.isNull()) {
        QString msg = "Null image returned from thumbReader.";
        G::issue("Warning", msg, "Thumb::loadFromEntireFile", dmRow, fPath);
        return Status::Fail;
    }

    return Status::Success;
}

Thumb::Status Thumb::loadFromJpgData(QString &fPath, QImage &image)
{
    QString fun = "Thumb::loadFromJpgData";
    if (G::isLogger) G::log(fun, fPath);
    if (isDebug)
        qDebug().noquote()
            << fun.leftJustified(col0Width)
            << fPath
            ;

    QFile imFile(fPath);
    if (imFile.isOpen()) {
        QString msg = "File is already open.";
        G::issue("Warning", msg, "Thumb::loadFromJpgData", dmRow, fPath);
        return Status::Open;
    }

    if (abort) return Status::Fail;

    if (imFile.open(QIODevice::ReadOnly)) {
        bool success = false;
        if (!abort) {
            if (imFile.seek(offsetThumb)) {
                QByteArray buf = imFile.read(lengthThumb);
                success =  image.loadFromData(buf, "JPEG");
                /* Embedded thumb is JPEG for all current formats; fall back to
                   Qt format auto-detection (content sniffing) in case a format
                   stores a non-JPEG thumb. */
                if (!success) success = image.loadFromData(buf);
            }
        }
        imFile.close();
        if (image.isNull()) {
            QString msg = "Null image.";
            G::issue("Warning", msg, "Thumb::loadFromJpgData", dmRow, fPath);
            return Status::Fail;
        }
        if (success) return Status::Success;
        else return Status::Fail;
    }
    else {
        return Status::Open;
    }
}

Thumb::Status Thumb::loadFromTiff(QString &fPath, QImage &image, int dmRow,
                                  const ImageMetadata &m)
{
/*
    From Tiff::parse set during DataModel::DataModel::addMetadataForItem
        - m.offsetThumb
        - m.lengthThumb
        - m.thumbFormat
        - m.isEmbeddedThumbMissing

        Based on priority:
            1. IRB Jpg thumb
            2. else chained IFD tiff thumb
            3. else subIFD tiff thumb
            4. else m.isEmbeddedThumbMissing = true, then sample main tiff

*/
    QString fun = "Thumb::loadFromTiff";
    if (G::isLogger) G::log(fun, fPath);
    if (isDebug)
        qDebug().noquote()
            << fun.leftJustified(col0Width)
            << "row =" << dmRow
            << "m.offsetThumb =" << m.offsetThumb
            << "m.m.isEmbeddedThumbMissing =" << m.isEmbeddedThumbMissing
            << fPath
            ;

    if (abort) return Status::Fail;

    QFile imFile(fPath);
    if (imFile.isOpen()) {
        QString msg = "File is already open.";
        G::issue("Warning", msg, "Thumb::loadFromTiff", dmRow, fPath);
        return Status::Open;
    }

    if (abort) return Status::Fail;
    Tiff tiff("Thumb::loadFromTiff");
    if (abort) return Status::Fail;

    // if no thumbnail then sample full image
    if (m.isEmbeddedThumbMissing) {
        if (!tiff.readSample(fPath, &image, G::maxIconSize, m.offsetFull)) {
            QString errMsg = "Could not read because Tiff::readSample failed.";
            G::issue("Error", errMsg, "Thumb::loadFromTiff", dmRow, fPath);
            // qDebug() << fun << errMsg;
            return Status::Fail;
        }
    }
    // read thumbnail
    else {
        if (!tiff.read(fPath, &image, m.offsetThumb)) {
            QString errMsg = "Could not read because Tiff::read failed.";
            G::issue("Error", errMsg, "Thumb::loadFromTiff", dmRow, fPath);
            qDebug() << fun << errMsg;
            return Status::Fail;
        }
    }

    image = image.scaled(G::maxIconSize, G::maxIconSize, Qt::KeepAspectRatio, Qt::FastTransformation);
    return Status::Success;
}

Thumb::Status Thumb::loadFromHeic(QString &fPath, QImage &image)
{
    QString fun = "Thumb::loadFromHeic";
    if (isDebug)
        qDebug().noquote()
            << fun.leftJustified(col0Width)
            << fPath
            ;
    if (G::isLogger) G::log(fun, fPath);

    if (abort) return Status::Fail;
    QFile imFile(fPath);
    if (imFile.isOpen()) {
        return Status::Open;
    }

    #ifdef Q_OS_WIN
    Heic heic;
    // try to read heic thumbnail
    if (heic.decodeThumbnail(fPath, image)) {
        if (image.isNull()) {
            QString msg = "Could not read thumb using Heic::decodeThumbnail.";
            G::issue("Warning", msg, "Thumb::loadFromHeic", dmRow, fPath);
            return Status::Fail;
        }
        return Status::Success;
    }

    // try read entire image
    if (heic.decodePrimaryImage(fPath, image)) {
        if (image.isNull()) {
            QString msg = "Could not read thumb using Heic::decodePrimaryImage.";
            G::issue("Warning", msg, "Thumb::loadFromHeic", dmRow, fPath);
            return Status::Fail;
        }
        return Status::Success;
    }

    return Status::Fail;
    #endif

    #ifdef Q_OS_MAC
    if (abort) return Status::Fail;

    // Fast path: ImageIO returns the embedded thumbnail when present, or
    // decodes at the requested size. Either is far cheaper than a full
    // QImage::load() + scaled() of the primary image.
    if (macImageIOThumbnail(fPath, G::maxIconSize, image) && !image.isNull()) {
        return Status::Success;
    }

    // Fallback: Heic natively supported on Mac via Qt's image plugin.
    if (image.load(fPath)) {
        if (image.isNull()) {
            QString msg = "Could not read thumb using QImage::load.";
            G::issue("Warning", msg, "Thumb::loadFromHeic", dmRow, fPath);
            return Status::Fail;
        }
        return Status::Success;
    }
    return Status::Fail;
    #endif
}

Thumb::Status Thumb::loadFromImageIO(QString &fPath, QImage &image)
{
/*
    Decode a thumbnail via the platform image framework. Used as a fallback for
    Canon CR3 files shot in HDR PQ mode, whose embedded THMB/preview is a
    headerless HEVC bitstream (not JPEG), so loadFromJpgData cannot decode it.
    On macOS, ImageIO natively supports CR3 and returns the embedded preview
    (or a downscaled decode of the raw). On Windows there is no safe equivalent
    here (libheif rejects CR3 and Heic::decodeThumbnail does not guard read
    failures), so this returns Fail and loadThumb falls through to
    loadFromEntireFile.
*/
    QString fun = "Thumb::loadFromImageIO";
    if (G::isLogger) G::log(fun, fPath);
    if (isDebug)
        qDebug().noquote() << fun.leftJustified(col0Width) << fPath;

    if (abort) return Status::Fail;

    #ifdef Q_OS_MAC
    if (macImageIOThumbnail(fPath, G::maxIconSize, image) && !image.isNull())
        return Status::Success;
    return Status::Fail;
    #else
    Q_UNUSED(fPath)
    Q_UNUSED(image)
    return Status::Fail;
    #endif
}

void Thumb::presetOffset(uint offset, uint length)
{
/*
    MetaRead reads the metadata and then the thumbnail.  If the thumbnail is embedded
    the offset and length are in the metadata, which is saved in the DataModel.  The
    thumbnail is decoded next, but the DataModel may not yet have been updated, so the
    MetaRead Reader presets the offset and length here.
*/
    QString fun = "Thumb::presetOffset";
    if (isDebug)
        qDebug().noquote()
            << fun.leftJustified(col0Width)
            << "offset =" << offset << "length =" << length << "isGUI =" << G::isGuiThread();
    if (G::isLogger) G::log(fun);
    offsetThumb = offset;
    lengthThumb = length;
    isPresetOffset = true;
}

bool Thumb::loadThumb(QString &fPath, int dmRow , QImage &image, int instance,
                      const ImageMetadata &m, QString src)
{
/*
    Load a thumbnail preview as a decoration icon in the datamodel dm in column 0.
    Raw, jpg, heic and tif files can contain smaller previews. Check if they do and
    load the smaller preview as that is faster than loading the entire full
    resolution image just to get a thumbnail. This thumbnail is used by the grid and
    filmstrip views.

    - if video then loadFromVideo
    - set isEmbeddedThumb
    - get thumb:
        - if isEmbeddedThumb loadFromJpgData
        - if heic loadFromHeic
        - if tif loadFromTiff
        - else loadFromEntireFile

    Called by Reader::readIcon.
*/
    QString fun = "Thumb::loadThumb";
    if (isDebug)
        qDebug().noquote()
            << fun.leftJustified(col0Width)
            << "Instance =" << instance << src << fPath;
    if (G::isLogger) G::log(fun, fPath);

    setBusy();
    abort = false;
    this->dmRow = dmRow;

    if (G::instanceClash(instance, "Thumb::loadThumb")) {
        QString msg = "Instance clash.";
        G::issueDedup("Comment", msg, "Thumb::loadThumb", dmRow, fPath);
        if (isDebug)
        {
        qDebug().noquote()
            << fun.leftJustified(col0Width)
            << "Instance Clash" << "row =" << dmRow
            << "G::instance =" << G::dmInstance << "instance =" << instance
            << fPath;
        }
        return false;
    }
    this->instance = instance;

    QFileInfo fileInfo(fPath);
    QString ext = fileInfo.suffix().toLower();

    // videos are loaded using videoFrameDecode
    bool isVideo = metadata->videoFormats.contains(ext);
    if (isVideo) {
        if (G::renderVideoThumb) {
            if (!abort) loadFromVideo(fPath, dmRow);
            idle = true;
            return true;
        }
    }

    // check permissions
    oldPermissions = fileInfo.permissions();
    if (!(oldPermissions & QFileDevice::ReadUser)) {
        QFileDevice::Permissions newPermissions = fileInfo.permissions() | QFileDevice::ReadUser;
        QFile(fPath).setPermissions(newPermissions);
    }

    if (abort) {idle = true; return false;}

    // get offset and length (both zero if not embedded thumb)
    if (!isPresetOffset) {
        offsetThumb = m.offsetThumb;
        lengthThumb = m.lengthThumb;
    }
    isPresetOffset = false;
    isEmbeddedThumb = offsetThumb && lengthThumb;
    if (isDebug)
    qDebug().noquote()
             << fun.leftJustified(col0Width)
             << "dmRow =" << dmRow
             << "offsetThumb =" << offsetThumb
             << "lengthThumb =" << lengthThumb
             << "isEmbeddedThumb =" << isEmbeddedThumb
             << fPath
                ;

    Status status = Status::None;
    int attempts = 0;
    int maxAttempts = 10;

    // try up to 10 times if file is open (probably ImageCaching)
    while ((status == Status::None || status == Status::Open) && attempts < maxAttempts) {

        if (abort) {idle = true; return false;}

        // try again after 100ms
        if (status == Status::Open) {
            attempts++;
            if (G::isPerfProbe) G::probeThumbRetryCount.fetch_add(1, std::memory_order_relaxed);
            G::wait(100);
        }
        if (abort) {idle = true; return false;}

        /* Raw image file or tiff with embedded jpg thumbnail. The embedded thumb
           is JPEG for all current formats; the only non-JPEG embedded thumbs are
           TIFF-format thumbs in tif IFDs, which leave lengthThumb == 0 (so
           isEmbeddedThumb is false) and are handled by loadFromTiff below.
           loadFromJpgData also content-sniffs as a fallback for safety. */
        if (isEmbeddedThumb) {
            status = loadFromJpgData(fPath, image);
            if (status == Status::Success) break;
        }

        /* Canon CR3 shot in HDR PQ mode stores its embedded preview as a
           headerless HEVC bitstream rather than JPEG, so loadFromJpgData above
           fails. Decode the thumbnail with the platform image framework
           (ImageIO supports CR3 on macOS). Standard CR3 files succeed in
           loadFromJpgData above and never reach here. The Open guard preserves
           the file-locked retry loop. */
        if (ext == "cr3" && status != Status::Success && status != Status::Open) {
            status = loadFromImageIO(fPath, image);
            if (status == Status::Success) break;
        }

        if (ext == "heic") {
            status = loadFromHeic(fPath, image);
            if (status == Status::Success) {
                if (!abort) setImageDimensions(fPath, image.size(), dmRow);
                break;
            }
        }

        if (ext == "tif" && G::useMyTiff) {
            if (!abort) status = loadFromTiff(fPath, image, dmRow, m);
            if (status == Status::Success) break;

            // if (m.isEmbeddedThumbMissing) {
            //     // process on another thread
            //     qDebug().noquote() << fun.leftJustified(col0Width) << dmRow << "no embedded thumb";
            //     tiffThumbDecoder->addToQueue(fPath, dmIdx, instance, m.offsetFull);
            // }
            // else {
            //     qDebug().noquote() << fun.leftJustified(col0Width) << dmRow << "embedded thumb";
            //     if (!abort) status = loadFromTiff(fPath, image, dmRow, m);
            //     if (status == Status::Success) break;
            // }
        }

        // all other image files
        // read the image file (supported by Qt), scaling to thumbnail size
        if (!abort) status = loadFromEntireFile(fPath, image, dmRow);
        if (status == Status::Success) break;

    }

    if (abort) {idle = true; return false;}

    QFile(fPath).setPermissions(oldPermissions);

    if (status == Status::Success) {
        // scale to max icon size
        image = image.scaled(thumbMax, Qt::KeepAspectRatio);
        image.convertTo(QImage::Format_RGB32);

        // rotate if there is orientation metadata
        if (!abort)
            if (metadata->rotateFormats.contains(ext)) checkOrientation(image, m.orientation, m.rotationDegrees);
    }

    setIdle();

    if (status == Status::Success) return true;
    else return false;
}

void Thumb::insertThumbnailsInJpg(QModelIndexList &selection)
{
/*
    Fix missing thumbnails in JPG.  Not being used.  Also see Jpeg::embedThumbnail().
*/
    qDebug() << "Thumb::insertThumbnailsInJpg";
    if (!G::modifySourceFiles) return;
    int count = selection.count();

    G::popup->setProgressVisible(true);
    G::popup->setProgressMax(count);
    QString txt = "Embedding thumbnail(s) for " + QString::number(count) +
                  " JPG images <p>Press <font color=\"red\"><b>Esc</b></font> to abort.";
    G::popup->showPopup(txt, 0, true, 1);
    insertingThumbnails = true;


    ExifTool et;
    et.setOverWrite(true);
    QStringList thumbList;
    for (int i = 0; i < count; ++i) {
        G::popup->setProgress(i+1);
        if (abort) break;

        // check if already a thumbnail
        int offsetThumb = selection.at(i).data(G::OffsetThumbColumn).toInt();
        int offsetFull = selection.at(i).data(G::OffsetFullColumn).toInt();
        if (offsetThumb != offsetFull) continue;

        // collect path information
        QString fPath = selection.at(i).data(G::PathRole).toString();
        QFileInfo info(fPath);
        QString folder = info.dir().path();
        QString base = info.baseName();
        QString thumbPath = folder + "/" + base + "_thumb.jpg";

        // add this thumbPath to the list
        thumbList << thumbPath;

        /*
        qDebug() << "Thumb::insertThumbnails" << i
                 << "fPath =" << fPath
                 << "thumbPath =" << thumbPath
                 ; //*/

        // create a thumbnail size jpg
        QImage thumb = QImage(fPath).scaled(160, 160, Qt::KeepAspectRatio);
        thumb.save(thumbPath, "JPG", 60);

        // back up the source file before modifying it in place
        if (G::backupBeforeModifying && !Utilities::backup(fPath, "backup")) {
            G::issue("Warning", "Backup failed; thumbnail not embedded.",
                     "Thumb::insertThumbnailsInJpg", -1, fPath);
            continue;
        }

        // add the thumb.jpg to the source file
        et.addThumb(thumbPath, fPath);
    }
    et.close();
    insertingThumbnails = false;

    // delete the thumbnail files
    for (int i = 0; i < thumbList.length(); ++i) {
        QFile::remove(thumbList.at(i));
    }
    G::popup->setProgressVisible(false);
    G::popup->reset();
}


