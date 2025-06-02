#include "embelexport.h"
#include "Main/global.h"

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

    Local exports call EmbelExport::exportImages.

    EmbelExport::exportImages cycles through the list of images to be processed, reporting the
    progress and calling EmbelExport::exportImage for each image.

    In EmbelExport::exportImage the image file is read and embellished. The resulting graphics
    scene is copied to a new QImage and saved to the assigned image format (jpg, png or tif).
    The metadata is copied from the source image file to the exported image file. Finally, the
    ICC color space is updated.
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
    if (G::isLogger) G::log("EmbelExport::~EmbelExport");
//    delete embellish;
//    delete scene;
//    delete pmItem;
}

bool EmbelExport::loadImage(QString fPath)
{
/*
    Read the image file, extract metadata, apply ICC profile and convert to
    QGraphicsPixmapItem.

    If Winnow has been launched remotely via a winnet then no folder has been loaded so
    no point in looking for an existing QImage or loading metadata.
*/
    if (G::isLogger) G::log("EmbelExport::loadImage");
    QImage image;
    int dmRow = dm->rowFromPath(fPath);

    QString msg = "embellish->isRemote = " + QVariant(embellish->isRemote).toString();
    if (G::isFileLogger) Utilities::log("EmbelExport::loadImage", msg);

    if (!embellish->isRemote) {
        if (icd->contains(fPath)) {
            pmItem->setPixmap(QPixmap::fromImage(icd->imCache.value(fPath)));
            return true;
        }
        // check metadata loaded for image
        if (!dm->index(dmRow, G::MetadataLoadedColumn).data().toBool()) {
            QFileInfo fileInfo(fPath);
            if (metadata->loadImageMetadata(fileInfo, dm->instance, true, true, false, true, "EmbelExport::")) {
                metadata->m.row = dmRow;
                metadata->m.instance = dm->instance;
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
//        ICC::setInProfile(ba);   // if ba.isEmpty then sRGB used
        ICC::transform(ba, im);
    }
    #endif
    // convert QImage to QGraphicsPixmapItem
    pmItem->setPixmap(QPixmap::fromImage(im));

    return true;
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
    if (G::isLogger) G::log("EmbelExport::isValidExportFolder");
    if (embelProperties->saveMethod == "Subfolder") {
        if (embelProperties->exportSubfolder == "") {
            if (G::isFileLogger) Utilities::log("EmbelExport::isValidExportFolder", "Embellish export subfolder not defined");
            return false;
        }
    }
    else {
        if (embelProperties->exportFolderPath == "") {
            if (G::isFileLogger) Utilities::log("EmbelExport::isValidExportFolder", "Embellish export folder not defined");
            return false;
        }
    }
    return true;
}

QStringList EmbelExport::exportRemoteFiles(QString templateName, QStringList &pathList)
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
*/
    if (G::isLogger) G::log("EmbelExport::exportRemoteFiles");

    // set global isRemote flag
    G::isRemote = true;

    // clear dstPaths
    dstPaths.clear();
    // set remote flag to avoid mainwindow updates
    embellish->setRemote(true);
    embelProperties->setRemote(true);
    // save current embellish template
    QString prevTemplate = embelProperties->templateName;
    // set the embellish template, which updates all the parameters
    embelProperties->setCurrentTemplate(templateName);
    // make sure export folder has been set
    if (!isValidExportFolder()) {
        return dstPaths;
    }

    //QMessageBox::information(this, "EmbelExport::exportRemoteFiles", pathList.at(0));
    if (G::isFileLogger) Utilities::log("EmbelExport::exportRemoteFiles  First file =", pathList.at(0));

    exportImages(pathList, true);

    // remove the temp image files used to create the embellished version
    for (int i = 0; i < pathList.size(); i++) {
        QFile(pathList.at(i)).remove();
    }

    // clear embellish
    embelProperties->setCurrentTemplate("Do not Embellish");

    if (G::isFileLogger) Utilities::log("EmbelExport::exportImages completed", "lastExportedPath = " + lastFileExportedPath);

    // set global isRemote flag
    G::isRemote = false;

    return dstPaths;
}

void EmbelExport::exportImages(const QStringList &srcList, bool isRemote)
{
/*
    Each image in the list is rendered to the assigned embellish template. The metadata is
    copied from the source image file to the exported image file. Finally, the ICC color space
    is updated.
*/
    if (G::isLogger) G::log("EmbelExport::exportImages");

    G::isProcessingExportedImages = true;
    abort = false;
    int count = srcList.size();
    if (count == 0) {
        G::popUp->showPopup("No images picked or selected");
        return;
    }

    if (embelProperties->templateName == "Do not Embellish") {
        G::popUp->showPopup("The current embellish template is 'Do not Embellish'<p>"
                            "Please select an embellish template and try again.<p><hr>"
                            "Press <font color=\"red\"><b>Spacebar</b></font> to continue.",
                            0);
        return;
    }

    exportFolderIfNotSubfolder = embelProperties->exportFolderPath;
    // Set export folder if not remote, not a subfolder and no export folder set
    if (!isRemote && embelProperties->saveMethod == "Another folder somewhere else") {
        if (exportFolderIfNotSubfolder == "") {
            QString msg = "Select export destination folder";
            exportFolderIfNotSubfolder = QFileDialog::getExistingDirectory(this, msg,
                "/home", QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
            if (exportFolderIfNotSubfolder == "") {
                return;
            }
        }
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

    // iterate list of files, embellish and transfer metadata, ICC and insert thumbnail
    ExifTool et;
    et.setOverWrite(true);
    for (int i = 0; i < count; i++) {
        G::popUp->setProgress(i+1);
        if (G::useProcessEvents) qApp->processEvents();
        if (abort) break;
        QString src = srcList.at(i);
        // embellish src image
        if (exportImage(src)) {
            QString dst = lastFileExportedPath;
            QString dstThumb = lastFileExportedThumbPath;
            thumbList << dstThumb;
            // copy all metadata tags from src to dst
            et.copyAllTags(src, dst);
            // copy ICC from src to dst
            et.copyICC(src, dst);
            // add thumbnail to dst
            et.addThumb(dstThumb, dst);

            // qDebug() << "EmbelExport::exportImages" << i << dst;
            dstPaths << dst;

            QString msg = "ExifTool copied tags, ICC and thumbnail to embellished image";
            if (G::isFileLogger) Utilities::log("EmbelExport::exportImages", msg);
        }
    }
    et.close();
    exportingEmbellishedImages = false;

    // delete the thumbnail files
    if (G::isFileLogger) Utilities::log("EmbelExport::exportImages", "Delete thumbnail files");
    for (int i = 0; i < thumbList.length(); ++i) {
        QFile::remove(thumbList.at(i));
    }

    G::popUp->setProgressVisible(false);
    G::popUp->reset();
    G::isProcessingExportedImages = false;
    if (G::isFileLogger) Utilities::log("EmbelExport::exportImages", "Delete embellish");
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
                  "Press <font color=\"red\"><b>Spacebar</b></font> to continue";
    G::popUp->showPopup(msg, 2000);
    //*/
}

bool EmbelExport::exportImage(const QString &fPath)
{
/*
    The image file is read and embellished.  The resulting graphics scene is copied to a new
    QImage and saved to the assigned image format (jpg, png or tif).  A thumbnail is created
    and saved to ":/thumb.jpg" in the export folder.
*/
    if (G::isLogger) G::log("EmbelExport::exportImage");

    QString extension = embelProperties->exportFileType;
    QFileInfo fileInfo(fPath);
    QString baseName = fileInfo.baseName() + embelProperties->exportSuffix;
    QString exportFolder;
    if (embelProperties->saveMethod == "Subfolder") {
       exportFolder = exportSubfolderPath(fPath);
    }
    else {
        exportFolder = exportFolderIfNotSubfolder;
    }
    QString exportPath = exportFolder + "/" + baseName + "." + extension;
    QString exportThumbPath = exportFolder + "/" + baseName + "_thumb." + extension;

    // Check if destination image file already exists
    if (!embelProperties->overwriteFiles) Utilities::uniqueFilePath(exportPath);

    QString msg = "src = " + fPath + " dst = " + exportPath;
    if (G::isFileLogger) Utilities::log("EmbelExport::exportImage", msg);

    // read the image, add it to the graphics scene and embellish
    if (loadImage(fPath)) {
        // embellish
        embellish->build(fPath, "EmbelExport::exportImage");
        setSceneRect(scene->itemsBoundingRect());
    }
    else {
        msg = "Failed to load Image " + fPath;
        if (G::isFileLogger) Utilities::log("EmbelExport::exportImage", msg);
        return false;
    }

    // Create QImage with the exact size of the scene
    QImage image(scene->sceneRect().size().toSize(), QImage::Format_ARGB32);
    // Start all pixels transparent
    image.fill(Qt::transparent);
    // Copy the scene to the QImage
    QPainter painter(&image);
    scene->render(&painter);

    bool wasSaved = false;

    // save
    if (extension == "JPG") wasSaved = image.save(exportPath, "JPG", 100);
    if (extension == "PNG") wasSaved = image.save(exportPath, "PNG", 100);
    if (extension == "TIF") wasSaved = image.save(exportPath, "TIF");

    msg = exportPath
            + (wasSaved ? " saved" : " failed")
            + " Image width: " + QString::number(image.width());
    if (G::isFileLogger) Utilities::log("EmbelExport::exportImage", msg);

    if (wasSaved) {
        // add thumbnail
        QImage thumb = image.scaled(160, 160, Qt::KeepAspectRatio);
        lastFileExportedThumbPath = exportThumbPath;
        thumb.save(lastFileExportedThumbPath, "JPG", 60);
    //    thumb.save(":/thumb.jpg", "JPG", 60);

        lastFileExportedPath = exportPath;
    }

//    QFile f(exportPath);
//    f.close();

    return wasSaved;
}

void EmbelExport::abortEmbelExport()
{
    // if (G::useProcessEvents) qApp->processEvents();
    G::isProcessingExportedImages = false;
    G::popUp->showPopup("Export has been aborted", 1500);
    qDebug() << "EmbelExport::" << "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX";
    abort = true;
}
