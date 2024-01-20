#include "Dialogs/viscmpdlg.h"
#include "ui_viscmpdlg.h"
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

VisCmpDlg::VisCmpDlg(QWidget *parent, DataModel *dm, Metadata *metadata) :
    QDialog(parent),
    ui(new Ui::VisCmpDlg),
    dm(dm),
    metadata(metadata)
{
    autonomousImage = new AutonomousImage(metadata);
    int id = 0; // dummy variable req'd by ImaeDecoder
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
    abort = false;
    ui->sameFileTypeCB->setChecked(true);
    ui->sameCreationDateCB->setChecked(true);
    ui->sameAspectCB->setChecked(true);
    setupModel();

    #ifdef Q_OS_WIN
    Win::setTitleBarColor(winId(), G::backgroundColor);
    #endif
}

VisCmpDlg::~VisCmpDlg()
{
    delete ui;
}

void VisCmpDlg::setupModel()
{
    model.setRowCount(dm->sf->rowCount());
    model.setColumnCount(4);
    model.setHorizontalHeaderItem(0, new QStandardItem("Dup"));
    model.setHorizontalHeaderItem(1, new QStandardItem("Delta"));
    model.setHorizontalHeaderItem(2, new QStandardItem(""));    // icon
    model.setHorizontalHeaderItem(3, new QStandardItem("File Name"));

    // populate model
    for (int a = 0; a < dm->sf->rowCount(); a++) {
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
    int w0 = fm.boundingRect("=Dup=").width();
    int w1 = fm.boundingRect("=Factor=").width();
    int w2 = fm.boundingRect("=Icon=").width();
    //int w3 = ui->tv->width() - w0 - w1 - w2;
    ui->tv->setColumnWidth(0, w0);
    ui->tv->setColumnWidth(1, w1);
    ui->tv->setColumnWidth(2, w2);
    ui->tv->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);

    // format

    isDebug = false;
}

void VisCmpDlg::preview(QString fPath, QImage &image)
{
    QFileInfo fileInfo(fPath);
    ImageMetadata *m;
    if (metadata->loadImageMetadata(fileInfo, dm->instance, true, true, false, true, "VisCmpDlg::preview")) {
        m = &metadata->m;
    }
    else {
        QString errMsg = "Failed to load metadata";
        if (G::isWarningLogger)
            qWarning() << "WARNING" << "DataModel::readMetadataForItem" << "Failed to load metadata for " << fPath;
        return;
    }
    imageDecoder->decode(image, metadata, *m);
}

int VisCmpDlg::reportRGB(QImage &im)
{
    int w = im.width();
    int h = im.height();
    qDebug() << "VisCmpDlg::reportRGB:"
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

double VisCmpDlg::compareImagesHues(QImage &imA, QImage &imB)
{
    Effects effect;
    QVector<int> huesA(360, 0);
    QVector<int> huesB(360, 0);
    quint64 pixelsA = imA.width() * imA.height();
    quint64 pixelsB = imB.width() * imB.height();
    double deltaHue = 0;
    /*
    qDebug().noquote()
             << "VisCmpDlg::visCmpImagesHues"
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
                 << "VisCmpDlg::visCmpImagesHues"
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
        << "VisCmpDlg::visCmpImagesHues"
        << "normalized deltaHue =" << deltaHue / pixelsA * 100
        ; //*/

    return deltaHue;
}

int VisCmpDlg::compareRGB(QImage &imA, QImage &imB)
{
    int w, h;
    if (imA.width() < imB.width()) w = imA.width();
    else w = imB.width();
    if (imA.height() < imB.height()) h = imA.height();
    else h = imB.height();
    /*
    qDebug() << "VisCmpDlg::compareRGB:"
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

void VisCmpDlg::pixelCompare()
{
    quint64 totIterations = dm->sf->rowCount() * bItems.count();
    ui->progressLbl->setText("Searching for duplicates in " + QString::number(totIterations) + " combinations");
    // iterate filtered datamodel
    int counter = 0;
    for (int a = 0; a < dm->sf->rowCount(); a++) {
        QString aFPath = dm->sf->index(a,0).data(G::PathRole).toString();
        QString aFName = dm->sf->index(a,G::NameColumn).data().toString();
        /*
        qDebug() << "VisCmpDlg::on_compareBtn_clicked"
                 << "a =" << a
                 << "aFile =" << aFPath;
        //*/
        QVariant var = dm->sf->index(a,0).data(Qt::DecorationRole);
        QIcon icon = var.value<QIcon>();
        QImage imA = icon.pixmap(icon.actualSize(QSize(256, 256))).toImage();

        // compare to each thumb in bList
        for (int b = 0; b < bItems.size(); b++) {
            if (abort) {
                ui->progressLbl->setText("Search aborted");
                clear();
                return;
            }
            QImage imB = bItems.at(b).im;
            int deltaRGB = compareRGB(imA, imB);
            //double deltaHue = visCmpImagesHues(imA, imB);
            R item;
            item.bId = b;
            item.fPath = bItems.at(b).fPath;
            item.delta = deltaRGB;
            results[a][b] = item;

            counter++;
            ui->progressBar->setValue(1.0 * counter / totIterations * 100);
            qApp->processEvents();

            /*
            qDebug() << "VisCmpDlg::on_compareBtn_clicked"
                     << "a =" << a
                     << "b =" << b
                     << "delta =" << deltaRGB
                     << "aFName =" << aFName.leftJustified(20)
                     << "bFPath =" << result[a][b].fPath
                ;
                //*/
        }
    }

    // sort deltas
    struct
    {
        bool operator()(R a, R b) const {return a.delta < b.delta;}
    }
    lessDelta;

    for(auto& vec : results) {
        std::sort(vec.begin(), vec.end(), lessDelta);
    }

    /* report deltas
    qDebug() << "VisCmpDlg::on_compareBtn_clicked report deltas: a b delta bIndex";
    for (int a = 0; a < dm->sf->rowCount(); a++) {
        QString aFName = dm->sf->index(a,G::NameColumn).data().toString();
        for (int b = 0; b < bItems.size(); b++) {
            qDebug() << "  "
                     << "a =" << a
                     << "b =" << b
                     << "b index =" << result[a][b].index
                     << "delta =" << result[a][b].delta
                     << "aFName =" << aFName.leftJustified(20)
                     << "bFPath =" << result[a][b].fPath
                ;
        }
    }
    //*/
}

QString VisCmpDlg::currentBString(int b)
{
    return QString::number(b) + " of "
           + QString::number(bItems.count())
           + " possible matches";
}

void VisCmpDlg::clear()
{
    abort = false;
    ui->progressBar->setValue(0);
    results.clear();
    bItems.clear();
}

void VisCmpDlg::on_clrFoldersBtn_clicked()
{
    ui->includeSubfolders->clear();
    ui->excludeSubfolders->clear();
}

void VisCmpDlg::buildBItemsList(QStringList &dPaths)
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
        int pctProgress = 1.0 * counter / totIterations * 100;
        ui->progressBar->setValue(pctProgress);
        qApp->processEvents();
        ///*
        qDebug() << "VisCmpDlg::buildBItemsList" << counter << totIterations << pctProgress;
//        G::wait(10);
//        continue;
        //*/

        B bItem;
        bItem.fPath = fPath;

        // get the thumbnail (used to compare to A thumbnail in datamodel)
        QImage image;
        if (ui->pixelCompare) {
            autonomousImage->thumbNail(bItem.fPath, image, G::maxIconSize);
            bItem.im = image;
        }

        // get metadata info for the B file to calc aspect
        QFileInfo fInfo(fPath);

        if (!metadata->loadImageMetadata(fInfo, dm->instance, true, true, false, true, "VisCmpDlg::preview")){
            // deal with failure
            //continue;
        }
        ImageMetadata *m = &metadata->m;
        bItem.type = QFileInfo(metadata->m.fPath).suffix().toLower();
        bItem.createdDate = m->createdDate.toString("yyyy-MM-dd hh:mm:ss.zzz");

        if (ui->sameAspectCB->isChecked() && ui->pixelCompare) {
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
        qDebug() << "VisCmpDlg::buildBItemsList" << i << bItem.fPath;
        //reportRGB(image);
        //*/
    }
}

void VisCmpDlg::buildBList()
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

    // build bItems
    buildBItemsList(bFolderPaths);
    if (bItems.size() == 0) {
        G::popUp->showPopup("No compare to images...");
        return;
    }
    /*
    qDebug() << "VisCmpDlg::on_compareBtn_clicked  bItems.size() =" << bItems.size();
    //*/
}

bool VisCmpDlg::sameFileType(int a, int b)
{
    QString pathA = dm->sf->index(a, G::PathColumn).data(G::PathRole).toString();
    QString extA = QFileInfo(pathA).suffix().toLower();
    QString pathB = bItems.at(b).fPath;
    QString extB = QFileInfo(pathB).suffix().toLower();
    bool isSame = (extA == extB);
    if (isDebug)
    qDebug() << "FileType     a =" << a << extA << "b =" << b << extB<< "isSame" << isSame;
    return isSame;
}

bool VisCmpDlg::sameCreationDate(int a, int b)
{
    QString dateA = dm->sf->index(a, G::CreatedColumn).data().toString();
    QString dateB = bItems.at(b).createdDate;
    //QString dateB = m->createdDate.toString("yyyy-MM-dd hh:mm:ss.zzz");
    bool isSame = (dateA == dateB);
    if (isDebug)
    qDebug() << "CreationDate a =" << a << dateA << "b =" << b << dateB << "isSame" << isSame;
    return isSame;
}

bool VisCmpDlg::sameAspect(int a, int b)
{
    // A datamodel
    if (a == 7 && b == 7) {
        int x = 0;
    }
    double aspect = dm->sf->index(a, G::AspectRatioColumn).data().toDouble();
    QString aspectA = QString::number(aspect,'f', 2);
    // B collection
    QString aspectB = bItems.at(b).aspect;
    bool isSame = (aspectA == aspectB);
    if (isDebug)
    qDebug() << "sameAspect   a =" << a << aspectA << "b =" << b << aspectB << "isSame" << isSame;
    return isSame;
}

void VisCmpDlg::buildResults()
{
/*
    The results vector matrix: results[a][b] is a R item.
        a = index for datamodel item
        b = index for bItems (list of all B images)
*/
    // initialize result vector [A indexs][BItems]
    results.clear();
    results.resize(dm->sf->rowCount());
    for (auto& vec : results) {
        vec.resize(bItems.count());
    }

    // compare criteria
    if (isDebug)
    qDebug() << "\nVisCmpDlg::buildResults\n";

    for (int a = 0; a < dm->sf->rowCount(); a++) {
        for (int b = 0; b < bItems.count(); b++) {
            if (isDebug)
            qDebug() << "VisCmpDlg::buildResults  A ="
                     <<  dm->sf->index(a,G::NameColumn).data().toString()
                     <<  "B =" << bItems.at(b).fPath
                ;
            // same file type
            if (ui->sameFileTypeCB->isChecked()) {
                results[a][b].sameType = sameFileType(a, b);
                if (isDebug)
                qDebug() << "VisCmpDlg::buildResults results[a][b].sameType" << results[a][b].sameType;
            }

            // same creation date
            if (ui->sameCreationDateCB->isChecked()) {
                results[a][b].sameCreationDate = sameCreationDate(a, b/*, m*/);
                if (isDebug)
                qDebug() << "VisCmpDlg::buildResults results[a][b].sameCreationDate" << results[a][b].sameCreationDate;
            }

            // same aspect
            if (ui->sameAspectCB->isChecked()) {
                results[a][b].sameAspect = sameAspect(a, b/*, m*/);
                if (isDebug)
                qDebug() << "VisCmpDlg::buildResults results[a][b].sameAspect" << results[a][b].sameAspect;
            }
            // same duration (video)
            if (ui->sameDurationCB->isChecked()) {

            }
            if (isDebug)
            qDebug() << "\n";
        }
    }
}

int VisCmpDlg::updateResults()
{
    int matches = 0;
    // update local model results table
    bool cmpType = ui->sameFileTypeCB->isChecked();
    bool cmpDate = ui->sameCreationDateCB->isChecked();
    bool cmpAspect = ui->sameAspectCB->isChecked();
    bool cmpDuration = ui->sameDurationCB->isChecked();
    bool cmpPixels = ui->pixelCompare->isChecked();
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
            if (isMetaMatch) results[a][b].sameMeta = true;
            if (isMetaMatch) matches++;
            if (isMetaMatch) results[a][b].sameMetaId = b;
            if (isMetaMatch && !cmpPixels) {
                model.itemFromIndex(model.index(a,0))->setCheckState(Qt::Checked);
                break;
            }
            // must be sorted by delta before this
            if (cmpPixels && (results[a][0].delta < 1.1)) {
                results[a][0].sameMetaId = b;
                model.setData(model.index(a,1), results[a][0].delta);
                model.itemFromIndex(model.index(a,0))->setCheckState(Qt::Checked);
            }
        }
    }
    return matches;
}

void::VisCmpDlg::reportAspects()
{
    qDebug() << "\n" << "VisCmpDlg::reportAspects";
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

void::VisCmpDlg::reportResults()
{
    qDebug() << "\n" << "VisCmpDlg::reportResults";
    qDebug() << "A count =" << dm->sf->rowCount() << "B count =" << bItems.count();
    for (int a = 0; a < dm->sf->rowCount(); a++) {
        QString fileNameA = dm->sf->index(a,G::NameColumn).data().toString().leftJustified(20);
        QString pathA = dm->sf->index(a, G::PathColumn).data(G::PathRole).toString();
        QString typeA = QFileInfo(pathA).suffix().toLower();
        QString dateA = dm->sf->index(a, G::CreatedColumn).data().toString();
        QString aspectA = QString::number(dm->sf->index(a, G::AspectRatioColumn).data().toDouble(),'f', 2);
        for (int b = 0; b < bItems.count(); b++) {
            QString typeB = bItems.at(b).type;
            QString dateB = bItems.at(b).createdDate;
            QString aspectB = bItems.at(b).aspect;
            QString same = results[a][b].sameMeta ? "true" : "false";
            qDebug().noquote()
                << "  " << "a =" << a << "b =" << b
                << " SAME" << same.leftJustified(5)
                << " TYPE" << (typeA + ":" + typeB).leftJustified(9)
                << " DATE" << dateA + ":" + dateB
                << " ASPECT" << (aspectA + ":" + aspectB).leftJustified(7)
                << " A" << fileNameA
                << "B" << bItems.at(b).fPath
                ;
        }
    }
}

void VisCmpDlg::on_compareBtn_clicked()
{
    isRunning = true;
    clear();
    buildBList();
    buildResults();
    if (ui->pixelCompare->isChecked()) pixelCompare();
    int matches = updateResults();
    isRunning = false;

    QString x = QString::number(matches);
    ui->progressLbl->setText("Search completed.  " + x + " matches found.  See candidate images table.");

    // show first best match
    ui->tv->setEnabled(true);
    //ui->tv->setCurrentIndex(model.index(0,0));
    //on_tv_clicked(model.index(0,0));

    //reportAspects();
}

void VisCmpDlg::on_prevToolBtn_clicked()
{
    if (currentMatch > 0) {
        currentMatch--;
        int a = ui->tv->currentIndex().row();
        int b = currentMatch;
        QString fPath = results[a][b].fPath;
        if (isDebug)
        qDebug() << "VisCmpDlg::on_prevToolBtn_clicked currentMatch =" << currentMatch << "a =" << a << "b =" << b;
        QImage image;
        preview(fPath, image);
        ui->matchLbl->setPixmap(QPixmap::fromImage(image.scaled(previewSize, Qt::KeepAspectRatio)));
        ui->deltaLbl->setText(QString::number(results[a][b].delta));
        ui->currentLbl->setText(currentBString(b));
    }
    else {
        G::popUp->showPopup("Start of match images");
    }
}


void VisCmpDlg::on_nextToolBtn_clicked()
{
    if (currentMatch + 1 < bItems.count()) {
        currentMatch++;
        int a = ui->tv->currentIndex().row();
        int b = currentMatch;
        QString fPath = results[a][b].fPath;
        QImage image;
        preview(fPath, image);
        ui->matchLbl->setPixmap(QPixmap::fromImage(image.scaled(previewSize, Qt::KeepAspectRatio)));
        ui->deltaLbl->setText(QString::number(results[a][b].delta));
        ui->currentLbl->setText(currentBString(b));
    }
    else {
        G::popUp->showPopup("End of match images");
    }
}


void VisCmpDlg::on_tv_clicked(const QModelIndex &index)
{
    if (index.column() == 0) return;
    currentMatch = 0;
    // larger A image (candidate)
    if (isDebug)
    qDebug() << "VisCmpDlg::on_tv_clicked  index =" << index;
    int a = index.row();
    QString fPath = dm->sf->index(a,0).data(G::PathRole).toString();
    qDebug() << "VisCmpDlg::on_tv_clicked  row (a) ="
             << a << "col =" << index.column() << "fPath =" << fPath;
    QImage image;
    preview(fPath, image);
    pA = QPixmap::fromImage(image);
    previewLongSide = ui->candidateLbl->width();
    previewSize = QSize(ui->candidateLbl->width(), ui->candidateLbl->height());
    ui->candidateLbl->setPixmap(pA.scaled(previewSize, Qt::KeepAspectRatio, Qt::SmoothTransformation));

    // larger B image (collection) best match
    int b = 0;
    fPath = results[a][b].fPath;
    preview(fPath, image);
    pB = QPixmap::fromImage(image);
    ui->matchLbl->setPixmap(pB.scaled(previewSize, Qt::KeepAspectRatio, Qt::SmoothTransformation));

    // best match delta
    ui->deltaLbl->setText(QString::number(results[a][b].delta));
    ui->currentLbl->setText(currentBString(b));
}

void VisCmpDlg::on_abortBtn_clicked()
{
    if (isRunning) abort = true;
    else clear();
}

void VisCmpDlg::on_matchBtn_clicked()
{
    int a = ui->tv->currentIndex().row();
    model.itemFromIndex(model.index(a,0))->setCheckState(Qt::Checked);
}

void VisCmpDlg::on_cancelBtn_clicked()
{
    reject();
}

void VisCmpDlg::on_updateDupsAndQuitBtn_clicked()
{
    for (int a = 0; a < dm->sf->rowCount(); a++) {
        /*
        qDebug() << "VisCmpDlg::on_updateDupsAndQuitBtn_clicked"
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

void VisCmpDlg::resizeEvent(QResizeEvent *event)
{
    QDialog::resizeEvent(event);
    int w = ui->candidateLbl->width();
    int h = ui->candidateLbl->height();
    if (isDebug)
    qDebug() << "VisCmpDlg::resizeEvent" << "w =" << w << "h =" << h;
    ui->candidateLbl->setPixmap(pA.scaled(w, h, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    ui->matchLbl->setPixmap(pB.scaled(w, h, Qt::KeepAspectRatio, Qt::SmoothTransformation));
}

void VisCmpDlg::on_helpBtn_clicked()
{


//    reportAspects();
//    reportResults();
}

void VisCmpDlg::progressMsg(QString msg)
{
    ui->progressLbl->setText(msg);
    QApplication::processEvents();
}

void VisCmpDlg::on_toggleTvHideChecked_clicked()
{
    for (int row = 0; row < model.rowCount(); ++row) {
    QModelIndex index = model.index(row, 0);
    Qt::CheckState state = model.data(index, Qt::CheckStateRole).value<Qt::CheckState>();
    if (state != Qt::Checked) ui->tv->setRowHidden(row, okToHide);
    }
    okToHide = !okToHide;
    if (okToHide) ui->toggleTvHideChecked->setText("Hide checked images");
    else ui->toggleTvHideChecked->setText("Show checked images");
}

