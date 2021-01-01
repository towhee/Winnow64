#include "embelexport.h"

EmbelExport::EmbelExport(Metadata *metadata,
                         DataModel *dm,
                         ImageCache *imageCacheThread,
                         EmbelProperties *embelProperties,
                         QWidget *parent)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    this->metadata = metadata;
    this->dm = dm;
    this->imageCacheThread = imageCacheThread;
    this->embelProperties = embelProperties;

    scene = new QGraphicsScene();
    pmItem = new QGraphicsPixmapItem;
    scene->addItem(pmItem);
    setScene(scene);

    embellish = new Embel(scene, pmItem, embelProperties, imageCacheThread);
}

EmbelExport::~EmbelExport()
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
//    delete embellish;
//    delete scene;
//    delete pmItem;
}

bool EmbelExport::loadImage(QString fPath)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
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
    pmItem->setPixmap(QPixmap(fPath));
    return true;
}

QString EmbelExport::exportFolderPath(QString fPath)
{
/*
    The incoming fPath is the file path for one of the images being exported.  Its folder is
    fromFolderPath.  If the export folder save method == Subfolder, then the subfolder name
    is used, otherwise, all this is ignored then embelProperties->exportSubfolder is used.
*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
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
*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
//    Utilities::log(__FUNCTION__, fPath);

    // save current embellish template
    QString prevTemplate = embelProperties->templateName;
    // set the embellish template, which updates all the parameters
    embelProperties->setCurrentTemplate(templateName);

    // export the file(s)
    for (int i = 0; i < pathList.length(); i++) {
        exportImage(pathList.at(i));
    }

    int n = pathList.length() - 1;
    QString fPath = pathList.at(n);
    QString exportFolder = exportFolderPath(fPath);

    // cleanup (open export folder??)
//    embelProperties->setCurrentTemplate(prevTemplate);
    delete embellish;

//    qDebug() << __FUNCTION__
//             << "exportFolderPath(fPath) ="
//             << exportFolderPath(fPath);
//    Utilities::log(__FUNCTION__, "fPath =" + fPath);
//    Utilities::log(__FUNCTION__, "exportFolderPath(fPath) =" + exportFolderPath(fPath));
    return exportFolderPath(fPath);
}

void EmbelExport::exportImages(const QStringList &fPathList)
{
/*
    Images within the current filtration that are picked or selected are rendered with
    the current embellish template and saved to the template output folder. This is called
    from MW::exportEmbel.
*/
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    int count = fPathList.size();
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

    QFileInfo filedir(fPathList.at(0));
    QString folderPath = filedir.dir().path();

    G::popUp->setProgressVisible(true);
    G::popUp->setProgressMax(count);
    QString txt = "Exporting " + QString::number(count) + " embellished images to "
            + folderPath;
    G::popUp->showPopup(txt, 0, true, 1);

    for (int i=0; i < count; i++) {
        exportImage(fPathList.at(i));
        G::popUp->setProgress(i);
    }

    G::popUp->setProgressVisible(false);
    G::popUp->hide();
    delete embellish;

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
    G::popUp->showPopup(msg, 0);
}

void EmbelExport::exportImage(const QString &fPath)
{
    QString extension = embelProperties->exportFileType;
    QFileInfo fileInfo(fPath);
    QString baseName = fileInfo.baseName() + embelProperties->exportSuffix;
    QString exportFolder = exportFolderPath(fPath);
    qDebug() << __FUNCTION__ << fPath << exportFolder;
    QString exportPath = exportFolder + "/" + baseName + "." + extension;

    // Check if destination image file already exists
    if (!embelProperties->overwriteFiles) {
        int count = 0;
        bool fileAlreadyExists = true;
        do {
            QFile testFile(exportPath);
            if (testFile.exists()) {
                exportPath = exportFolder + "/" + baseName + "_" + QString::number(++count) + "." + extension;
            }
            else fileAlreadyExists = false;
        } while (fileAlreadyExists);
    }

    // read the image, add it to the scene and embellish
    if (loadImage(fPath)) {
        // embellish
        embellish->build(fPath, __FUNCTION__);
        setSceneRect(scene->itemsBoundingRect());
    }

    // create image to show rendering with embellish
    QImage image(scene->sceneRect().size().toSize(), QImage::Format_ARGB32);  // Create the image with the exact size of the shrunk scene
    image.fill(Qt::transparent);                                              // Start all pixels transparent
    QPainter painter(&image);
    scene->render(&painter);

    // save
    if (extension == "JPG") image.save(exportPath, "JPG", 100);
    if (extension == "PNG") image.save(exportPath, "PNG", 100);
    if (extension == "TIF") image.save(exportPath, "TIF");
}
