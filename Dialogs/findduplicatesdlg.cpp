#include "Dialogs/FindDuplicatesDlg.h"
#include "ui_findduplicatesdlg.h"
#include "Effects/effects.h"

/*******************************************************************************************/

DragToList::DragToList(QWidget *parent) :
    QListWidget(parent)
{
    setAcceptDrops(true);
    setDragEnabled(false);
    setDragDropMode(QAbstractItemView::DropOnly);
}

void DragToList::showEvent(QShowEvent *event)
{
/*
    These actions must be run after the dialog constructor is finished.
*/
    setStyleSheet(
        "color: yellow;"
        "background-image: url(:/images/dragfoldershere.png)"
        );
    event->accept();
}

void DragToList::dragEnterEvent(QDragEnterEvent *event)
{
    qDebug() << "DragToList::dragEnterEvent" << event;
    event->acceptProposedAction();
}

void DragToList::dragMoveEvent(QDragMoveEvent *event)
{
    if (event->mimeData()->hasUrls()) {  // Check if the drag event contains URLs
        event->acceptProposedAction();   // Accept the proposed action (copy or move)
    } else {
        event->ignore();  // Ignore the event if it doesn't contain URLs
    }
}

void DragToList::dropEvent(QDropEvent *event)
{
    qDebug() << "DragToList::dropEvent" << event;
    if (event->mimeData()->hasUrls()) {
        QList<QUrl> urls = event->mimeData()->urls();
        for (QList<QUrl>::Iterator i = urls.begin(); i != urls.end(); ++i) {
            QFileInfo fInfo = QFileInfo(i->toLocalFile());
            if (fInfo.isDir()) {
                QString path = i->toLocalFile(); // Get the full path of the file or folder
                addItem(path);
            }
        }
        event->acceptProposedAction();
        emit dropped();
    }
}

/*******************************************************************************************

Comparison terms:

    A or a refers to the items in dm->sf datamodel
    B or b refers to the items in ui->cmpToFolders
    delta[a][b] is the difference between image A and image B
    cmpToFolders is the list of all folders containing B images
    bList is the list of all B images

*/

FindDuplicatesDlg::FindDuplicatesDlg(QWidget *parent, DataModel *dm,
                                     Metadata *metadata, FrameDecoder *frameDecoder) :
    QDialog(parent),
    ui(new Ui::FindDuplicatesDlg),
    dm(dm),
    metadata(metadata),
    frameDecoder(frameDecoder)
{
    autonomousImage = new AutonomousImage(metadata, frameDecoder);
    connect(frameDecoder, &FrameDecoder::frameImage, this, &FindDuplicatesDlg::setImageFromVideoFrame);
    // add disconnect in destructor...

    int id = 0; // dummy variable req'd by ImageDecoder
    imageDecoder = new ImageDecoder(this, id, dm, metadata);

    ui->setupUi(this);
    setStyleSheet(G::css);

    // candidate images tableview
    ui->tv->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->tv->setEnabled(false);
    ui->tv->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft);
    ui->tv->horizontalHeader()->setStyleSheet(
        "QHeaderView::section {"
            "border-color:black;"
            "border-width:0px;"
            "border:none;"
            //"text-align: left;"
            //"outline-color:red;"
        "}");
    ui->tv->verticalHeader()->setStyleSheet(
        "QHeaderView::section {"
        "border:none;"
        "}");

    // Image comparison
    QImage prevIm(":/images/prev.png");
    QImage nextIm(":/images/next.png");
    ui->prevToolBtn->setIcon(QIcon(QPixmap::fromImage(prevIm.scaled(16,16))));
    ui->nextToolBtn->setIcon(QIcon(QPixmap::fromImage(nextIm.scaled(16,16))));
    ui->deltaLbl->setText("");
    ui->currentLbl->setText("");
    //ui->progressLbl->setText("");    //ui->currentLbl->setText("");
    ui->includeSubfoldersCB->setChecked(false);
    ui->sameFileTypeCB->setChecked(true);
    ui->sameCreationDateCB->setChecked(true);
    ui->sameAspectCB->setChecked(false);
    ui->sameDurationCB->setChecked(true);
    // set enabled states
    on_samePixelsCB_clicked();

    setupModel();

    #ifdef Q_OS_WIN
    Win::setTitleBarColor(winId(), G::backgroundColor);
    #endif

    abort = false;
    isDebug = false;
}

FindDuplicatesDlg::~FindDuplicatesDlg()
{
    delete ui;
}

void FindDuplicatesDlg::setupModel()
{
    model.setRowCount(dm->sf->rowCount());
    model.setColumnCount(4);
    model.setHorizontalHeaderItem(0, new QStandardItem("Dup"));
    model.setHorizontalHeaderItem(1, new QStandardItem("Delta"));
    model.setHorizontalHeaderItem(2, new QStandardItem(""));    // icon
    model.setHorizontalHeaderItem(3, new QStandardItem("File Name"));
//    model.setHorizontalHeaderItem(4, new QStandardItem("TargetId"));
//    model.setHorizontalHeaderItem(5, new QStandardItem("Match"));

    // populate model
    for (int a = 0; a < dm->sf->rowCount(); a++) {
        QVariant dupCount = 0;
        model.setData(model.index(a,0), 0);
        // add checkbox
        model.itemFromIndex(model.index(a,0))->setCheckable(true);
        // add pixmap
        QVariant var = dm->sf->index(a,0).data(Qt::DecorationRole);
        QIcon icon = var.value<QIcon>();
        QPixmap pm = icon.pixmap(icon.actualSize(QSize(256, 256))).scaled(48,48, Qt::KeepAspectRatio);
        model.setData(model.index(a,2), pm, Qt::DecorationRole);
        // add file name
        QString fName = dm->sf->index(a,G::NameColumn).data().toString();
        model.setData(model.index(a,3), fName);
    }

    // format tableview
    ui->tv->setModel(&model);
    QFontMetrics fm(ui->tv->fontMetrics());
    int w0 = fm.boundingRect("=Duplica=").width();
    int w1 = fm.boundingRect("=Delta=").width();
    int w2 = fm.boundingRect("=Icon=").width();
    //int w3 = fm.boundingRect("==========2022-09-27_0006.jpg==========").width();
    //int w4 = 0;
    //int w3 = ui->tv->width() - w0 - w1 - w2;
    ui->tv->setColumnWidth(0, w0);
    ui->tv->setColumnWidth(1, w1);
    ui->tv->setColumnWidth(2, w2);
    //ui->tv->setColumnWidth(3, w3);
    //ui->tv->setColumnWidth(4, w4);
    //ui->tv->setColumnHidden(4, true);
    ui->tv->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);

    // format

    isDebug = false;
}

void FindDuplicatesDlg::getPreview(QString fPath, QImage &image, QString source)
{
/*
    Get the preview image to display in the comparison view: either the candidate or
    target.  If it is a video, then get the first video frame.
*/
    QFileInfo fileInfo(fPath);
    ImageMetadata *m;
    if (metadata->loadImageMetadata(fileInfo, dm->instance, true, true, false, true, "FindDuplicatesDlg::preview")) {
        m = &metadata->m;
    }
    else {
        QString errMsg = "Failed to load metadata";
        if (G::isWarningLogger)
            qWarning() << "WARNING" << "DataModel::readMetadataForItem" << "Failed to load metadata for " << fPath;
        return;
    }
    if (m->video) {
        int longSide = 0;
        qDebug() << "FindDuplicatesDlg::getPreview"
                 << "longSide =" << longSide
                 << "source =" << source
                 << "fPath =" << fPath
            ;
        autonomousImage->image(fPath, image, longSide, source);
    }
    else {
        imageDecoder->decode(image, metadata, *m);
    }
}

void FindDuplicatesDlg::showPreview(QString path, QImage image, QString source)
{
    previewLongSide = ui->candidateLbl->width();
    if (image.width() > previewLongSide)
        previewSize = QSize(ui->candidateLbl->width(), ui->candidateLbl->height());
    else
        previewSize = QSize(image.width(), image.width());

    if (source == "FindDupCandidate") {
        pA = QPixmap::fromImage(image);
        ui->candidateLbl->setPixmap(pA.scaled(previewSize, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        ui->candidateLbl->setToolTip(path);
    }
    if (source == "FindDupMatch") {
        pB = QPixmap::fromImage(image);
        ui->matchLbl->setPixmap(pB.scaled(previewSize, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        ui->matchLbl->setToolTip(path);
    }
}

void FindDuplicatesDlg::setImageFromVideoFrame(QString path, QImage image, QString source)
{
/*
    Thumbnails and previews for videos are obtained from FrameDecoder, which signals here
    with the image.
*/
    int w = image.width();
    int h = image.height();
    qDebug() << path << "We did it!" << w << h << image << source;
    if (source == "BItemThumbnail") {
        bool foundItem = false;
        int b;
        for (b = 0; b < bItems.size(); b++) {
            if (bItems.at(b).fPath == path) {
                foundItem = true;
                break;
            }
        }
        if (foundItem) {
            bItems[b].im = image;
            qDebug() << path << "We found it!" << bItems[b].im;
        }
    }
    if (source == "FindDupCandidate") {
        showPreview(path, image, source);
    }
    if (source == "FindDupMatch") {
        showPreview(path, image, source);
    }
}

int FindDuplicatesDlg::reportRGB(QImage &im)
{
    int w = im.width();
    int h = im.height();
    qDebug() << "FindDuplicatesDlg::reportRGB:"
             << "w =" << w
             << "h =" << h
        ;
    return 0;
    for (int x = 0; x < w; x++) {
        for (int y = 0; y < h; y++) {
            QColor p = im.pixelColor(x,y);
            if (x < 15 && y < 1)
                qDebug() << "  " << x
                         << "\t red:" << p.red()
                         << "\t green:" << p.green()
                         << "\t blue:" << p.blue()
                    ;
        }
    }
}

double FindDuplicatesDlg::compareImagesHues(QImage &imA, QImage &imB)
{
    Effects effect;
    QVector<int> huesA(360, 0);
    QVector<int> huesB(360, 0);
    quint64 pixelsA = imA.width() * imA.height();
    quint64 pixelsB = imB.width() * imB.height();
    double deltaHue = 0;
    /*
    qDebug().noquote()
             << "FindDuplicatesDlg::visCmpImagesHues"
             << "A: w =" << imA.width() << "h =" << imA.height() << "pixelsA =" << pixelsA
             << "B: w =" << imB.width() << "h =" << imB.height() << "pixelsB =" << pixelsA
        ; //*/

    effect.hueCount(imA, huesA);
    effect.hueCount(imB, huesB);

    for (int hue = 0; hue < 360; hue++) {
        double diff = qAbs(huesA.at(hue) - huesB.at(hue));
        deltaHue += diff;
        /*
        qDebug().noquote()
                 << "FindDuplicatesDlg::visCmpImagesHues"
                 << QString::number(hue).leftJustified(4)
                 << "A =" << QString::number(huesA.at(hue)).leftJustified(4)
                 << "B =" << QString::number(huesB.at(hue)).leftJustified(4)
                 << "diff =" << diff
                 << "deltaHue =" << deltaHue
            ; //*/
    }

    // normalized deltaHue
    deltaHue = deltaHue / pixelsA * 100;
    /*
    qDebug().noquote()
        << "FindDuplicatesDlg::visCmpImagesHues"
        << "normalized deltaHue =" << deltaHue / pixelsA * 100
        ; //*/

    return deltaHue;
}

int FindDuplicatesDlg::compareRGB(QImage &imA, QImage &imB)
{
    int w, h;
    if (imA.width() < imB.width()) w = imA.width();
    else w = imB.width();
    if (imA.height() < imB.height()) h = imA.height();
    else h = imB.height();
    /*
    qDebug() << "FindDuplicatesDlg::compareRGB:"
             << "w =" << w
             << "h =" << h
             << "aW =" << imA.width()
             << "aH =" << imA.height()
             << "bW =" << imB.width()
             << "bH =" << imB.height()
        ;
    //*/
    int deltaRGB = 0;
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            QColor p1 = imA.pixelColor(x,y);
            QColor p2 = imB.pixelColor(x,y);
            deltaRGB += qAbs(p1.red() - p2.red());
            deltaRGB += qAbs(p1.green() - p2.green());
            deltaRGB += qAbs(p1.blue() - p2.blue());
            /*
            if (x < 5 && y < 2)
                qDebug() << "  x =" << x << "y =" << y
                     << "\t red:" << p1.red() << p2.red()
                     << "\t green:" << p1.green() << p2.green()
                     << "\t blue:" << p1.blue() << p2.blue()
                     << "delta =" << deltaRGB
                ;
            //*/
        }
    }
    /*
    qDebug() << "w =" << w
             << "h =" << h
             << "tot delta =" << deltaRGB
        ;
    //*/
    int delta = 1.0 * deltaRGB / (w * h * 3 * 256) * 100;
    return delta;
}

void FindDuplicatesDlg::pixelCompare()
{
/*

*/
    //clear();
    quint64 totIterations = dm->sf->rowCount() * bItems.count();
    ui->progressLbl->setText("Searching for duplicates in " + QString::number(totIterations) + " combinations");
    // iterate filtered datamodel
    int counter = 0;
    for (int a = 0; a < dm->sf->rowCount(); a++) {
        QString aPath = dm->sf->index(a,0).data(G::PathRole).toString();
        QString aFName = dm->sf->index(a,G::NameColumn).data().toString();
        /*
        qDebug() << "FindDuplicatesDlg::on_compareBtn_clicked"
                 << "a =" << a
                 << "aFile =" << aFPath;
        //*/

        // aList thumbnail
        QVariant var = dm->sf->index(a,0).data(Qt::DecorationRole);
        QIcon icon = var.value<QIcon>();
        QImage imA = icon.pixmap(icon.actualSize(QSize(256, 256))).toImage();

        // compare to each thumbnail in bList
        for (int b = 0; b < bItems.size(); b++) {
            if (abort) {
                ui->progressLbl->setText("Search aborted");
                clear();
                return;
            }
            // getMetadataBItems has loaded thumbnails into bItems
            QImage imB = bItems.at(b).im;
            QString bPath = bItems.at(b).fPath;

            // compare every pixel
            int deltaPixels = compareRGB(imA, imB);
            //double deltaHue = visCmpImagesHues(imA, imB);

            R item;
            //item.bId = b;
            //item.bPath = bItems.at(b).fPath;
            item.deltaPixels = deltaPixels;
            results[a][b] = item;

            if (isDebug)
            {
            qDebug() << "FindDuplicatesDlg::pixelCompare"
                     << "a =" << a
                     << "b =" << b
                     << "deltaPixels =" << deltaPixels
                     << "ui->deltaThreshold->value() =" << ui->deltaThreshold->value()
                     << bItems.at(b).fPath
                ;
            }

            // preview (for debugging)
            // candidate image
            /*
            getPreview(aPath, imA, "FindDupCandidate");
            showPreview(imA, aPath, "FindDupCandidate");
            // best match image
            getPreview(bPath, imB, "FindDupMatch");
            showPreview(imB, bPath, "FindDupMatch");
            */

            if (deltaPixels <= ui->deltaThreshold->value()) {
                Matches mItem;
                mItem.deltaPixels = deltaPixels;
                mItem.path = bItems.at(b).fPath;
                matches[a] << mItem;
            }

            counter++;
            ui->progressBar->setValue(1.0 * counter / totIterations * 100);
            qApp->processEvents();

        }
    }

    // sort deltas
    struct
    {
        bool operator()(Matches a, Matches b) const {return a.deltaPixels < b.deltaPixels;}
    }
    lessDelta;
    for(auto& vec : matches) {
        std::sort(vec.begin(), vec.end(), lessDelta);
    }

    // update A list source list
    for (int a = 0; a < model.rowCount(); a++) {
        if (matches[a].count() == 0) continue;
        QModelIndex idxDelta = model.index(a, MC::Delta);
        //int delta = static_cast<int>(results[a][0].deltaPixels);

        int deltaPixels = matches[a].at(0).deltaPixels;
        if (deltaPixels <= ui->deltaThreshold->value()) {
            model.setData(idxDelta, deltaPixels);
            idxDelta = model.index(a, MC::CheckBox);
            model.itemFromIndex(idxDelta)->setCheckState(Qt::Checked);
            model.setData(model.index(a,MC::CheckBox), matches[a].count());
        }
    }
}

QString FindDuplicatesDlg::currentMatchString(int a, int b)
{
    if (!matches.contains(a)) {
        return "No matches";
    }
    //int count = matches[a].count();
    QString tot = QString::number(matches[a].count());
    return QString::number(b+1) + " of " + tot + " matches";
}

void FindDuplicatesDlg::clear()
{
    abort = false;
    ui->progressBar->setValue(0);
    matches.clear();
    results.clear();
    bItems.clear();
    for (int i = 0; i < model.rowCount(); i++) {
        model.itemFromIndex(model.index(i,0))->setCheckState(Qt::Unchecked);
        model.setData(model.index(i, MC::CheckBox), 0);
        model.setData(model.index(i, MC::Delta), "");
    }
}

void FindDuplicatesDlg::initializeResultsVector()
{
    results.clear();
    results.resize(dm->sf->rowCount());
    for (auto& vec : results) {
        vec.resize(bItems.count());
    }
}

void FindDuplicatesDlg::on_clrFoldersBtn_clicked()
{
    ui->includeSubfolders->clear();
    ui->excludeSubfolders->clear();
}

void FindDuplicatesDlg::buildBItemsList(QStringList &dPaths)
{
    QDir *dir = new QDir;
    QStringList *fileFilters = new QStringList;
    foreach (const QString &str, metadata->supportedFormats) {
        fileFilters->append("*." + str);
    }
    dir->setNameFilters(*fileFilters);
    dir->setFilter(QDir::Files);

    // build list of image files
    ui->progressLbl->setText("Building list of image files");
    QStringList bFiles;
    foreach (QString dPath, dPaths) {
        dir->setPath(dPath);
        for (int i = 0; i < dir->entryInfoList().size(); i++) {
            bFiles << dir->entryInfoList().at(i).filePath();
        }
    }
    int totIterations = bFiles.count();

    // populate bItems;
    QString s = "Reading metadata for " + QString::number(totIterations) + " source files";
    ui->progressLbl->setText(s);
    int counter = 0;
    foreach (QString fPath, bFiles) {
        if (abort) {
            ui->progressLbl->setText("Search aborted");
            clear();
            return;
        }

        counter++;
        QString s = "Reading target images: " + QString::number(counter) + " of "
                    + QString::number(totIterations);
        int pctProgress = 1.0 * counter / totIterations * 100;
        ui->progressLbl->setText(s);
        ui->progressBar->setValue(pctProgress);
        qApp->processEvents();

        B bItem;
        bItem.fPath = fPath;

        // get the thumbnail (used to compare to A thumbnail in datamodel)
        QImage image;
        if (ui->samePixelsCB) {
            autonomousImage->image(bItem.fPath, image, G::maxIconSize, "BItemThumbnail");
            bItem.im = image;
        }

        // get metadata info for the B file to calc aspect
        QFileInfo fInfo(fPath);

        if (!metadata->loadImageMetadata(fInfo, dm->instance, true, true, false, true, "FindDuplicatesDlg::buildBItemsList")) {
            // deal with failure
            //continue;
        }
        ImageMetadata *m = &metadata->m;

        // type
        bItem.type = QFileInfo(fPath).suffix().toLower();

        // create date
        bItem.createdDate = m->createdDate.toString("yyyy-MM-dd hh:mm:ss.zzz");

        // duration
        if (ui->sameDurationCB->isChecked()) {
            if (metadata->videoFormats.contains(bItem.type)) {
                QString s = metadata->readExifToolTag(fPath, "duration#");
                quint64 duration = static_cast<quint64>(s.toDouble());
                //duration /= 1000;
                QTime durationTime((duration / 3600) % 60, (duration / 60) % 60,
                                   duration % 60, (duration * 1000) % 1000);
                QString format = "mm:ss";
                if (duration > 3600) format = "hh:mm:ss";
                bItem.duration = durationTime.toString(format);
                /*
                qDebug() << "FindDuplicatesDlg::buildBItemsList"
                         << "s =" << s
                         << "duration =" << duration
                         << "durationTime =" << durationTime
                         << "bItem.duration =" << bItem.duration
                    ; //*/
            }
            else bItem.duration = "00:00";
        }

        if (ui->sameAspectCB->isChecked() && ui->samePixelsCB) {
            double aspect;
            if (m->width && m->height) {
                if (m->orientation == 6 || m->orientation == 8) aspect = m->height * 1.0 / m->width;
                else aspect = m->width * 1.0 / m->height;
            }
            else {
                if (!image.isNull()) aspect = image.width() * 1.0 / image.height();
                else aspect = 0;
            }
            bItem.aspect = QString::number(aspect,'f', 2);
        }
        bItems.append(bItem);
        /*
        qDebug() << "FindDuplicatesDlg::buildBItemsList" << i << bItem.fPath;
        //reportRGB(image);
        //*/
    }
}

void FindDuplicatesDlg::getMetadataBItems()
{
    int totIterations = bItems.count();

    // populate bItems;
    QString s = "Reading metadata for " + QString::number(totIterations) + " source files";
    ui->progressLbl->setText(s);
    int counter = 0;
    for (int b = 0; b < bItems.count(); b++ ) {
        if (abort) {
            ui->progressLbl->setText("Search aborted");
            clear();
            return;
        }

        counter++;
        QString s = "Reading target images: " + QString::number(counter) + " of "
                    + QString::number(totIterations);
        int pctProgress = 1.0 * counter / totIterations * 100;
        ui->progressLbl->setText(s);
        ui->progressBar->setValue(pctProgress);
        qApp->processEvents();

        QString fPath = bItems.at(b).fPath;

        // get the thumbnail (used to compare to A thumbnail in datamodel)
        QImage image;
        if (ui->samePixelsCB->isChecked()) {
            qDebug() << "FindDuplicatesDlg::getMetadataBItems autonomousImage->thumbNail"
                     << "G::maxIconSize =" << G::maxIconSize
                     << fPath;
            autonomousImage->image(fPath, image, G::maxIconSize, "BItemThumbnail");
            bItems[b].im = image;
        }

        // get metadata info for the B file to calc aspect
        QFileInfo fInfo(fPath);

        if (b == 4) {
            int x = 0; // pause for debug
        }

        if (!metadata->loadImageMetadata(fInfo, dm->instance, true, true, false, true, "FindDuplicatesDlg::buildBItemsList")) {
            // deal with failure
            //continue;
        }
        ImageMetadata *m = &metadata->m;

        // type
        bItems[b].type = QFileInfo(fPath).suffix().toLower();

        // create date
        bItems[b].createdDate = m->createdDate.toString("yyyy-MM-dd hh:mm:ss.zzz");

        qDebug() << "FindDuplicatesDlg::buildBItemsList"
                 << "b =" << b
                 << "createdDate =" << bItems[b].createdDate;

        // duration
        if (ui->sameDurationCB->isChecked()) {
            if (metadata->videoFormats.contains(bItems.at(b).type)) {
                QString s = metadata->readExifToolTag(fPath, "duration#");
                quint64 duration = static_cast<quint64>(s.toDouble());
                //duration /= 1000;
                QTime durationTime((duration / 3600) % 60, (duration / 60) % 60,
                                   duration % 60, (duration * 1000) % 1000);
                QString format = "mm:ss";
                if (duration > 3600) format = "hh:mm:ss";
                bItems[b].duration = durationTime.toString(format);
                /*
                qDebug() << "FindDuplicatesDlg::buildBItemsList"
                         << "s =" << s
                         << "duration =" << duration
                         << "durationTime =" << durationTime
                         << "bItem.duration =" << bItem.duration
                    ; //*/
            }
            else bItems[b].duration = "00:00";
        }

        if (ui->sameAspectCB->isChecked()) {
            double aspect;
            if (m->width && m->height) {
                if (m->orientation == 6 || m->orientation == 8) aspect = m->height * 1.0 / m->width;
                else aspect = m->width * 1.0 / m->height;
            }
            else {
                if (!image.isNull()) aspect = image.width() * 1.0 / image.height();
                else aspect = 0;
            }
            bItems[b].aspect = QString::number(aspect,'f', 2);
        }
    }
}

void FindDuplicatesDlg::buildBList()
{
/*
    Build a B list (collection or library) of images to look for duplicates of the candidate
    A list images in the datamodel.
*/
    // build include folders list bFolderPaths
    ui->progressLbl->setText("Building list of image folders");
    QStringList bFolderPaths;
    for (int cF = 0; cF < ui->includeSubfolders->count(); cF++) {
        QString root = ui->includeSubfolders->item(cF)->text();
        if (!root.isEmpty() && root[root.length()-1] == '/') {
            root.chop(1);
        }
        bFolderPaths << root;
        if (ui->includeSubfoldersCB->isChecked()) {
            QDirIterator it(root, QDir::Dirs | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
            while (it.hasNext()) {
                it.next();
                bFolderPaths << it.filePath();
            }
        }
    }
    /*
    qDebug() << "bFolderPaths list:";
    foreach (QString s, bFolderPaths) qDebug() << "" << s;
    //*/

    // build exclude folders list bExcludeFolderPaths
    QStringList bExcludeFolderPaths;
    for (int cF = 0; cF < ui->excludeSubfolders->count(); cF++) {
        QString root = ui->excludeSubfolders->item(cF)->text();
        if (!root.isEmpty() && root[root.length()-1] == '/') {
            root.chop(1);
        }
        bExcludeFolderPaths << root;
        if (ui->includeSubfoldersCB->isChecked()) {
            QDirIterator it(root, QDir::Dirs | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
            while (it.hasNext()) {
                it.next();
                bExcludeFolderPaths << it.filePath();
            }
        }
    }
    if (isDebug) {
        qDebug() << "bExcludeFolderPaths list:";
        foreach (QString s, bExcludeFolderPaths) qDebug() << "" << s;
    }

    // remove exclude folders from bFolderPaths
    foreach (const QString &s, bExcludeFolderPaths) {
        bFolderPaths.removeAll(s);
    }
    if (isDebug) {
        qDebug() << "bFolderPaths list after exclusion:";
        foreach (QString s, bFolderPaths) qDebug() << "" << s;
    }

    // populate bItems fPath
    QDir *dir = new QDir;
    QStringList *fileFilters = new QStringList;
    foreach (const QString &str, metadata->supportedFormats) {
        fileFilters->append("*." + str);
    }
    dir->setNameFilters(*fileFilters);
    dir->setFilter(QDir::Files);

    // build list of image files in bItems
    //ui->progressLbl->setText("Building list of image files");
    QStringList bFiles;
    foreach (QString dPath, bFolderPaths) {
        dir->setPath(dPath);
        for (int i = 0; i < dir->entryInfoList().size(); i++) {
            B bItem;
            bItem.fPath = dir->entryInfoList().at(i).filePath();
            //qDebug() << bItem.fPath;
            bItems << bItem;
        }
    }

    // build bItems
//    buildBItemsList(bFolderPaths);
//    if (bItems.size() == 0) {
//        G::popUp->showPopup("No compare to images...");
//        return;
//    }
    /*
    qDebug() << "FindDuplicatesDlg::on_compareBtn_clicked  bItems.size() =" << bItems.size();
    //*/
}

bool FindDuplicatesDlg::sameFileType(int a, int b)
{
    QString pathA = dm->sf->index(a, G::PathColumn).data(G::PathRole).toString();
    QString extA = QFileInfo(pathA).suffix().toLower();
    QString pathB = bItems.at(b).fPath;
    QString extB = QFileInfo(pathB).suffix().toLower();
    bool isSame = (extA == extB);
    if (isDebug)
    qDebug() << "FileType     a =" << a << extA << "b =" << b << extB<< "isSame" << isSame;
    results[a][b].sameType = isSame;
    return isSame;
}

bool FindDuplicatesDlg::sameCreationDate(int a, int b)
{
    QString dateA = dm->sf->index(a, G::CreatedColumn).data().toString();
    QString dateB = bItems.at(b).createdDate;
    //QString dateB = m->createdDate.toString("yyyy-MM-dd hh:mm:ss.zzz");
    bool isSame = (dateA == dateB);
    if (isDebug)
    qDebug() << "CreationDate a =" << a << dateA << "b =" << b << dateB << "isSame" << isSame;
    results[a][b].sameCreationDate = isSame;
    return isSame;
}

bool FindDuplicatesDlg::sameAspect(int a, int b)
{
    // A datamodel
    double aspect = dm->sf->index(a, G::AspectRatioColumn).data().toDouble();
    QString aspectA = QString::number(aspect,'f', 2);
    // B collection
    QString aspectB = bItems.at(b).aspect;
    bool isSame = (aspectA == aspectB);
    if (isDebug)
    qDebug() << "sameAspect   a =" << a << aspectA << "b =" << b << aspectB << "isSame" << isSame;
    results[a][b].sameAspect = isSame;
    return isSame;
}

bool FindDuplicatesDlg::sameDuration(int a, int b)
{
    // A datamodel
    bool isSame;
    QString durationA;
    QString durationB;
    bool isVideo = dm->sf->index(a, G::VideoColumn).data().toBool();
    if (isVideo) {
        durationA = dm->sf->index(a, G::DurationColumn).data().toString();
        if (durationA.length() == 0) durationA = "00:00";
        // B collection
        durationB = bItems.at(b).duration;
        isSame = (durationA == durationB);
    }
    else {
        durationA = "00:00";
        durationB = "00:00";
        isSame = true;
    }
    if (isDebug)
        qDebug() << "sameDuration   a =" << a << durationA << "b =" << b << durationB << "isSame" << isSame;
    results[a][b].sameDuration = isSame;
    return isSame;
}

void FindDuplicatesDlg::findMatches()
{
    //qDebug() << "\nFindDuplicatesDlg::findMatches\n";
    initializeResultsVector();
    int aCount =  dm->sf->rowCount();
    int bCount =  bItems.count();
    for (int a = 0; a < aCount; a++) {
        matchCount = 0;
        int pctProgress = 1.0 * (a+1) / aCount * 100;
        ui->progressBar->setValue(pctProgress);
        qApp->processEvents();
        for (int b = 0; b < bCount; b++) {
            QString s = "a = " + QString::number(a+1) + " of " + QString::number(aCount) + "   " +
                        "b = " + QString::number(b+1) + " of " + QString::number(bCount);
            ui->progressLbl->setText(s);
            //qDebug() << s;
            //reportFindMatch(a, b);

             // same file type
            if (ui->sameFileTypeCB->isChecked()) {
                if (!sameFileType(a, b)) continue;
            }
            // same creation date
            if (ui->sameCreationDateCB->isChecked()) {
                if (!sameCreationDate(a, b)) continue;
            }
            // same aspect
            if (ui->sameAspectCB->isChecked()) {
                if (!sameAspect(a, b)) continue;
            }
            // same duration (video)
            if (ui->sameDurationCB->isChecked()) {
                if (!sameDuration(a, b)) continue;
            }
            // item match
            Matches mItem;
            mItem.path = bItems.at(b).fPath;
            matches[a] << mItem;
            model.itemFromIndex(model.index(a,0))->setCheckState(Qt::Checked);
            matchCount++;
            if (isDebug) reportFindMatch(a, b);
            /*
            qDebug() //<< "FindDuplicatesDlg::findMatches"
                     << QString::number(a).leftJustified(5)
                     << bItems.at(b).fPath
                ; //*/
            //break;
        }
        model.setData(model.index(a,MC::CheckBox), matchCount);
    }
}

void FindDuplicatesDlg::buildResults()
{
/*
    The results vector matrix: results[a][b] is an R item.
        a = index for datamodel items (candidates)
        b = index for bItems (targets)

        R parameters:
            int bId;
            QString fPath;
            bool sameType;
            bool sameCreationDate;
            bool sameAspect;
            bool sameDuration;
            double deltaPixels;
            bool match;
            int matchId;

    Examples:   Is datamodel item a type the same as bItems type
                    results[a][b].sameType
                The best target deltaPixels match for image a
                    results[a][0]   (results is sorted after comparing pixels)
*/
    // initialize result vector [A indexs][BItems]
    initializeResultsVector();

    // compare criteria
    if (isDebug)
    qDebug() << "\nFindDuplicatesDlg::buildResults\n";

    for (int a = 0; a < dm->sf->rowCount(); a++) {
        for (int b = 0; b < bItems.count(); b++) {
            if (isDebug)
            qDebug() << "FindDuplicatesDlg::buildResults  A ="
                     <<  dm->sf->index(a,G::NameColumn).data().toString()
                     <<  "B =" << bItems.at(b).fPath
                ;
            // same file type
            if (ui->sameFileTypeCB->isChecked()) {
                results[a][b].sameType = sameFileType(a, b);
                if (isDebug)
                qDebug() << "FindDuplicatesDlg::buildResults results[a][b].sameType" << results[a][b].sameType;
            }

            // same creation date
            if (ui->sameCreationDateCB->isChecked()) {
                results[a][b].sameCreationDate = sameCreationDate(a, b);
                if (isDebug)
                qDebug() << "FindDuplicatesDlg::buildResults results[a][b].sameCreationDate" << results[a][b].sameCreationDate;
            }

            // same aspect
            if (ui->sameAspectCB->isChecked()) {
                results[a][b].sameAspect = sameAspect(a, b);
                if (isDebug)
                qDebug() << "FindDuplicatesDlg::buildResults results[a][b].sameAspect" << results[a][b].sameAspect;
            }
            // same duration (video)
            if (ui->sameDurationCB->isChecked()) {
                results[a][b].sameDuration = sameDuration(a, b);
                if (isDebug)
                qDebug() << "FindDuplicatesDlg::buildResults results[a][b].sameDuration" << results[a][b].sameDuration;
            }
            if (isDebug)
            qDebug() << "\n";
        }
    }
}

int FindDuplicatesDlg::updateResults()
{
    int matchCount = 0;
    // update local model results table
    bool cmpType = ui->sameFileTypeCB->isChecked();
    bool cmpDate = ui->sameCreationDateCB->isChecked();
    bool cmpAspect = ui->sameAspectCB->isChecked();
    bool cmpDuration = ui->sameDurationCB->isChecked();
    bool cmpPixels = ui->samePixelsCB->isChecked();
    bool isType;
    bool isDate;
    bool isAspect;
    bool isDuration;
    for (int a = 0; a < dm->sf->rowCount(); a++) {
        for (int b = 0; b < bItems.count(); b++) {
            // metadata match?
            if (cmpType) isType = results[a][b].sameType;
            else isType = true;
            if (cmpDate) isDate = results[a][b].sameCreationDate;
            else isDate = true;
            if (cmpAspect) isAspect = results[a][b].sameAspect;
            else isAspect = true;
            if (cmpDuration) isDuration = results[a][b].sameDuration;
            else isDuration = true;
            if (isDebug)
            qDebug() << "a =" << a << "b =" << b
                     << "sameType =" << results[a][b].sameType
                     << "sameCreationDate =" << results[a][b].sameCreationDate
                     << "sameAspect =" << results[a][b].sameAspect
                ;

            bool isMetaMatch = isType && isDate && isAspect && isDuration;
            //if (isMetaMatch) results[a][b].match = true;
            if (isMetaMatch) matchCount++;
            //if (isMetaMatch) results[a][b].matchId = b;
            if (isMetaMatch && !cmpPixels) {
                model.itemFromIndex(model.index(a,0))->setCheckState(Qt::Checked);
                break;
            }
            // must be sorted by delta before this
            if (cmpPixels && (results[a][0].deltaPixels < 1.1)) {
                //results[a][0].matchId = b;
                model.setData(model.index(a,1), results[a][0].deltaPixels);
                model.itemFromIndex(model.index(a,0))->setCheckState(Qt::Checked);
            }
        }
    }
    return matchCount;
}

void FindDuplicatesDlg::reportbItems()
{
    qDebug() << "FindDuplicatesDlg::reportbItems";
    int counter = 0;
    foreach (B bItem, bItems) {
        qDebug().noquote()
                 << QString::number(counter).leftJustified(5)
                 << "TYPE" << bItem.type.leftJustified(8)
                 << "DATE" << bItem.createdDate.leftJustified(25)
                 << "ASPECT" << bItem.aspect.leftJustified(6)
                 << "DURATION" << bItem.duration.leftJustified(10)
                 << bItem.fPath;
        counter++;
    }
}

void FindDuplicatesDlg::reportAspects()
{
    qDebug() << "\n" << "FindDuplicatesDlg::reportAspects";
    for (int a = 0, b = 0; static_cast<void>(a < dm->sf->rowCount()), b < bItems.count(); a++, b++) {
        QFileInfo fInfo(bItems.at(b).fPath);
        QString fileNameB  = (QFileInfo(bItems.at(b).fPath)).fileName();
        metadata->loadImageMetadata(fInfo, dm->instance, true, true, false, true);
        ImageMetadata *m = &metadata->m;
        // QString::number().rightJustified(3)
        qDebug().noquote()
                 << QString::number(a).rightJustified(3)
                 << QString::number(b).rightJustified(3)
                 //<< dm->sf->index(a,G::NameColumn).data().toString()
                 << "aspectA/B" << QString::number(dm->sf->index(a,G::AspectRatioColumn).data().toDouble(), 'f', 2)
                 << bItems.at(b).aspect
                 << "m >> w" << QString::number(m->width).rightJustified(5)
                 << "h" << QString::number(m->height).rightJustified(5)
                 << "orientation" << QString::number(m->orientation).rightJustified(1)
                 << fInfo.fileName()
            ;
    }
}

void::FindDuplicatesDlg::reportFindMatch(int a, int b)
{
    QString s = " ";
    QString rpt;
    rpt = "a = " + QString::number(a).leftJustified(5) + " b = " + QString::number(b).leftJustified(5);

    // A items
    QString fileNameA = dm->sf->index(a,G::NameColumn).data().toString().leftJustified(20);
    QString pathA = dm->sf->index(a, G::PathColumn).data(G::PathRole).toString();
    QString typeA = QFileInfo(pathA).suffix().toLower();
    QString dateA = dm->sf->index(a, G::CreatedColumn).data().toString();
    QString aspectA = QString::number(dm->sf->index(a, G::AspectRatioColumn).data().toDouble(),'f', 2);
    QString durationA = dm->sf->index(a, G::DurationColumn).data().toString();
    if (durationA.length() == 0) durationA = "00:00";

    QString same;
    if (ui->sameFileTypeCB->isChecked()) {
        QString typeB = bItems.at(b).type;
        bool same = typeA == typeB;
        QString sameType = same ? "true" : "false";
        rpt += "    TYPE " + sameType.leftJustified(6) + (typeA + s + typeB).leftJustified(9);
    }
    if (ui->sameCreationDateCB->isChecked()) {
        QString dateB = bItems.at(b).createdDate.leftJustified(23);     // might be blank date
        bool same = dateA == dateB;
        QString sameDate = same ? "true" : "false";
        rpt += "    DATE " + sameDate.leftJustified(6) + dateA + s + dateB;
    }
    if (ui->sameAspectCB->isChecked()) {
        QString aspectB = bItems.at(b).aspect;
        bool same = aspectA == aspectB;
        QString sameAspect = same ? "true" : "false";
        rpt += "    ASPECT "  + sameAspect.leftJustified(6) + (aspectA + s + aspectB).leftJustified(7);
    }
    if (ui->sameDurationCB->isChecked()) {
        QString durationB = bItems.at(b).duration;
        bool same = durationA == durationB;
        QString sameDuration = same ? "true" : "false";
        rpt += "    DURATION "  + sameDuration.leftJustified(6) + (durationA + s + durationB).leftJustified(7);
    }
    rpt += "    A: " + fileNameA.leftJustified(40);
    rpt += "B: " + bItems.at(b).fPath;

    qDebug().noquote() << rpt;
}

void::FindDuplicatesDlg::reportMatches()
{
    // titles
    QString t0 = "Candidate";
    QString t1 = "#";
    QString t2 = "Delta";
    QString t3 = "Duplicate";
    t0 = t0.leftJustified(40);
    t1 = t1.rightJustified(3);
    qDebug().noquote() << t0 << t1 << t2 << t3;

    bool isSamePixels = ui->samePixelsCB->isChecked();
    QString matchCount;
    QString delta;
    QString mPath;
    for (int a = 0; a < dm->sf->rowCount(); a++) {
        QString candidate = dm->sf->index(a,G::NameColumn).data().toString().leftJustified(40);
        if (matches[a].count() == 0) {
            matchCount = "  0";
            delta = "  n/a";
            QString nada = "No match found";
            qDebug().noquote() << candidate << matchCount << delta << nada;
            continue;
        }
        for (int b = 0; b < matches[a].count(); b++) {
            matchCount = QString::number(matches[a].count()).rightJustified(3);
            if (isSamePixels)
                delta = QString::number(matches[a].at(b).deltaPixels).rightJustified(5);
            else
                delta = "  n/a";
            mPath = matches[a].at(b).path;
            qDebug().noquote() << candidate << matchCount << delta << mPath;
        }
    }
}

void::FindDuplicatesDlg::reportResults()
{
    qDebug() << "\n" << "FindDuplicatesDlg::reportResults"
             << "  A count =" << dm->sf->rowCount() << "B count =" << bItems.count();
    QString reportString;
    QTextStream rpt;
    rpt.setString(&reportString);
    QString s = " ";
    int aDigits = QString::number(dm->sf->rowCount()).length() + 1;
    int bDigits = QString::number(bItems.count()).length() + 1;
    for (int a = 0; a < dm->sf->rowCount(); a++) {
        QString fileNameA = dm->sf->index(a,G::NameColumn).data().toString().leftJustified(40);
        QString pathA = dm->sf->index(a, G::PathColumn).data(G::PathRole).toString();
        QString typeA = QFileInfo(pathA).suffix().toLower();
        QString dateA = dm->sf->index(a, G::CreatedColumn).data().toString();
        QString aspectA = QString::number(dm->sf->index(a, G::AspectRatioColumn).data().toDouble(),'f', 2);
        QString durationA = dm->sf->index(a, G::DurationColumn).data().toString();
        if (durationA.length() == 0) durationA = "00:00";
        for (int b = 0; b < bItems.count(); b++) {
            if (ui->samePixelsCB->isChecked()) {

            }
            else {
                //rpt = "";
                rpt << "  a " + QString::number(a).leftJustified(aDigits);
                rpt << "b " + QString::number(b).leftJustified(bDigits);
                QString match = results[a][b].match ? "true" : "false";
                rpt << "  Match " + match.leftJustified(5);
                if (ui->sameFileTypeCB->isChecked()) {
                    QString sameType = results[a][b].sameType ? "true" : "false";
                    QString typeB = bItems.at(b).type;
                    rpt << "    TYPE " + sameType.leftJustified(6) + (typeA + s + typeB).leftJustified(9);
                }
                if (ui->sameCreationDateCB->isChecked()) {
                    QString sameCreationDate = results[a][b].sameCreationDate ? "true" : "false";
                    QString dateB = bItems.at(b).createdDate.leftJustified(23);     // might be blank date
                    rpt << "  DATE " + sameCreationDate.leftJustified(6) + dateA + s + dateB;
                }
                if (ui->sameAspectCB->isChecked()) {
                    QString sameAspect = results[a][b].sameAspect ? "true" : "false";
                    QString aspectB = bItems.at(b).aspect;
                    rpt << "    ASPECT "  + sameAspect.leftJustified(6) + (aspectA + s + aspectB).leftJustified(7);
                }
                if (ui->sameDurationCB->isChecked()) {
                    QString sameDuration = results[a][b].sameDuration ? "true" : "false";
                    QString durationB = bItems.at(b).duration;
                    rpt << "    DURATION "  + sameDuration.leftJustified(6) + (durationA + s + durationB).leftJustified(7);
                }
                rpt << "    A: " + fileNameA;
                rpt << "B: " + bItems.at(b).fPath;
                rpt << "\n";
            }
        }
    }
    QDialog *dlg = new QDialog;
    dlg->setStyleSheet(G::css);
    #ifdef Q_OS_WIN
    Win::setTitleBarColor(dlg->winId(), G::backgroundColor);
    #endif
    Ui::metadataReporttDlg md;
    md.setupUi(dlg);
    dlg->resize(2500, dlg->height());
    //dlg->setFixedWidth(1500);
    md.textBrowser->setStyleSheet(G::css);
    QFont courier("Courier", 12);
    md.textBrowser->setFont(courier);
    md.textBrowser->setText(reportString);
    md.textBrowser->setWordWrapMode(QTextOption::NoWrap);
    QFontMetrics metrics(md.textBrowser->font());
    md.textBrowser->setTabStopDistance(3 * metrics.horizontalAdvance(' '));
    dlg->exec();
}

void FindDuplicatesDlg::on_samePixelsCB_clicked()
{
    qDebug() << "FindDuplicatesDlg::on_samePixels_clicked";
    bool isPix = ui->samePixelsCB->isChecked();
    ui->sameFileTypeCB->setEnabled(!isPix);
    ui->sameCreationDateCB->setEnabled(!isPix);
    ui->sameAspectCB->setEnabled(!isPix);
    ui->sameDurationCB->setEnabled(!isPix);
    ui->deltaThresholdLbl->setEnabled(isPix);
    ui->deltaThreshold->setEnabled(isPix);
    ui->deltaThresholdToolBtn->setEnabled(isPix);
}

void FindDuplicatesDlg::on_compareBtn_clicked()
{
    // check target folder(s) assigned
    if (ui->includeSubfolders->count() == 0) {
        QString msg = "Comparison requires target folder(s)";
        G::popUp->showPopup(msg, 3000);
        return;
    }

    isRunning = true;
    ui->tv->setEnabled(false);
    clear();
    buildBList();
    if (bItems.size() == 0) {
        QString msg = "There are no images in the target folder(s).<br>"
                      "Include subfolders or use another folder."
            ;
        QMessageBox::warning(this, tr("Empty Folder(s)"), msg);
        return;
    }
    getMetadataBItems();
    if (ui->samePixelsCB->isChecked()) {
        initializeResultsVector();
        pixelCompare();
    }
    else {
        findMatches();
    }

    if (isDebug) {
        buildResults();  // populate results[a][b] for debugging
        //if (ui->pixelCompare->isChecked()) pixelCompare();
        matchCount = updateResults();
        reportResults();
    }

    isRunning = false;

    QString x = QString::number(matchCount);
    ui->progressLbl->setText("Search completed.  " + x + " matches found.  See candidate images table.");
    ui->progressBar->setValue(0);

    // show first best match
    ui->tv->setEnabled(true);
}

void FindDuplicatesDlg::on_prevToolBtn_clicked()
{
    int a = ui->tv->currentIndex().row();
    if (currentMatch > 0) {
        currentMatch--;
        int a = ui->tv->currentIndex().row();
        int b = currentMatch;
        QString bPath = matches[a].at(b).path;
        if (isDebug)
        {
        qDebug() << "FindDuplicatesDlg::on_prevToolBtn_clicked currentMatch =" << currentMatch << "a =" << a << "b =" << b;
        }
        QImage image;
        getPreview(bPath, image, "FindDupMatch");
        showPreview(bPath, image, "FindDupMatch");
        if (ui->samePixelsCB->isChecked()) {
            ui->deltaLbl->setText(QString::number(matches[a].at(b).deltaPixels));
        }
        ui->currentLbl->setText(currentMatchString(a, b));
    }
    else {
        G::popUp->showPopup("Start of match images");
    }
}

void FindDuplicatesDlg::on_nextToolBtn_clicked()
{
    int a = ui->tv->currentIndex().row();
    if (currentMatch + 1 < matches[a].count()) {
        currentMatch++;
        int b = currentMatch;
        QString bPath = matches[a].at(b).path;
        if (isDebug)
        {
        qDebug() << "FindDuplicatesDlg::on_nextToolBtn_clicked"
                 << "a =" << a
                 << "b =" << b
                 << "bPath =" << bPath;
        }
        QImage image;
        getPreview(bPath, image, "FindDupMatch");
        showPreview(bPath, image, "FindDupMatch");
        if (ui->samePixelsCB->isChecked()) {
            ui->deltaLbl->setText(QString::number(matches[a].at(b).deltaPixels));
        }
        ui->currentLbl->setText(currentMatchString(a, b));
    }
    else {
        G::popUp->showPopup("End of match images");
    }
}


void FindDuplicatesDlg::on_tv_clicked(const QModelIndex &index)
{
/*
    tv = tableview of candidate images

    Initiate the compare images, with the selected candidate image on the left,
    and the first target image on the right.
*/
    // do nothing if click on checkbox column
    if (index.column() == 0) return;

    currentMatch = 0;
    // larger A image (candidate)
    int a = index.row();
    // do nothing if no matches
    if (matches[a].count() == 0) {
        QPixmap pmNull;
        ui->candidateLbl->setPixmap(pmNull);
        ui->matchLbl->setPixmap(pmNull);
        ui->currentLbl->setVisible(false);
        return;
    }
    // candidate image path
    QString aPath = dm->sf->index(a,0).data(G::PathRole).toString();
    // first matching target image path
    QString bPath = matches[a].at(0).path;
    if (isDebug)
    {
        qDebug() << "FindDuplicatesDlg::on_tv_clicked  row (a) ="
                 << a << "col =" << index.column()
                 << "aPath =" << aPath
                 << "match =" << bPath
            ;
    }

    // show candidate and match images
    QImage image;
    // candidate image
    getPreview(aPath, image, "FindDupCandidate");
    showPreview(aPath, image, "FindDupCandidate");
    // best match image
    getPreview(bPath, image, "FindDupMatch");
    showPreview(bPath, image, "FindDupMatch");

    // best match delta
    if (ui->samePixelsCB->isChecked()) {
        ui->deltaLbl->setVisible(true);
        ui->deltaTxt->setVisible(false);
        //ui->currentLbl->setVisible(true);
        ui->deltaLbl->setText(QString::number(matches[a].at(0).deltaPixels));
    }
    else {
        ui->deltaLbl->setVisible(false);
        ui->deltaTxt->setVisible(false);
        //ui->currentLbl->setVisible(false);
    }
    ui->currentLbl->setVisible(true);
    ui->currentLbl->setText(currentMatchString(a, 0));
    ui->pathLbl->setText(bPath);
}

void FindDuplicatesDlg::on_abortBtn_clicked()
{
    if (isRunning) abort = true;
    else clear();
}

void FindDuplicatesDlg::on_cancelBtn_clicked()
{
    int a = 3;
    int b = 4;
    QIcon icon = dm->sf->index(a,0).data(Qt::DecorationRole).value<QIcon>();
    QImage imA = icon.pixmap(icon.actualSize(QSize(256, 256))).toImage();
    QString aPath = dm->sf->index(a,G::PathColumn).data().toString();
    QImage imB = bItems.at(b).im;
    qDebug() << "FindDuplicatesDlg::on_cancelBtn_clicked" << imB.size();
    QString bPath = bItems.at(b).fPath;
    //getPreview(aPath, imA, "FindDupCandidate");
    showPreview(aPath, imA, "FindDupCandidate");
    // best match image
    //getPreview(bPath, imB, "FindDupMatch");
    showPreview(bPath, imB, "FindDupMatch");
    return;
    reject();
}

void FindDuplicatesDlg::on_updateDupsAndQuitBtn_clicked()
{
    for (int a = 0; a < dm->sf->rowCount(); a++) {
        /*
        qDebug() << "FindDuplicatesDlg::on_updateDupsAndQuitBtn_clicked"
                 << model.itemFromIndex(model.index(a,0))->checkState();
        //*/
        QModelIndex idx = dm->sf->index(a, G::CompareColumn);
        dm->sf->setData(idx, false);
        if (model.itemFromIndex(model.index(a,0))->checkState() == Qt::Checked) {
            dm->sf->setData(idx, true);
        }
    }
    accept();
}

void FindDuplicatesDlg::resizeEvent(QResizeEvent *event)
{
    QDialog::resizeEvent(event);
    int w = ui->candidateLbl->width();
    int h = ui->candidateLbl->height();
    if (isDebug)
    qDebug() << "FindDuplicatesDlg::resizeEvent" << "w =" << w << "h =" << h;
    ui->candidateLbl->setPixmap(pA.scaled(w, h, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    ui->matchLbl->setPixmap(pB.scaled(w, h, Qt::KeepAspectRatio, Qt::SmoothTransformation));
}

void FindDuplicatesDlg::on_helpBtn_clicked()
{
//    reportbItems();
//    return;
//    reportAspects();
//    reportResults();

    QDialog *dlg = new QDialog;
    Ui::HelpFindDuplicatesDlg *ui = new Ui::HelpFindDuplicatesDlg;
    ui->setupUi(dlg);
    ui->textBrowser->setOpenExternalLinks(true);
    dlg->setWindowTitle("Find Duplicates");
    dlg->setStyleSheet(G::css);
    dlg->exec();

}

void FindDuplicatesDlg::progressMsg(QString msg)
{
    ui->progressLbl->setText(msg);
    QApplication::processEvents();
}

void FindDuplicatesDlg::on_toggleTvHideChecked_clicked()
{
    for (int row = 0; row < model.rowCount(); ++row) {
    QModelIndex index = model.index(row, 0);
    Qt::CheckState state = model.data(index, Qt::CheckStateRole).value<Qt::CheckState>();
    if (state != Qt::Checked) ui->tv->setRowHidden(row, okToHide);
    }
    okToHide = !okToHide;
    if (okToHide) ui->toggleTvHideChecked->setText("Hide unchecked images");
    else ui->toggleTvHideChecked->setText("Show unchecked images");
}


void FindDuplicatesDlg::on_tv_doubleClicked(const QModelIndex &index)
{
    reportMatches();
}


void FindDuplicatesDlg::on_reportBtn_clicked()
{
    reportResults();
}

