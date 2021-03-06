#include "embelexport.h"

EmbelExport::EmbelExport(Metadata *metadata,
                         DataModel *dm,
                         ImageCache *imageCacheThread,
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

    Local exports call EmbelExport::exportImages.

    EmbelExport::exportImages cycles through the list of images to be processed, reporting the
    progress and calling EmbelExport::exportImage for each image.

    In EmbelExport::exportImage the image file is read and embellished. The resulting graphics
    scene is copied to a new QImage and saved to the assigned image format (jpg, png or tif).
    The metadata is copied from the source image file to the exported image file. Finally, the
    ICC color space is updated.
*/
    if (G::isLogger) G::log(__FUNCTION__); 
    this->metadata = metadata;
    this->dm = dm;
    this->imageCacheThread = imageCacheThread;
    this->embelProperties = embelProperties;

    scene = new QGraphicsScene();
    pmItem = new QGraphicsPixmapItem;
    scene->addItem(pmItem);
    setScene(scene);

    embellish = new Embel(scene, pmItem, embelProperties, imageCacheThread, "Export");
}

EmbelExport::~EmbelExport()
{
    if (G::isLogger) G::log(__FUNCTION__); 
//    delete embellish;
//    delete scene;
//    delete pmItem;
}

bool EmbelExport::loadImage(QString fPath)
{
/*
    Read the image file, extract metadata, apply ICC profile and convert to
    QGraphicsPixmapItem.
*/
    if (G::isLogger) G::log(__FUNCTION__); 
    if (imageCacheThread->imCache.contains(fPath)) {
        pmItem->setPixmap(QPixmap::fromImage(imageCacheThread->imCache.value(fPath)));
        return true;
    }
    // check metadata loaded for image
    int dmRow = dm->fPathRow[fPath];
    if (!dm->index(dmRow, G::MetadataLoadedColumn).data().toBool()) {
        QFileInfo fileInfo(fPath);
        if (metadata->loadImageMetadata(fileInfo, true, true, false, true, __FUNCTION__)) {
            metadata->m.row = dmRow;
            dm->addMetadataForItem(metadata->m);
        }
        else return false;
    }
    // load QImage
    QImage im(fPath);
    // color manage if available
    #ifdef Q_OS_WIN
    QString ext = metadata->m.type.toLower();
    if (metadata->iccFormats.contains(ext)) {
        QByteArray ba = dm->index(dmRow, G::ICCBufColumn).data().toByteArray();
//        ICC::setInProfile(ba);   // if ba.isEmpty then sRGB used
        ICC::transform(ba, im);
    }
    #endif
    // convert QImage to QGraphicsPixmapItem
    pmItem->setPixmap(QPixmap::fromImage(im));

    return true;
}

QString EmbelExport::exportFolderPath(QString fPath)
{
/*
    The incoming fPath is the file path for one of the images being exported.  Its folder is
    fromFolderPath.  If the export folder save method == "Subfolder", then the subfolder name
    is used, otherwise, all this is ignored and embelProperties->exportSubfolder is used.
*/
    if (G::isLogger) G::log(__FUNCTION__); 
    QFileInfo filedir(fPath);
    QString fromFolderPath = filedir.dir().path();
    QString exportFolder;
    if (embelProperties->saveMethod == "Subfolder") {
        exportFolder = fromFolderPath + "/" + embelProperties->exportSubfolder;
        QDir dir(fromFolderPath);
        dir.mkdir(embelProperties->exportSubfolder);
    }
    else exportFolder = embelProperties->exportFolderPath;

    return exportFolder;
}

QString EmbelExport::exportRemoteFiles(QString templateName, QStringList &pathList)
{
/*
    Images sent from another program, such as lightroom, are sent here from
    MW::handleStartupArgs.  The current embellish template is saved, the assigned
    template is set, the images are embellished and exported, and the original
    template is re-established, or the exported folder is opened, with the sort order
    set to last modified in reverse.

    In Embel it is important to call setRemote(true).  If Embel::isRemote == false then the
    datamodel is expected to be loaded and up-to-date.  For remote situations, the data model
    may not be loaded, and Embel will call Metadata to load the image data.
*/
    if (G::isLogger) G::log(__FUNCTION__); 

    // save current embellish template
    QString prevTemplate = embelProperties->templateName;
    // set the embellish template, which updates all the parameters
    embelProperties->setCurrentTemplate(templateName);
    embellish->setRemote(true);

    exportImages(pathList);

    embellish->setRemote(false);

    int n = pathList.length() - 1;
    QString fPath = pathList.at(n);
    QString exportFolder = exportFolderPath(fPath);
    QFileInfo info(fPath);
    QString exportPathToLastFile = exportFolder + "/" + info.fileName();

    return lastFileExportedPath;
}

void EmbelExport::exportImages(const QStringList &srcList)
{
/*
    Each image in the list is rendered to the assigned embellish template. The metadata is
    copied from the source image file to the exported image file. Finally, the ICC color space
    is updated.
*/
    if (G::isLogger) G::log(__FUNCTION__); 

    G::isProcessingExportedImages = true;
    abort = false;
    int count = srcList.size();
    if (count == 0) {
        G::popUp->showPopup("No images picked or selected");
        return;
    }
    qDebug() << __FUNCTION__ << embelProperties->templateName;
    if (embelProperties->templateName == "Do not Embellish") {
        G::popUp->showPopup("The current embellish template is 'Do not Embellish'<p>"
                            "Please select an embellish template and try again.<p><hr>"
                            "Press <font color=\"red\"><b>Esc</b></font> to continue.",
                            0);
        return;
    }

    QElapsedTimer t;
    t.start();

    QFileInfo filedir(srcList.at(0));
    QString folderPath = filedir.dir().path();
    QStringList thumbList;

    G::popUp->setProgressVisible(true);
    G::popUp->setProgressMax(count);
    QString txt = "Exporting " + QString::number(count) +
                  " embellished images to " + folderPath +
                  "<p>Press <font color=\"red\"><b>Esc</b></font> to abort.";
    G::popUp->showPopup(txt, 0, true, 1);
    exportingEmbellishedImages = true;

    ExifTool et;
    et.overwrite();
//    QStringList dstList;
    for (int i = 0; i < count; i++) {
        G::popUp->setProgress(i+1);
        qApp->processEvents();
        if (abort) break;
        QString src = srcList.at(i);
        // embellish src image
        exportImage(src);
        QString dst = lastFileExportedPath;
        QString dstThumb = lastFileExportedThumbPath;
        thumbList << dstThumb;
        // copy all metadata tags from src to dst
        et.copyAllTags(src, dst);
        // copy ICC from src to dst
        et.copyICC(src, dst);
        // add thumbnail to dst
        et.addThumb(dstThumb, dst);
    }
    et.close();
    exportingEmbellishedImages = false;

    // delete the thumbnail files
    for (int i = 0; i < thumbList.length(); ++i) {
        QFile::remove(thumbList.at(i));
    }

    G::popUp->setProgressVisible(false);
    G::popUp->hide();
    G::isProcessingExportedImages = false;
    delete embellish;

    if (abort) {
        abort = false;
        G::isProcessingExportedImages = false;
        G::popUp->showPopup("Export has been aborted", 1500);
        return;
    }
    /*
    qint64 ms = t.elapsed();
    QString _ms = QString("%L1").arg(ms);
    double sec = ms * 1.0 / 1000;
    QString _sec = QString::number(sec, 'g', 2);
    QString second;
    sec > 1 ? second = " seconds." : second = " second.";
    int msperim = static_cast<int>(ms * 1.0 / count);
    QString _msperim = QString("%L1").arg(msperim);
    QString msg = "Rendered and exported " +
                  QString::number(count) + " images.<p>"
                  "Elapsed time " + _sec + second + "<p>" +
                  _msperim + " milliseconds per image.<p>" +
                  "<hr>" +
                  "Press <font color=\"red\"><b>Esc</b></font> to continue";
    G::popUp->showPopup(msg, 2000);
    //*/
}

void EmbelExport::exportImage(const QString &fPath)
{
/*
    The image file is read and embellished.  The resulting graphics scene is copied to a new
    QImage and saved to the assigned image format (jpg, png or tif).  A thumbnail is created
    and saved to ":/thumb.jpg" in the export folder.
*/
    if (G::isLogger) G::log(__FUNCTION__); 
    QString extension = embelProperties->exportFileType;
    QFileInfo fileInfo(fPath);
    QString baseName = fileInfo.baseName() + embelProperties->exportSuffix;
    QString exportFolder = exportFolderPath(fPath);
    qDebug() << __FUNCTION__ << fPath << exportFolder;
    QString exportPath = exportFolder + "/" + baseName + "." + extension;
    QString exportThumbPath = exportFolder + "/" + baseName + "_thumb." + extension;

    // Check if destination image file already exists
    if (!embelProperties->overwriteFiles) Utilities::uniqueFilePath(exportPath);

    // read the image, add it to the graphics scene and embellish
    if (loadImage(fPath)) {
        // embellish
        embellish->build(fPath, __FUNCTION__);
        setSceneRect(scene->itemsBoundingRect());
    }

    // Create QImage with the exact size of the scene
    QImage image(scene->sceneRect().size().toSize(), QImage::Format_ARGB32);
    // Start all pixels transparent
    image.fill(Qt::transparent);
    // Copy the scene to the QImage
    QPainter painter(&image);
    scene->render(&painter);

    // save
    if (extension == "JPG") image.save(exportPath, "JPG", 100);
    if (extension == "PNG") image.save(exportPath, "PNG", 100);
    if (extension == "TIF") image.save(exportPath, "TIF");

    // add thumbnail
    QImage thumb = image.scaled(160, 160, Qt::KeepAspectRatio);
    lastFileExportedThumbPath = exportThumbPath;
    thumb.save(lastFileExportedThumbPath, "JPG", 60);
//    thumb.save(":/thumb.jpg", "JPG", 60);

    lastFileExportedPath = exportPath;
}

void EmbelExport::abortEmbelExport()
{
    qApp->processEvents();
    G::isProcessingExportedImages = false;
    G::popUp->showPopup("Export has been aborted", 1500);
    qDebug() << __FUNCTION__ << "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX";
    abort = true;
}
