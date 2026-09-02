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

void Thumb::setImageDimensions(QString &fPath, QSize fullSize, QSize previewSize, int row)
{
/*
    Publish an image's dimensions from the thumbnail path.

    TWO SIZES, AND THEY ARE NOT THE SAME FACT. G::WidthColumn is how big the
    IMAGE is -- what the table shows, what MPix and Dimensions and Aspect Ratio
    are derived from, and what gets catalogued. G::WidthOrigPreviewColumn is how
    big the EMBEDDED PREVIEW is, which the image cache uses to budget memory.

    THIS FUNCTION USED TO WRITE THE SAME NUMBER TO BOTH, which was right for the
    paths that decode the whole file and wrong for the ones that decode the
    embedded thumbnail. A HEIC took the second kind on both platforms
    (Heic::decodeThumbnail on Windows, macImageIOThumbnail on macOS, both of
    which return the PREVIEW), so its width was overwritten with 160 -- and
    whichever of the metadata read and the thumbnail decode landed last decided
    what the row said. DataModel::catalogRows then catalogued whatever was
    there, so the index could hold a preview's dimensions for an image. It was
    found by fingerprinting a row served from the catalog against the same row
    read from its file, which is the only reason it was visible at all.

    AN INVALID fullSize MEANS "I ONLY DECODED THE PREVIEW", and the image
    dimensions are then left alone rather than guessed at. The metadata read has
    already set them; if it failed they stay 0, which is honest and which the
    cache and the delegates already handle. Writing the preview's size there
    would be a plausible number that is wrong, and a wrong number is worse than
    a missing one precisely because nothing downstream can tell.
*/
    QString fun = "Thumb::setImageDimensions";
    if (isDebug)
        qDebug().noquote()
            << fun.leftJustified(col0Width)
            << "row =" << row;
    if (G::isLogger) G::log(fun, "row = " + QString::number(row));

    QString src = "Thumb::setImageDimensions";

    if (previewSize.isValid() && previewSize.height() > 0) {
        emit setValDm(row, G::WidthOrigPreviewColumn, previewSize.width(), instance, src);
        emit setValDm(row, G::HeightOrigPreviewColumn, previewSize.height(), instance, src);
    }

    if (fullSize.isValid() && fullSize.height() > 0) {
        const int w = fullSize.width();
        const int h = fullSize.height();
        emit setValDm(row, G::WidthColumn, w, instance, src);
        emit setValDm(row, G::HeightColumn, h, instance, src);
        emit setValDm(row, G::AspectRatioColumn,
                      QString::number(w * 1.0 / h, 'f', 2), instance, src);
        emit setValDm(row, G::DimensionsColumn,
                      QString::number(w) + "x" + QString::number(h), instance, src);
    }
    else if (!previewSize.isValid() || previewSize.height() == 0) {
        /*  Neither size is usable, which is what the old height==0 guard caught.
            Nothing to publish, and MetaLoaded below would be a lie. */
        QString msg = "Image width and/or height = 0.";
        G::issue("Warning", msg, "Thumb::setImageDimensions", dmRow, fPath);
        return;
    }

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

Thumb::Status Thumb::loadFromEntireFile(QString &fPath, QImage &image, int row,
                                        QSize knownFull)
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

    /*  srcSize is QImageReader::size() -- the FULL image. When it is invalid no
        scaled size was set either, so the image just read IS the full image and
        serves as both. */
    /*  knownFull is the metadata read's answer, and it WINS. QImageReader::size()
        looks like the full image and is not reliably so: on macOS a raw goes
        through the ImageIO plugin, which reports the dimensions of whatever
        representation it decoded -- for a NEF that was the 160px embedded
        preview. Trusting it overwrote a correct 6016 with 160, and since only
        the thumbnail path writes these columns there was nothing to put it
        back.

        So the file's own header, already parsed, is preferred; srcSize is the
        fallback for the formats that have no metadata read of their own. */
    const QSize fullSize = knownFull.isValid() && knownFull.height() > 0
                               ? knownFull
                               : (srcSize.isValid() ? srcSize : image.size());
    if (!abort) setImageDimensions(fPath, fullSize, image.size(), row);

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

bool Thumb::loadDevThumb(QString &fPath, QImage &image)
{
/*
    The 256px JPEG of the developed image cached in the XMP sidecar
    (winnow:DevelopPreview), or false when there is none.

    Metadata::readDevThumb returns nothing unless the stored key still matches the
    recipe beside it, so a sidecar rewritten by another application can never show stale
    pixels here -- it just falls through to the camera thumbnail. It also returns
    immediately when no sidecar exists, which is the common case and costs one stat.

    The image arrives already oriented and cropped (developCompositeStack applies EXIF
    rotation and geometry), so the caller must NOT rotate it again.
*/
    /* Original: the user has asked for the camera's picture, so the developed thumbnail is
       the wrong answer even where one exists. Develop mode always shows developed -- you
       cannot edit what you cannot see -- regardless of the setting. */
    if (G::operationMode != G::OperationMode::Develop
        && G::previewSource != G::PreviewSource::Developed) return false;

    const QByteArray jpg = Metadata::readDevThumb(fPath);
    if (jpg.isEmpty()) return false;
    if (!image.loadFromData(jpg, "JPG") || image.isNull()) return false;

    /* Written at G::maxIconSize, but a preview from an older build or a larger icon
       setting must not paint over its cell. */
    if (image.width() > thumbMax.width() || image.height() > thumbMax.height())
        image = image.scaled(thumbMax, Qt::KeepAspectRatio, Qt::FastTransformation);
    image.convertTo(QImage::Format_RGB32);
    return true;
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

    /* A cached develop preview wins over the camera's embedded thumbnail: it is what the
       image actually looks like now. Checked here, BEFORE the permission fiddling and the
       retry loop, because it reads the sidecar and never touches the image file -- and
       before the tail below, which scales and applies checkOrientation. The preview comes
       out of developCompositeStack already rotated and cropped, so running it through
       that tail would rotate it a second time. */
    if (!abort && loadDevThumb(fPath, image)) {
        setIdle();
        return true;
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
                /*  The HEIC paths return the EMBEDDED PREVIEW on both platforms
                    (Heic::decodeThumbnail, macImageIOThumbnail), so there is no
                    full size to publish -- an invalid QSize says so rather than
                    passing the preview off as the image. */
                if (!abort) setImageDimensions(fPath, QSize(), image.size(), dmRow);
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
        if (!abort) status = loadFromEntireFile(fPath, image, dmRow,
                                                QSize(m.width, m.height));
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


