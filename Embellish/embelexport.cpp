#include "embelexport.h"
#include "Main/global.h"
#include "Utilities/fileops.h"

/*  AN ASYNCHRONOUS EXPORT.  Each image is a chain of steps run from the event loop, the
    same shape as ImageExporter: startNext asks pixelSource for the develop render,
    onPixels puts it on the scene, compose builds the embellishment and renders the scene
    to a QImage, and the EmbelWriter thread encodes it and runs the ExifTool copies.  The
    GUI thread is never held across an image and never pumped.

    It used to be one loop on the GUI thread that pumped the event loop (G::wait) between
    images and while each develop render ran, with an application-wide filter that
    swallowed every user input except Esc for the duration -- because the pump would
    otherwise have delivered a folder change, a selection change or a quit underneath it.
    Both are gone.  The UI stays usable while an export runs, so the export depends on
    nothing the user can change: the template is locked by MW (startEmbelExport), the
    develop render takes a metadata snapshot (setEmbelPixelSource), the export's Embel
    renders whatever mode the loupe is in, and texts read the exported image's metadata.

    Only the scene stays on the GUI thread: it is a QGraphicsScene of QPixmap items.
    Moving that off too means a QImage-only render path (see notes/_ToDo.txt). */

/* ---------------------------------------------------------------------------------- */
/*  EmbelWriter                                                                         */
/* ---------------------------------------------------------------------------------- */

EmbelWriter::~EmbelWriter()
{
    // runs on the writer thread (deleteLater on QThread::finished), where et was made
    if (et) {
        et->close();
        delete et;
    }
}

void EmbelWriter::write(const QImage &image, const QString &src, const QString &dst,
                        const QString &dstThumb, const QString &extension)
{
/*
    Save the embellished image, then copy the source's tags and ICC into it and embed a
    thumbnail.  The thumbnail file is only a vehicle for ExifTool and is removed after.
*/
    QString srcFun = "EmbelWriter::write";
    if (G::embelLog) G::log(srcFun, dst);

    bool ok = false;
    if (extension == "JPG") ok = image.save(dst, "JPG", 100);
    if (extension == "PNG") ok = image.save(dst, "PNG", 100);
    if (extension == "TIF") ok = image.save(dst, "TIF");

    QString msg = dst + (ok ? " saved" : " failed") +
                  " Image width: " + QString::number(image.width());
    if (G::embelLog) G::log(srcFun, msg);

    if (ok) {
        QImage thumb = image.scaled(160, 160, Qt::KeepAspectRatio);
        const bool hasThumb = thumb.save(dstThumb, "JPG", 60);
        if (!et) {
            et = new ExifTool;
            et->setOverWrite(true);
        }
        // copy all metadata tags from src to dst
        // embellished pixels are upright: the source's Orientation must not follow
        et->copyAllTags(src, dst, /*excludeIcc*/ false, /*upright*/ true);
        // copy ICC from src to dst
        et->copyICC(src, dst);
        // add thumbnail to dst
        if (hasThumb) {
            et->addThumb(dstThumb, dst);
            QFile::remove(dstThumb);
        }
        if (G::embelLog)
            G::log(srcFun, "ExifTool copied tags, ICC and thumbnail to embellished image");
    }

    emit written(ok, dst);
}

/* ---------------------------------------------------------------------------------- */
/*  EmbelExport                                                                         */
/* ---------------------------------------------------------------------------------- */

EmbelExport::EmbelExport(Metadata *metadata,
                         DataModel *dm,
                         ImageCacheData *icd,
                         EmbelProperties *embelProperties,
                         QWidget * /*parent*/)
{
/*
    Create a new, embellished jpg, png or tif from a source image(s).  The source image(s) can
    be local (selected in Winnow) or remote (sent from another program).

    If the source is remote, then some housekeeping has to occur before the embellish export
    process can take place. Remote exports launch Winnow and are initially processed by
    MW::handleStartupArgs, which in turn, calls EmbelExport::exportRemoteFiles, which saves
    the current embellish template, the remote defined template is set, and then
    EmbelExport::exportImages is called.

    Local exports call EmbelExport::exportImages (via MW::startEmbelExport).

    EmbelExport::exportImages starts the chain described at the top of this file, which
    emits finished when the last image has been written.
*/
    if (G::isLogger) G::log("EmbelExport::EmbelExport");
    this->metadata = metadata;
    this->dm = dm;
    this->icd = icd;
    this->embelProperties = embelProperties;

    scene = new QGraphicsScene();
    pmItem = new QGraphicsPixmapItem;
    scene->addItem(pmItem);
    setScene(scene);

    embellish = new Embel(scene, pmItem, embelProperties, dm, icd, "Export");
}

EmbelExport::~EmbelExport()
{
/*
    Deleted mid-export only by MW::closeEvent, after abortEmbelExport.  The write in
    progress completes (a half-written file is worse than a finished one); any queued
    after it are dropped with the thread's event loop.
*/
    if (G::isLogger) G::log("EmbelExport::~EmbelExport");
    if (writerThread) {
        writerThread->quit();
        writerThread->wait();
        delete writerThread;
    }
    delete embellish;
    delete scene;                       // and pmItem with it
}

QString EmbelExport::exportSubfolderPath(QString fPath, bool allowOverride)
{
/*
    The incoming fPath is the file path for one of the images being exported.  Its folder is
    fromFolderPath.  If the export folder save method == "Subfolder", then the subfolder name
    is used, otherwise, all this is ignored and embelProperties->exportSubfolder is used.
*/
    if (G::isLogger) G::log("EmbelExport::exportSubfolderPath");
    QFileInfo filedir(fPath);
    QString fromFolderPath = filedir.dir().path();
    QString exportFolder;
    exportFolder = fromFolderPath + "/" + embelProperties->exportSubfolder;
    QDir dir(fromFolderPath);
    dir.mkdir(embelProperties->exportSubfolder);
    return exportFolder;

    /*
    if (G::isLogger) G::log("EmbelExport::");
    QFileInfo filedir(fPath);
    QString fromFolderPath = filedir.dir().path();
    QString exportFolder;
    if (embelProperties->saveMethod == "Subfolder") {
        exportFolder = fromFolderPath + "/" + embelProperties->exportSubfolder;
        QDir dir(fromFolderPath);
        dir.mkdir(embelProperties->exportSubfolder);
    }
    else exportFolder = embelProperties->exportFolderPath;

    // override default export folder
    if (allowOverride) {
        QString msg = "Select export destination folder";
        exportFolder = QFileDialog::getExistingDirectory(this, msg, exportFolder,
            QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    }
    return exportFolder;
    */
}

bool EmbelExport::isValidExportFolder()
{
    QString srcFun = "EmbelExport::isValidExportFolder";
    if (G::isLogger) G::log(srcFun);
    if (embelProperties->saveMethod == "Subfolder") {
        if (embelProperties->exportSubfolder == "") {
            if (G::embelLog) G::log(srcFun, "Embellish export subfolder not defined");
            return false;
        }
    }
    else {
        if (embelProperties->exportFolderPath == "") {
            if (G::embelLog) G::log(srcFun, "Embellish export subfolder not defined");
            return false;
        }
    }
    return true;
}


bool EmbelExport::exportRemoteFiles(QString templateName, const QStringList &pathList)
{
/*
    Images sent from another program, such as lightroom, are sent here from
    MW::handleStartupArgs. The current embellish template is saved, the assigned template
    is set, the images are embellished and exported, and the original template is
    re-established or the exported folder is opened, with the sort order set to last
    modified in reverse.

    In Embel it is important to call setRemote(true). If Embel::isRemote == false then
    the datamodel is expected to be loaded and up-to-date. For remote situations, the
    data model may not be loaded, and Embel will call Metadata to load the image data.

    Returns at once; the temp files are removed and the template reset in finish, before
    finished carries the embellished paths.
*/
    QString srcFun = "EmbelExport::exportRemoteFiles";
    if (G::isLogger) G::log(srcFun);

    // set global isRemote flag
    G::isRemote = true;

    // set remote flag to avoid mainwindow updates
    embellish->setRemote(true);
    embelProperties->setRemote(true);
    // set the embellish template, which updates all the parameters
    embelProperties->setCurrentTemplate(templateName);
    // make sure export folder has been set
    if (!isValidExportFolder() || pathList.isEmpty()) {
        G::isRemote = false;
        return false;
    }

    if (G::embelLog) G::log(srcFun, "First file =" + pathList.at(0));

    remoteTempFiles = pathList;
    if (!exportImages(pathList, true)) {
        remoteTempFiles.clear();
        G::isRemote = false;
        return false;
    }
    return true;
}

bool EmbelExport::exportImages(const QStringList &srcList, bool isRemote)
{
/*
    Start embellishing the images in srcList with the current embellish template.  Returns
    false, having said why, when there is nothing to do; otherwise the export runs from
    the event loop and emits finished.

    isRemote only skips the destination folder dialog.  Whether the SOURCE is remote --
    a winnet's temp files, no datamodel -- is embellish->isRemote, set by
    exportRemoteFiles.
*/
    QString srcFun = "EmbelExport::exportImages";
    if (G::isLogger) G::log(srcFun);

    if (running) return false;
    if (srcList.isEmpty()) {
        G::popup->showPopup("No images picked or selected");
        return false;
    }

    if (embelProperties->templateName == "Do not Embellish") {
        G::popup->showPopup("The current embellish template is 'Do not Embellish'<p>"
                            "Please select an embellish template and try again.<p><hr>"
                            "Press <font color=\"red\"><b>Esc</b></font> to continue.",
                            0);
        return false;
    }

    exportFolderIfNotSubfolder = embelProperties->exportFolderPath;
    // Set export folder if not remote, not a subfolder and no export folder set
    if (!isRemote && embelProperties->saveMethod == "Another folder somewhere else") {
        if (exportFolderIfNotSubfolder == "") {
            QString msg = "Select export destination folder";
            exportFolderIfNotSubfolder = QFileDialog::getExistingDirectory(this, msg,
                "/home", QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
            if (exportFolderIfNotSubfolder == "") return false;
        }
    }

    this->srcList = srcList;
    dstPaths.clear();
    reservedPaths.clear();
    index = 0;
    pendingWrites = 0;
    waitingForWriter = false;
    abort = false;
    remote = embellish->isRemote;
    running = true;

    /*  A remote export runs at startup, before any folder is shown: nothing may load
        into the loupe underneath it (ImageView::loadImage).  A local one leaves the
        user free to browse. */
    if (remote) G::isProcessingExportedImages = true;

    writerThread = new QThread;
    writerThread->setObjectName("EmbelWriter");
    writer = new EmbelWriter;
    writer->moveToThread(writerThread);
    connect(writerThread, &QThread::finished, writer, &QObject::deleteLater);
    connect(writer, &EmbelWriter::written, this, &EmbelExport::onWritten);
    writerThread->start();

    QString folderPath = QFileInfo(srcList.at(0)).dir().path();
    G::popup->setProgressVisible(true);
    G::popup->setProgressMax(srcList.size());
    QString txt = "Exporting " + QString::number(srcList.size()) +
                  " embellished images to " + folderPath +
                  "<p>Press <font color=\"red\"><b>Esc</b></font> to abort.";
    G::popup->showPopup(txt, 0, true, 1);

    QTimer::singleShot(0, this, &EmbelExport::startNext);
    return true;
}

void EmbelExport::startNext()
{
/*
    Render the next image, or finish once every image has been rendered and written.
    The render waits while two images are with the writer: a remote export renders far
    faster than ExifTool writes, and each one queued is a full-size image held.
*/
    if (!running) return;
    if (abort || index >= srcList.size()) {
        if (pendingWrites == 0) finish();
        return;
    }
    if (pendingWrites >= 2) {
        waitingForWriter = true;
        return;
    }

    G::popup->setProgress(index + 1);
    const QString fPath = srcList.at(index);

    /*  The develop render first: the image cache holds the EMBEDDED PREVIEW of a raw
        while the user browses (3200x2400 for an OM-1, against a 5184x3888 sensor), and
        nothing in it carries the develop recipe.  `done` may arrive on a worker thread,
        and after this export has been deleted (a quit): it is delivered through qApp,
        which outlives both, and checked against the QPointer on the GUI thread. */
    if (pixelSource && !remote) {
        QPointer<EmbelExport> self(this);
        pixelSource(fPath, [self](bool ok, const QImage &img) {
            QMetaObject::invokeMethod(qApp, [self, ok, img]() {
                if (self) self->onPixels(ok, img);
            }, Qt::QueuedConnection);
        });
        return;
    }
    onPixels(false, QImage());
}

void EmbelExport::onPixels(bool ok, const QImage &developed)
{
/*
    The develop render for srcList[index] has landed (or there is none).  Fall back to
    the image cache, else the file, and compose.
*/
    QString srcFun = "EmbelExport::onPixels";
    if (!running) return;
    if (abort) { startNext(); return; }

    const QString fPath = srcList.at(index);
    if (ok && !developed.isNull()) {
        if (G::embelLog) {
            G::log(srcFun, QString("%1x%2").arg(developed.width()).arg(developed.height()));
        }
        pmItem->setPixmap(QPixmap::fromImage(developed));
        compose(fPath);
        return;
    }
    if (pixelSource && !remote && G::embelLog)
        G::log(srcFun, "Develop render failed, fallback: " + fPath);

    if (loadImage(fPath)) {
        compose(fPath);
        return;
    }
    if (G::embelLog) G::log(srcFun, "Failed to load Image " + fPath);
    ++index;
    QTimer::singleShot(0, this, &EmbelExport::startNext);
}

void EmbelExport::compose(const QString &fPath)
{
/*
    Embellish the image on pmItem, render the scene to a QImage and hand it to the
    writer.  The scene is GUI-thread only, so this is the one step that stays here.
*/
    QString srcFun = "EmbelExport::compose";
    if (G::isLogger) G::log(srcFun);

    embellish->build(fPath, srcFun);
    setSceneRect(scene->itemsBoundingRect());

    // Create QImage with the exact size of the scene
    QImage image(scene->sceneRect().size().toSize(), QImage::Format_ARGB32);
    // Start all pixels transparent
    image.fill(Qt::transparent);
    // Copy the scene to the QImage
    QPainter painter(&image);
    scene->render(&painter);
    painter.end();
    pmItem->setPixmap(QPixmap());       // release the full-size source

    const QString extension = embelProperties->exportFileType;
    const QString baseName = QFileInfo(fPath).baseName() + embelProperties->exportSuffix;
    const QString exportFolder = embelProperties->saveMethod == "Subfolder"
                                 ? exportSubfolderPath(fPath)
                                 : exportFolderIfNotSubfolder;

    /*  A destination name is decided here, before the file exists: the writer may still
        be saving an earlier image of the same name, which a check of the disk alone
        would not see.  So the batch's own names are reserved too.  Overwrite replaces an
        old file on disk, never another image of this batch. */
    QString dst = exportFolder + "/" + baseName + "." + extension;
    int n = 0;
    while (reservedPaths.contains(dst) ||
           (!embelProperties->overwriteFiles && QFile::exists(dst))) {
        dst = exportFolder + "/" + baseName + "_" + QString::number(++n) + "." + extension;
    }
    reservedPaths.insert(dst);
    const QString dstThumb = dst.left(dst.length() - extension.length() - 1) +
                             "_thumb." + extension;

    if (G::embelLog) G::log(srcFun, "src = " + fPath + " dst = " + dst);

    EmbelWriter *w = writer;
    QMetaObject::invokeMethod(w, [w, image, fPath, dst, dstThumb, extension]() {
        w->write(image, fPath, dst, dstThumb, extension);
    }, Qt::QueuedConnection);
    ++pendingWrites;

    ++index;
    QTimer::singleShot(0, this, &EmbelExport::startNext);
}

void EmbelExport::onWritten(bool ok, const QString &dst)
{
    if (!running) return;
    --pendingWrites;
    if (ok) {
        dstPaths << dst;
        FileOps::onCreated({dst});
    }
    if (waitingForWriter || abort || index >= srcList.size()) {
        waitingForWriter = false;
        startNext();
    }
}

void EmbelExport::finish()
{
    QString srcFun = "EmbelExport::finish";
    if (G::isLogger) G::log(srcFun);

    running = false;
    writerThread->quit();               // the writer (and its ExifTool) close on it

    G::popup->setProgressVisible(false);
    G::popup->reset();
    if (abort) G::popup->showPopup("Export has been aborted", 1500);

    if (remote) {
        G::isProcessingExportedImages = false;
        // remove the temp image files used to create the embellished version
        for (const QString &path : std::as_const(remoteTempFiles)) {
            if (QFile(path).remove()) FileOps::onDeleted(path);
        }
        remoteTempFiles.clear();
        // clear embellish
        embelProperties->setCurrentTemplate("Do not Embellish");
        G::isRemote = false;
    }

    if (G::embelLog)
        G::log(srcFun, "Completed. " + QString::number(dstPaths.size()) + " exported");
    emit finished(dstPaths, abort);
}

bool EmbelExport::loadImage(QString fPath)
{
/*
    The fallback when there is no develop render: the image cache, else the file decoded
    by Qt (color managed on Windows).  Puts the image on pmItem.

    If Winnow has been launched remotely via a winnet then no folder has been loaded so
    no point in looking for an existing QImage or loading metadata.  Nor for an image
    that left the datamodel during the export (a folder change).
*/
    QString srcFun = "EmbelExport::loadImage";
    if (G::isLogger) G::log(srcFun);

    int dmRow = dm->rowFromKey(fPath);

    if (!remote && dmRow >= 0) {
        /*  One locked lookup, and a null image falls through to the decode below rather
            than exporting a blank frame.  See ImageCacheData::get. */
        QImage cached;
        if (icd->get(fPath, cached) && !cached.isNull()) {
            pmItem->setPixmap(QPixmap::fromImage(cached));
            return true;
        }
        // check metadata loaded for image
        if (dm->index(dmRow, G::MetadataStatusColumn).data().toInt() != G::MetaLoaded) {
            QFileInfo fileInfo(fPath);
            if (metadata->loadImageMetadata(fileInfo, dmRow, dm->instance, true, true, false, true, "EmbelExport::")) {
                dm->addMetadataForItem(metadata->m, "EmbelExport::loadImage");
            }
            else {
                return false;
            }
        }
    }
    // load QImage
    QImage im(fPath);
    // confirm load was successful
    if (im.isNull()) {
        return false;
    }
    // color manage if available
    #ifdef Q_OS_WIN
    QString ext = metadata->m.type.toLower();
    if (metadata->iccFormats.contains(ext)) {
        QByteArray ba = dm->index(dmRow, G::ICCBufColumn).data().toByteArray();
        ICC::transform(ba, im);
    }
    #endif
    // convert QImage to QGraphicsPixmapItem
    pmItem->setPixmap(QPixmap::fromImage(im));
    return true;
}

void EmbelExport::abortEmbelExport()
{
/*
    Esc (MW::keyReleaseEvent) or a quit.  The image being rendered and any with the
    writer complete; nothing after them starts.
*/
    if (!running) return;
    abort = true;
}
