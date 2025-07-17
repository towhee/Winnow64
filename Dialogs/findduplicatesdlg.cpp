#include "Dialogs/FindDuplicatesDlg.h"
#include "ui_findduplicatesdlg.h"
#include "Main/global.h"
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
    // qDebug() << "DragToList::dragEnterEvent" << event;
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
    // qDebug() << "DragToList::dropEvent" << event;
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

Purpose

    For the images in the folder(s) currently selected in Winnow, look for matching images
    in the target foldewr(s).  A typical use case is selecting the downloads folder in
    Winnow, and then set all recent folders in the photo library as the target, and then
    search for matches to see if you already have to download images in your library.

Comparison terms:

    - A or a refers to the items in dm->sf datamodel
    - B or b refers to the items in ui->cmpToFolders
    - cmpToFolders is the list of all folders containing B images
    - bList is the list of all B images
    - matches is a hash indexed on A with the matching images from B
    - delta is the average difference in pixels A vs B

*/

FindDuplicatesDlg::FindDuplicatesDlg(QWidget *parent, DataModel *dm, Metadata *metadata) :
    QDialog(parent),
    ui(new Ui::FindDuplicatesDlg),
    dm(dm),
    metadata(metadata)
{
    frameDecoder = new FrameDecoder;
    autonomousImage = new AutonomousImage(metadata, frameDecoder);
    connect(frameDecoder, &FrameDecoder::frameImage, this, &FindDuplicatesDlg::setImageFromVideoFrame);
    // add disconnect in destructor...

    int id = 0; // dummy variable req'd by ImageDecoder
    imageDecoder = new ImageDecoder(id, dm, metadata);

    ui->setupUi(this);
    setStyleSheet(G::css);

    // set group box titles blue
    ui->targetBox->setStyleSheet("QGroupBox::title { color: #6CC1E8; }");
    ui->criteriaBox->setStyleSheet("QGroupBox::title { color: #6CC1E8; }");
    ui->compareImagesBox->setStyleSheet("QGroupBox::title { color: #6CC1E8; }");
    ui->candidateListBox->setStyleSheet("QGroupBox::title { color: #6CC1E8; }");
    // reset children of  ui->candidateListBox
    ui->AImageBox->setStyleSheet("QGroupBox::title { color: white; }");
    ui->BImageBox->setStyleSheet("QGroupBox::title { color: white; }");

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
    ui->sameFileTypeCB->setChecked(true);
    ui->sameCreationDateCB->setChecked(true);
    ui->sameAspectCB->setChecked(false);
    ui->sameDurationCB->setChecked(true);
    // set enabled states
    on_samePixelsCB_clicked();

    clear();

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
    model.setColumnCount(5);
    // optional way to set header alignment
    QStandardItem *iconItem = new QStandardItem("Icon");
    iconItem->setData(Qt::AlignHCenter, Qt::TextAlignmentRole);
    // model.setHorizontalHeaderItem(0, dupItem);
    model.setHorizontalHeaderItem(0, new QStandardItem("Dup"));
    model.setHorizontalHeaderItem(1, new QStandardItem(" #"));
    model.setHorizontalHeaderItem(2, new QStandardItem("Delta"));
    model.setHorizontalHeaderItem(3, iconItem);
    // model.setHorizontalHeaderItem(3, new QStandardItem("Icon"));
    model.setHorizontalHeaderItem(4, new QStandardItem("  File Name"));

    // populate model
    for (int a = 0; a < dm->sf->rowCount(); a++) {
        QVariant dupCount = 0;
        //model.setData(model.index(a,0), 0);
        // add checkbox
        model.itemFromIndex(model.index(a,0))->setCheckable(true);
        // add pixmap
        QVariant var = dm->sf->index(a,0).data(Qt::DecorationRole);
        QIcon icon = var.value<QIcon>();
        QPixmap pm = icon.pixmap(icon.actualSize(QSize(256, 256))).scaled(48, 48, Qt::KeepAspectRatio);
        model.setData(model.index(a,3), pm, Qt::DecorationRole);
        // add file name
        QString fName = dm->sf->index(a,G::NameColumn).data().toString();
        model.setData(model.index(a,4), fName);
    }

    // format tableview
    ui->tv->setModel(&model);
    QFontMetrics fm(ui->tv->fontMetrics());
    int w0 = fm.boundingRect("Dup-").width();
    int w1 = fm.boundingRect("=99=").width();
    int w2 = fm.boundingRect("=Delta=").width();
    int w3 = 55;    // icon width = 48px
    ui->tv->setColumnWidth(0, w0);
    ui->tv->setColumnWidth(1, w1);
    ui->tv->setColumnWidth(2, w2);
    ui->tv->setColumnWidth(3, w3);
    ui->tv->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Stretch);

    // format

    isDebug = false;
}

void FindDuplicatesDlg::getPreview(QString fPath, QImage &image, QString source)
{
/*
    Get the preview image for display in the comparison view: either the candidate or
    target.  If it is a video, then get the first video frame.

    The getPreview and showPreview are separate functions because video previews are
    obtained from FrameDecoder, which does not return the QImage, rather it siganls when
    it has the first frame
*/
    QFileInfo fileInfo(fPath);
    ImageMetadata *m;
    int row = dm->proxyRowFromPath(fPath);
    metadata->loadImageMetadata(fileInfo, row, dm->instance, true, true, false, true, "FindDuplicatesDlg::preview");
    m = &metadata->m;

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
        frameDecoder->clear();
        imageDecoder->decodeIndependent(image, metadata, *m);
    }
}

void FindDuplicatesDlg::showPreview(QString path, QImage image, QString source)
{
    /*
    Show the preview image for display in the comparison view: either the candidate or
    target.  If it is a video, then get the first video frame.

    The getPreview and showPreview are separate functions because video previews are
    obtained from FrameDecoder, which does not return the QImage, rather it siganls when
    it has the first frame
*/
    // previewLongSide = ui->candidateLbl->width();
    // if (image.width() > previewLongSide)
    //     previewSize = QSize(ui->candidateLbl->width(), ui->candidateLbl->height());
    // else
    //     previewSize = QSize(image.width(), image.width());

    if (source == "FindDupCandidate") {
        pA = QPixmap::fromImage(image);
        fit(pA, ui->candidateLbl);
        ui->candidateLbl->setToolTip(path);
    }
    if (source == "FindDupMatch") {
        pB = QPixmap::fromImage(image);
        fit(pB, ui->matchLbl);
        ui->matchLbl->setToolTip(path);
    }
}

void FindDuplicatesDlg::setImageFromVideoFrame(QString path, QImage image, QString source)
{
/*
    Thumbnails and previews for videos are obtained from FrameDecoder, which signals here
    with the first frame image.
*/
    int w = image.width();
    int h = image.height();
    // qDebug() << path << "We did it!" << w << h << image << source;
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
            // qDebug() << path << "We found it!" << bItems[b].im;
        }
    }
    if (source == "FindDupCandidate") {
        showPreview(path, image, source);
    }
    if (source == "FindDupMatch") {
        showPreview(path, image, source);
    }
}

double FindDuplicatesDlg::compareImagesHues(QImage &imA, QImage &imB)
{
/*
    Not being used.  Alternative to comparing RGB pixels.
*/
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
/*
    Compare the RGB for each pixel in imA / imB.  The difference (deltaRGB) can be
    0 - 255.  All the deltaRGBs are summed and then devided by the number of deltaRGBs
    to arrive at an overall deltaRGB.

    If deltaRGB = 0 then there is a perfect match between the images.
*/
    int w, h;
    if (imA.width() < imB.width()) w = imA.width();
    else w = imB.width();
    if (imA.height() < imB.height()) h = imA.height();
    else h = imB.height();

    if (w == 0 || h == 0) {
        QString msg = "Image width and/or height = 0.";
        G::issue("Warning", msg, "FindDuplicatesDlg::compareRGB");
        return 255;
    }
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
    int deltaAverage = 1.0 * deltaRGB / (w * h * 3 * 256) * 100;
    /*
    qDebug() << "w =" << w
             << "h =" << h
             << "tot delta =" << deltaRGB
             << "ave delta =" << deltaAverage
        ;
    //*/
    return deltaAverage;
}

void FindDuplicatesDlg::pixelCompare()
{
/*
    Iterate through all combinations of candidate images (imA) and target images (imB),
    comparing RGB to get a deltaPixels for each combination.

    If there is a match (dletaPixels < deltaThreshold) then add the imB to the
    matches hash.
*/
    initializeResultsVector();
    quint64 totIterations = dm->sf->rowCount() * bItems.count();
    ui->progressLbl->setText("Searching for duplicates in " + QString::number(totIterations) + " combinations");
    // iterate filtered datamodel
    int counter = 0;
    for (int a = 0; a < dm->sf->rowCount(); a++) {
        QString aPath = dm->sf->index(a,0).data(G::PathRole).toString();
        QString aFName = dm->sf->index(a,G::NameColumn).data().toString();

        // candidate thumbnail
        QVariant var = dm->sf->index(a,0).data(Qt::DecorationRole);
        QIcon icon = var.value<QIcon>();
        QImage imA = icon.pixmap(icon.actualSize(QSize(256, 256))).toImage();

        // compare candidate to each thumbnail in bList
        for (int b = 0; b < bItems.size(); b++) {
            if (abort) {
                ui->progressLbl->setText("Search aborted");
                clear();
                return;
            }
            // getMetadataBItems has loaded thumbnails into bItems
            QImage imB = bItems.at(b).im;
            QString bPath = bItems.at(b).fPath;
            int deltaPixels;

            // compare every pixel
            if (imB.width() > 0 && imB.height() > 0) {
                deltaPixels = compareRGB(imA, imB);
            }
            else {
                deltaPixels = 255;  // worst delta possible;
            }

            // save for reporting / debugging
            results[a][b].deltaPixels = deltaPixels;
            results[a][b].match = false;    // default

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

            /*
            // preview (for debugging)
            // candidate image
            getPreview(aPath, imA, "FindDupCandidate");
            showPreview(imA, aPath, "FindDupCandidate");
            // best match image
            getPreview(bPath, imB, "FindDupMatch");
            showPreview(imB, bPath, "FindDupMatch");
            */

            if (deltaPixels <= ui->deltaThreshold->value()) {
                qDebug() << "FindDuplicatesDlg::pixelCompare"
                         << "a =" << a
                         << "b =" << b
                         << "deltaPixels =" << deltaPixels
                         << "ui->deltaThreshold->value() =" << ui->deltaThreshold->value()
                         << bItems.at(b).fPath
                    ;
                Matches mItem;
                mItem.deltaPixels = deltaPixels;
                mItem.path = bItems.at(b).fPath;
                matches[a] << mItem;
                results[a][b].match = true;
            }

            counter++;
            ui->progressBar->setValue(1.0 * counter / totIterations * 100);
            if (G::useProcessEvents) qApp->processEvents();
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

    // update A list candidate list (tableview tv in dialog using model for indexes)
    for (int a = 0; a < model.rowCount(); a++) {
        // are there any duplicates in target images
        if (matches[a].count() == 0) continue;
        // update model
        QModelIndex idxDelta = model.index(a, MC::Delta);
        int deltaPixels = matches[a].at(0).deltaPixels;
        if (deltaPixels <= ui->deltaThreshold->value()) {
            model.setData(idxDelta, deltaPixels);
            QModelIndex idxChkBox = model.index(a, MC::CheckBox);
            model.itemFromIndex(idxChkBox)->setCheckState(Qt::Checked);
            model.setData(model.index(a, MC::Count), matches[a].count());
        }
    }
}

QString FindDuplicatesDlg::currentMatchString(int a, int b)
{
/*
    Reports ie "3 of 7 matches" under the target image preview.
                b of tot

    matches[a].at(b)
        a = match index = candidate row selected
        b = nth match for candidate a
*/
    if (!matches.contains(a)) {
        return "No matches";
    }
    QString tot = QString::number(matches[a].count());
    return QString::number(b+1) + " of " + tot + " matches";
}

void FindDuplicatesDlg::clear()
{
    abort = false;
    frameDecoder->clear();
    ui->progressBar->setValue(0);
    matches.clear();
    results.clear();
    bItems.clear();
    for (int i = 0; i < model.rowCount(); i++) {
        model.itemFromIndex(model.index(i, MC::CheckBox))->setCheckState(Qt::Unchecked);
        model.setData(model.index(i, MC::Count), 0);
        model.setData(model.index(i, MC::Delta), "");
    }
    ui->prevToolBtn->setVisible(false);
    ui->nextToolBtn->setVisible(false);
    ui->deltaTxt->setVisible(false);
    ui->deltaLbl->setText("");
    ui->currentLbl->setText("");
    ui->currentLbl->setVisible(false);
    ui->candidateFilenameLbl->setText("");
    ui->candidateFilenameLbl->setVisible(false);
    ui->candidateLbl->setPixmap(QPixmap());
    ui->targetPathLbl->setText("");
    ui->targetPathLbl->setVisible(false);
    ui->matchLbl->setPixmap(QPixmap());
    ui->progressLbl->setText("");    //ui->currentLbl->setText("");
    ui->tv->setEnabled(false);
}

void FindDuplicatesDlg::showImageComparisonStuff(int a, int b, QString bPath)
{
    if (!matches.contains(a)) return;
    if (ui->samePixelsCB->isChecked()) {
        ui->deltaLbl->setVisible(true);
        ui->deltaTxt->setVisible(true);
        ui->deltaLbl->setText(QString::number(matches[a].at(0).deltaPixels));
    }
    else {
        ui->deltaLbl->setVisible(false);
        ui->deltaTxt->setVisible(false);
    }
    ui->prevToolBtn->setVisible(true);
    ui->nextToolBtn->setVisible(true);
    ui->currentLbl->setVisible(true);
    ui->currentLbl->setText(currentMatchString(a, b));
    ui->currentLbl->setToolTip(currentMatchString(a, b));
    ui->targetPathLbl->setVisible(true);
    ui->targetPathLbl->setText(bPath);
    ui->targetPathLbl->setToolTip(bPath);
    ui->candidateLbl->setText("");
    ui->matchLbl->setText("");
}

void FindDuplicatesDlg::initializeResultsVector()
{
/*
    The results vector is used to save all the comparisons between the A candidate
    images and the B target images for reporting / debugging. It must be reset before
    a search for matches is run.
*/
    results.clear();
    results.resize(dm->sf->rowCount());
    for (auto& vec : results) {
        vec.resize(bItems.count());
    }
}

void FindDuplicatesDlg::on_clrFoldersBtn_clicked()
{
/*
    Removes any include and exclude folders from the target folder lists.
*/
    ui->includeSubfolders->clear();
    ui->excludeSubfolders->clear();
    clear();
}

void FindDuplicatesDlg::getMetadataBItems()
{
/*
    BThe B list bItems (target folder(s) images) of images is used to look for duplicates
    of the candidate A list images in the datamodel. The list is built in buildBList().

    The list bItems contains all the information required for comparison:
        - file name
        - type
        - createdDate
        - aspect
        - duration
        - im (QImage thumbnail for pixel comparison)

    This function populates bItems.
*/
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
        if (G::useProcessEvents) qApp->processEvents();

        QString fPath = bItems.at(b).fPath;

        // get the thumbnail (used to compare to A thumbnail in datamodel)
        QImage image;
        if (ui->samePixelsCB->isChecked()) {
            autonomousImage->image(fPath, image, G::maxIconSize, "BItemThumbnail");
            qDebug() << "WARNING" << "FindDuplicatesDlg::getMetadataBItems"
                     << "Zero width height"
                     << "b =" << b
                     << fPath;
            bItems[b].im = image;
        }

        // get metadata info for the B file to calc aspect
        QFileInfo fInfo(fPath);
        bool loadMeta = true;
        int row = dm->proxyRowFromPath(fPath);

        if (!metadata->loadImageMetadata(fInfo, row, dm->instance, true, true, false, true, "FindDuplicatesDlg::buildBItemsList")) {
            loadMeta = false;
        }
        ImageMetadata *m = &metadata->m;

        // file name
        bItems[b].name = QFileInfo(fPath).fileName().toLower();

        // type
        bItems[b].type = QFileInfo(fPath).suffix().toLower();


        if (bItems[b].type == "heic") {
            int x = 0;
        }

        // create date
        /*
        qDebug() << "FindDuplicatesDlg::getMetadataBItems"
                 << "b =" << b
                 << "m->createdDate =" << m->createdDate;
        */
        if (m->createdDate.isValid()) {
            bItems[b].createdDate = m->createdDate.toString("yyyy-MM-dd hh:mm:ss.zzz");
        }
        else {
            bItems[b].createdDate = fInfo.birthTime().toString("yyyy-MM-dd hh:mm:ss.zzz");
        }

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
    Build a B list bItems (target folder(s) images) of images to look for duplicates of
    the candidate A list images in the datamodel.

    The list bItems contains all the information required for comparison:
        - type
        - createdDate
        - aspect
        - duration
        - im (QImage thumbnail for pixel comparison)

    This function iterates through all the images defined by the include/exclude folders
    and appends a bItem for each image.  The bItem fields are populated in
    getMetadataBItems().
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
}

bool FindDuplicatesDlg::sameFilePath(int a, int b)
{
/*
    Compare candidate / target image file path and return result.  This is used
    to prevent including the candidate file as a duplicate result.
*/
    QString pathA = dm->sf->index(a, G::PathColumn).data(G::PathRole).toString().toLower();
    QString pathB = bItems.at(b).fPath.toLower();
    bool isSame = (pathA == pathB);
    // if (isDebug)
        qDebug() << "FilePath     a =" << a << pathA << "b =" << b << pathB << "isSame" << isSame;
    return isSame;
}

bool FindDuplicatesDlg::sameFileName(int a, int b)
{
    /*
    Compare candidate / target image file name and return result
*/
    QString nameA = dm->sf->index(a, G::NameColumn).data().toString().toLower();
    QString nameB = bItems.at(b).name;
    bool isSame = (nameA == nameB);
    if (isDebug)
        qDebug() << "FileName     a =" << a << nameA << "b =" << b << nameB << "isSame" << isSame;
    results[a][b].sameName = isSame;
    return isSame;
}

bool FindDuplicatesDlg::sameFileType(int a, int b)
{
/*
    Compare candidate / target image file type and return result
*/
    QString pathA = dm->sf->index(a, G::PathColumn).data(G::PathRole).toString();
    QString extA = QFileInfo(pathA).suffix().toLower();
    QString pathB = bItems.at(b).fPath;
    QString extB = QFileInfo(pathB).suffix().toLower();
    bool isSame = (extA == extB);
    if (isDebug)
        qDebug() << "FileType     a =" << a << extA << "b =" << b << extB << "isSame" << isSame;
    results[a][b].sameType = isSame;
    return isSame;
}

bool FindDuplicatesDlg::sameCreationDate(int a, int b)
{
/*
    Compare candidate / target image creation date and return result
*/
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
/*
    Compare candidate / target image aspect and return result
*/
    // A datamodel
    double aspect = dm->sf->index(a, G::AspectRatioColumn).data().toDouble();
    QString aspectA = QString::number(aspect,'f', 2);
    // B target
    QString aspectB = bItems.at(b).aspect;
    bool isSame = (aspectA == aspectB);
    if (isDebug)
    qDebug() << "sameAspect   a =" << a << aspectA << "b =" << b << aspectB << "isSame" << isSame;
    results[a][b].sameAspect = isSame;
    return isSame;
}

bool FindDuplicatesDlg::sameDuration(int a, int b)
{
/*
    Compare candidate / target image duration and return result
    Only relevent for videos.
*/
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
/*
    Iterates all candidate/target combinations, comparing the criteria that is checked.
    If they all match then the model item is checked then:
        - the item path is appended to the matches hash
        - results match set to true
    Note that pixel comparison is not done here.  See pixelCompare().
*/
    initializeResultsVector();
    int aCount =  dm->sf->rowCount();
    int bCount =  bItems.count();
    for (int a = 0; a < aCount; a++) {
        matchCount = 0;
        int pctProgress = 1.0 * (a+1) / aCount * 100;
        ui->progressBar->setValue(pctProgress);
        if (G::useProcessEvents) qApp->processEvents();
        for (int b = 0; b < bCount; b++) {
            QString s = "a = " + QString::number(a+1) + " of " + QString::number(aCount) + "   " +
                        "b = " + QString::number(b+1) + " of " + QString::number(bCount);
            ui->progressLbl->setText(s);
            results[a][b].match = false;

            // same file path = candidate image file = compare to itself
            if (!sameFilePath(a, b)) continue;

            // same file name
            if (ui->sameFileNameCB->isChecked()) {
                if (!sameFileName(a, b)) continue;
            }
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
            model.itemFromIndex(model.index(a, MC::CheckBox))->setCheckState(Qt::Checked);
            matchCount++;
            results[a][b].match = true;
            if (isDebug) reportFindMatch(a, b);
            /*
            qDebug() //<< "FindDuplicatesDlg::findMatches"
                     << QString::number(a).leftJustified(5)
                     << bItems.at(b).fPath
                ; //*/
            //break;
        }
        model.setData(model.index(a,MC::Count), matchCount);
    }
}

void FindDuplicatesDlg::buildResults()
{
/*
    Used for reporting / debugging
    The results vector matrix: results[a][b] is an R item.
        a = index for datamodel items (candidates)
        b = index for bItems (targets)

        R parameters:
            bool sameType;
            bool sameCreationDate;
            bool sameAspect;
            bool sameDuration;
            double deltaPixels;
            bool match;

    Examples:   Is datamodel item a type the same as bItems type
                    results[a][b].sameType
                The deltaPixels match for image a / b
                    results[a][b].deltaPixels
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
            // same file name
            if (ui->sameFileNameCB->isChecked()) {
                results[a][b].sameName = sameFileName(a, b);
                if (isDebug)
                qDebug() << "FindDuplicatesDlg::buildResults results[a][b].sameName" << results[a][b].sameName;
            }

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

void::FindDuplicatesDlg::reportResults()
{
/*
    The dialog report button executes this function, listing all the candicate/target
    matches, with the comparison results.  If alt/option pressed then all A/B
    combinations are reported, up to 100,000.
*/
    Qt::KeyboardModifiers modifiers = QGuiApplication::queryKeyboardModifiers();
    bool isModifier = false;
    if (modifiers & Qt::AltModifier) isModifier = true;
    // check for too many combinations
    if (isModifier) {
        quint32 tot = dm->sf->rowCount() * bItems.count();
        QLocale locale(QLocale::English, QLocale::UnitedStates);
        // Format the number using the locale-specific rules
        //QString formattedNumber = locale.toString(tot);
        if (tot > 100000) {
            QString msg = "The report length (candidates * targets) = " +
                          locale.toString(tot) +
                          ". This exceeds the maximum allowed (100,000).";
            G::popup->showPopup(msg, 4000);
            return;
        }
    }
    QString reportString;
    QTextStream rpt;
    rpt.setString(&reportString);
    QString s = " ";
    int aDigits = QString::number(dm->sf->rowCount()).length() + 1;
    int bDigits = QString::number(bItems.count()).length() + 1;
    // longest A filename string length
    int maxFileNameLenA = 0;
    for (int a = 0; a < dm->sf->rowCount(); a++) {
        QString pathA = dm->sf->index(a, G::NameColumn).data().toString();
        if (pathA.length() > maxFileNameLenA) maxFileNameLenA = pathA.length();
    }
    // longest B path string length
    int pathLenB = 0;
    for (int b = 0; b < bItems.count(); b++) {
        QString pathB = bItems.at(b).fPath;
        if (pathB.length() > pathLenB) pathLenB = pathB.length();
    }
    // report each combination
    for (int a = 0; a < dm->sf->rowCount(); a++) {
        QString fileNameA = dm->sf->index(a,G::NameColumn).data().toString().leftJustified(maxFileNameLenA);
        QString pathA = dm->sf->index(a, G::PathColumn).data(G::PathRole).toString();
        QString typeA = QFileInfo(pathA).suffix().toLower();
        QString dateA = dm->sf->index(a, G::CreatedColumn).data().toString();
        QString aspectA = QString::number(dm->sf->index(a, G::AspectRatioColumn).data().toDouble(),'f', 2);
        QString durationA = dm->sf->index(a, G::DurationColumn).data().toString();
        if (durationA.length() == 0) durationA = "00:00";
        for (int b = 0; b < bItems.count(); b++) {
            // show only matches
            if (!isModifier && !results[a][b].match) continue;
            // show all
            rpt << "  a " + QString::number(a).leftJustified(aDigits);
            rpt << "b " + QString::number(b).leftJustified(bDigits);
            QString match = results[a][b].match ? "true" : "false";
            rpt << "  Match " + match.leftJustified(5);
            if (ui->samePixelsCB->isChecked()) {
                QString delta = QString::number(results[a][b].deltaPixels).leftJustified(5);
                rpt << "    SAME PIXELS   Delta " << delta;
            }
            else {
                if (ui->sameFileNameCB->isChecked()) {
                    QString sameType = results[a][b].sameName ? "true" : "false";
                    QString typeB = bItems.at(b).name;
                    rpt << "    NAME " + sameType.leftJustified(25) + (typeA + s + typeB).leftJustified(25);
                }
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
            }
            rpt << "    A: " + fileNameA;
            // rpt << "    A: " + fileNameA.leftJustified(10);
            rpt << "   B: " + bItems.at(b).fPath;
            rpt << "   B: " + bItems.at(b).name;
            rpt << "\n";
        }
    }
    QDialog *dlg = new QDialog;
    dlg->setStyleSheet(G::css);
    #ifdef Q_OS_WIN
    Win::setTitleBarColor(dlg->winId(), G::backgroundColor);
    #endif
    Ui::metadataReporttDlg md;
    md.setupUi(dlg);
    md.textBrowser->setStyleSheet(G::css);
    QFont courier("Courier", 12);
    md.textBrowser->setFont(courier);
    md.textBrowser->setText(reportString);
    md.textBrowser->setWordWrapMode(QTextOption::NoWrap);
    QFontMetrics metrics(md.textBrowser->font());
    md.textBrowser->setTabStopDistance(3 * metrics.horizontalAdvance(' '));
    // get width in pixels of the longest line of text in report
    QString text = md.textBrowser->toPlainText();
    QStringList lines = text.split("\n");
    int longestLinePixelWidth = 0;
    for (const QString& line : lines) {
        int lineWidth = metrics.horizontalAdvance(line);
        longestLinePixelWidth = qMax(longestLinePixelWidth, lineWidth);
    }
    longestLinePixelWidth += 50;    // add dialog borders
    int widthToUse = longestLinePixelWidth < G::displayVirtualHorizontalPixels
                         ? G::displayVirtualHorizontalPixels : longestLinePixelWidth;
    qDebug() << "longestLinePixelWidth =" << longestLinePixelWidth
             << "G::displayVirtualHorizontalPixels =" << G::displayVirtualHorizontalPixels;
    dlg->resize(widthToUse, dlg->height());
    dlg->exec();
}

void FindDuplicatesDlg::on_includeSubfoldersCB_clicked()
{
    clear();
}

void FindDuplicatesDlg::on_samePixelsCB_clicked()
{
/*
    When selecting criteria for a match search, clicking on "Same Pixels" toggles
    all the other criteria.  Comparisons can only be executed for pixel matches
    or metadata matches.
*/
    // qDebug() << "FindDuplicatesDlg::on_samePixels_clicked";
    bool isPix = ui->samePixelsCB->isChecked();
    ui->sameFileNameCB->setEnabled(!isPix);
    ui->sameFileTypeCB->setEnabled(!isPix);
    ui->sameCreationDateCB->setEnabled(!isPix);
    ui->sameAspectCB->setEnabled(!isPix);
    ui->sameDurationCB->setEnabled(!isPix);
    ui->deltaThresholdLbl->setEnabled(isPix);
    ui->deltaThreshold->setEnabled(isPix);
    ui->deltaThresholdToolBtn->setEnabled(isPix);
    clear();
}

void FindDuplicatesDlg::on_compareBtn_clicked()
{
/*
    Clicking on "Find Duplicates" searches for any matches between the candidate
    images in the target images.

    - the bItems, matches and results data is cleared
    - the candidates table dup column is unchecked
    - the bItems list is built and populated
    - the checked criteria is used to compare candidates to targets
    - the candidates table is updated
*/
    // check target folder(s) assigned
    if (ui->includeSubfolders->count() == 0) {
        QString msg = "Comparison requires target folder(s)";
        G::popup->showPopup(msg, 3000);
        return;
    }

    isRunning = true;
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
        pixelCompare();
    }
    else {
        findMatches();
    }

    isRunning = false;

    // candidates with duplicates
    int candidatesWithDups = 0;
    for (int a = 0; a < model.rowCount(); a++) {
        if (model.itemFromIndex(model.index(a, MC::CheckBox))->checkState() == Qt::Checked)
            candidatesWithDups++;
    }
    QString x = QString::number(candidatesWithDups);
    if (candidatesWithDups == 1)
        ui->progressLbl->setText(x + " candidate is duplicated. See candidate images table.");
    else
        ui->progressLbl->setText(x + " candidates are duplicated. See candidate images table.");
    ui->progressBar->setValue(0);

    // enable candidate table
    ui->tv->setEnabled(true);
}

void FindDuplicatesDlg::on_prevToolBtn_clicked()
{
/*
    If there are multiple matches, the previous matched target image is displayed in
    the Compare Images section.
*/
    int a = ui->tv->currentIndex().row();
    if (currentMatch > 0) {
        QPixmap pmNull;
        ui->matchLbl->setPixmap(pmNull);
        ui->matchLbl->setText("Loading image...");
        ui->matchLbl->repaint();
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
        showImageComparisonStuff(a, b, bPath);
        // if (ui->samePixelsCB->isChecked()) {
        //     ui->deltaLbl->setText(QString::number(matches[a].at(b).deltaPixels));
        // }
        // ui->currentLbl->setText(currentMatchString(a, b));
        // ui->targetPathLbl->setVisible(true);
        // ui->targetPathLbl->setText(bPath);
        // ui->targetPathLbl->setToolTip(bPath);
    }
    else {
        G::popup->showPopup("Start of match images");
    }
}

void FindDuplicatesDlg::on_nextToolBtn_clicked()
{
/*
    If there are multiple matches, the next matched target image is displayed in
    the Compare Images section.
*/
    int a = ui->tv->currentIndex().row();
    if (currentMatch + 1 < matches[a].count()) {
        QPixmap pmNull;
        ui->matchLbl->setPixmap(pmNull);
        ui->matchLbl->setText("Loading image...");
        ui->matchLbl->repaint();
        currentMatch++;
        int b = currentMatch;
        QString bPath = matches[a].at(b).path;
        // if (isDebug)
        {
        qDebug() << "FindDuplicatesDlg::on_nextToolBtn_clicked"
                 << "a =" << a
                 << "b =" << b
                 << "bPath =" << bPath;
        }
        QImage image;
        getPreview(bPath, image, "FindDupMatch");
        showPreview(bPath, image, "FindDupMatch");
        showImageComparisonStuff(a, b, bPath);
        // if (ui->samePixelsCB->isChecked()) {
        //     ui->deltaLbl->setText(QString::number(matches[a].at(b).deltaPixels));
        // }
        // ui->currentLbl->setText(currentMatchString(a, b));
        // ui->targetPathLbl->setVisible(true);
        // ui->targetPathLbl->setText(bPath);
        // ui->targetPathLbl->setToolTip(bPath);
    }
    else {
        G::popup->showPopup("End of match images");
    }
}


void FindDuplicatesDlg::on_tv_clicked(const QModelIndex &index)
{
/*
    tv = tableview of candidate images

    Initiate the "Compare Images", with the selected candidate image on the left,
    and the first target image on the right.
*/
    qDebug() << "FindDuplicatesDlg::on_tv_clicked" << index;
    // do nothing if click on checkbox column
    if (index.column() == 0) return;

    // larger A image (candidate)
    int a = index.row();

    // wait
    QPixmap pmNull;
    ui->candidateLbl->setPixmap(pmNull);
    ui->candidateLbl->setText("Loading image...");
    ui->candidateLbl->repaint();
    ui->matchLbl->setPixmap(pmNull);
    ui->matchLbl->setText("Loading image...");
    ui->matchLbl->repaint();
    if (G::useProcessEvents) qApp->processEvents();

    currentMatch = 0;
    // larger A image (candidate)
    QString aName = dm->sf->index(a,G::NameColumn).data().toString();
    // candidate image path
    QString aPath = dm->sf->index(a,0).data(G::PathRole).toString();
    QString bPath = "";
    bool isMatch = matches.contains(a);
    if (isMatch) {
        if (matches[a].count() > 0) bPath = matches[a].at(0).path;
        showImageComparisonStuff(a, 0, bPath);
    }
    ui->candidateFilenameLbl->setVisible(true);
    ui->candidateFilenameLbl->setText(aName);

    // show candidate and match images
    QImage image;
    // candidate image
    getPreview(aPath, image, "FindDupCandidate");
    showPreview(aPath, image, "FindDupCandidate");
    // if no matches
    // if (matches[a].count() == 0) {
    if (isMatch) {
        // best match image
        if (matches[a].count() < 2) {
            ui->prevToolBtn->setVisible(false);
            ui->nextToolBtn->setVisible(false);
        }
        else {
            ui->prevToolBtn->setVisible(true);
            ui->nextToolBtn->setVisible(true);
        }
        if (matches[a].count() > 0) {
            getPreview(bPath, image, "FindDupMatch");
            showPreview(bPath, image, "FindDupMatch");
        }
    }
    else {
        ui->matchLbl->setText("No match");
        ui->currentLbl->setVisible(false);
    }
}

void FindDuplicatesDlg::on_abortBtn_clicked()
{
    if (isRunning) abort = true;
    else clear();
}

void FindDuplicatesDlg::on_cancelBtn_clicked()
{
    reject();
}

void FindDuplicatesDlg::on_updateDupsAndQuitBtn_clicked()
{
    for (int a = 0; a < dm->sf->rowCount(); a++) {
        /*
        qDebug() << "FindDuplicatesDlg::on_updateDupsAndQuitBtn_clicked"
                 << model.itemFromIndex(model.index(a,MC::CheckBox))->checkState();
        //*/
        QModelIndex idx = dm->sf->index(a, G::CompareColumn);
        dm->sf->setData(idx, false);
        if (model.itemFromIndex(model.index(a, MC::CheckBox))->checkState() == Qt::Checked) {
            dm->sf->setData(idx, true);
        }
    }
    accept();
}

void FindDuplicatesDlg::fit(QPixmap &pm, QLabel *label)
{
    int w = label->width();
    int h = label->height();
    // int d;
    // // if (pm.width() / pm.height() > 1) {
    // double aspectPM = pm.width() / pm.height();
    // double aspectLabel = label->width() / label->height();
    // if (aspectPM > aspectLabel) {
    //     d = label->height();
    // }
    // else {
    //     d = label->width();
    // }
    label->setPixmap(pA.scaled(w, h, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    // label->setPixmap(pA.scaled(d, d, Qt::KeepAspectRatio, Qt::SmoothTransformation));
}

void FindDuplicatesDlg::resizeEvent(QResizeEvent *event)
{
    //QDialog::resizeEvent(event);
    fit(pA, ui->candidateLbl);
    fit(pB, ui->matchLbl);
}

void FindDuplicatesDlg::on_helpBtn_clicked()
{
    QDialog *dlg = new QDialog;
    Ui::HelpFindDuplicatesDlg *ui = new Ui::HelpFindDuplicatesDlg;
    ui->setupUi(dlg);
    ui->textBrowser->setOpenExternalLinks(true);
    dlg->setWindowTitle("Find Duplicates");
    dlg->setStyleSheet(G::css);
    #ifdef Q_OS_WIN
    Win::setTitleBarColor(dlg->winId(), G::backgroundColor);
    #endif
    dlg->exec();
}

void FindDuplicatesDlg::on_deltaThresholdToolBtn_clicked()
{
/*
    The delta or difference in pixels between two images is determined by taking the
    difference of the red, green and blue component values (RGB), and then averaging the
    sum of all the differences. The values range of a red, green or blue component is 0 -
    255, therefore the maximum pixel delta = 255 and two identical images would have a
    pixel delta = 0.
*/
    QDialog *dlg = new QDialog;
    Ui::HelpPixelDelta *ui = new Ui::HelpPixelDelta;
    ui->setupUi(dlg);
    ui->textBrowser->setOpenExternalLinks(true);
    dlg->setWindowTitle("Pixel Delta");
    dlg->setStyleSheet(G::css);
    #ifdef Q_OS_WIN
    Win::setTitleBarColor(dlg->winId(), G::backgroundColor);
    #endif
    dlg->exec();
}

void FindDuplicatesDlg::progressMsg(QString msg)
{
    ui->progressLbl->setText(msg);
    if (G::useProcessEvents) QApplication::processEvents();
}

void FindDuplicatesDlg::on_toggleTvHideChecked_clicked()
{
    for (int row = 0; row < model.rowCount(); ++row) {
        QModelIndex index = model.index(row, MC::CheckBox);
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

// Debugging helpers

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
        int row = dm->proxyRowFromPath(bItems.at(b).fPath);
        QString fileNameB  = (QFileInfo(bItems.at(b).fPath)).fileName();
        metadata->loadImageMetadata(fInfo, row, dm->instance, true, true, false, true);
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
    QString nameA = QFileInfo(pathA).fileName().toLower();
    QString typeA = QFileInfo(pathA).suffix().toLower();
    QString dateA = dm->sf->index(a, G::CreatedColumn).data().toString();
    QString aspectA = QString::number(dm->sf->index(a, G::AspectRatioColumn).data().toDouble(),'f', 2);
    QString durationA = dm->sf->index(a, G::DurationColumn).data().toString();
    if (durationA.length() == 0) durationA = "00:00";

    QString same;
    if (ui->sameFileNameCB->isChecked()) {
        QString typeB = bItems.at(b).name;
        bool same = typeA == typeB;
        QString sameName = same ? "true" : "false";
        rpt += "    NAME " + sameName.leftJustified(25) + (typeA + s + typeB).leftJustified(25);
    }
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

void FindDuplicatesDlg::reportRGB(QImage &im)
{
    /*
    For debugging
*/
    int xBound = 15;
    int yBound = 1;

    int w = im.width();
    int h = im.height();
    qDebug() << "FindDuplicatesDlg::reportRGB:"
             << "w =" << w
             << "h =" << h
        ;
    for (int x = 0; x < w; x++) {
        for (int y = 0; y < h; y++) {
            QColor p = im.pixelColor(x,y);
            if (x < xBound && y < yBound)
                qDebug() << "  " << x
                         << "\t red:" << p.red()
                         << "\t green:" << p.green()
                         << "\t blue:" << p.blue()
                    ;
        }
    }
}

