#include "Dialogs/FindDuplicatesDlg.h"
#include "ui_FindDuplicatesDlg.h"
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

FindDuplicatesDlg::FindDuplicatesDlg(QWidget *parent, DataModel *dm, Metadata *metadata) :
    QDialog(parent),
    ui(new Ui::FindDuplicatesDlg),
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
    ui->includeSubfoldersCB->setChecked(false);
    ui->sameFileTypeCB->setChecked(true);
    ui->sameCreationDateCB->setChecked(true);
    ui->sameAspectCB->setChecked(false);
    ui->sameDurationCB->setChecked(true);
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
    model.setHorizontalHeaderItem(4, new QStandardItem("Match"));

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
    int w3 = fm.boundingRect("=name twenty wide=").width();
    //int w3 = ui->tv->width() - w0 - w1 - w2;
    ui->tv->setColumnWidth(0, w0);
    ui->tv->setColumnWidth(1, w1);
    ui->tv->setColumnWidth(2, w2);
    ui->tv->setColumnWidth(3, w3);
    ui->tv->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Stretch);

    // format

    isDebug = false;
}

void FindDuplicatesDlg::preview(QString fPath, QImage &image)
{
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
    imageDecoder->decode(image, metadata, *m);
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
    quint64 totIterations = dm->sf->rowCount() * bItems.count();
    ui->progressLbl->setText("Searching for duplicates in " + QString::number(totIterations) + " combinations");
    // iterate filtered datamodel
    int counter = 0;
    for (int a = 0; a < dm->sf->rowCount(); a++) {
        QString aFPath = dm->sf->index(a,0).data(G::PathRole).toString();
        QString aFName = dm->sf->index(a,G::NameColumn).data().toString();
        /*
        qDebug() << "FindDuplicatesDlg::on_compareBtn_clicked"
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
            qDebug() << "FindDuplicatesDlg::on_compareBtn_clicked"
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
    qDebug() << "FindDuplicatesDlg::on_compareBtn_clicked report deltas: a b delta bIndex";
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

QString FindDuplicatesDlg::currentBString(int b)
{
    return QString::number(b) + " of "
           + QString::number(bItems.count())
           + " possible matches";
}

void FindDuplicatesDlg::clear()
{
    abort = false;
    ui->progressBar->setValue(0);
    results.clear();
    bItems.clear();
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
        if (ui->pixelCompare) {
            autonomousImage->thumbNail(bItem.fPath, image, G::maxIconSize);
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
    for (int i = 0; i < bItems.count(); i++ ) {
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

        QString fPath = bItems.at(i).fPath;

        // get the thumbnail (used to compare to A thumbnail in datamodel)
        QImage image;
        if (ui->pixelCompare) {
            autonomousImage->thumbNail(fPath, image, G::maxIconSize);
            bItems[i].im = image;
        }

        // get metadata info for the B file to calc aspect
        QFileInfo fInfo(fPath);

        if (!metadata->loadImageMetadata(fInfo, dm->instance, true, true, false, true, "FindDuplicatesDlg::buildBItemsList")) {
            // deal with failure
            //continue;
        }
        ImageMetadata *m = &metadata->m;

        // type
        bItems[i].type = QFileInfo(fPath).suffix().toLower();

        // create date
        bItems[i].createdDate = m->createdDate.toString("yyyy-MM-dd hh:mm:ss.zzz");

        // duration
        if (ui->sameDurationCB->isChecked()) {
            if (metadata->videoFormats.contains(bItems.at(i).type)) {
                QString s = metadata->readExifToolTag(fPath, "duration#");
                quint64 duration = static_cast<quint64>(s.toDouble());
                //duration /= 1000;
                QTime durationTime((duration / 3600) % 60, (duration / 60) % 60,
                                   duration % 60, (duration * 1000) % 1000);
                QString format = "mm:ss";
                if (duration > 3600) format = "hh:mm:ss";
                bItems[i].duration = durationTime.toString(format);
                /*
                qDebug() << "FindDuplicatesDlg::buildBItemsList"
                         << "s =" << s
                         << "duration =" << duration
                         << "durationTime =" << durationTime
                         << "bItem.duration =" << bItem.duration
                    ; //*/
            }
            else bItems[i].duration = "00:00";
        }

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
            bItems[i].aspect = QString::number(aspect,'f', 2);
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
    return isSame;
}

bool FindDuplicatesDlg::sameDuration(int a, int b)
{
    // A datamodel
    QString durationA = dm->sf->index(a, G::DurationColumn).data().toString();
    if (durationA.length() == 0) durationA = "00:00";
    // B collection
    QString durationB = bItems.at(b).duration;
    bool isSame = (durationA == durationB);
    if (isDebug)
    qDebug() << "sameDuration   a =" << a << durationA << "b =" << b << durationB << "isSame" << isSame;
    return isSame;
}

void FindDuplicatesDlg::findMatches()
{
    qDebug() << "\nFindDuplicatesDlg::findMatches\n";
    matchCount = 0;
    int aCount =  dm->sf->rowCount();
    int bCount =  bItems.count();
    for (int a = 0; a < aCount; a++) {
        int pctProgress = 1.0 * (a+1) / aCount * 100;
        ui->progressBar->setValue(pctProgress);
        qApp->processEvents();
        for (int b = 0; b < bCount; b++) {
            QString s = "a = " + QString::number(a+1) + " of " + QString::number(aCount) + "   " +
                        "b = " + QString::number(b+1) + " of " + QString::number(bCount);
            ui->progressLbl->setText(s);
            //qDebug() << s;
//            reportFindMatch(a, b);

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
            model.setData(model.index(a,4), bItems.at(b).fPath);
            model.itemFromIndex(model.index(a,0))->setCheckState(Qt::Checked);
            matchCount++;
            reportFindMatch(a, b);
            /*
            qDebug() //<< "FindDuplicatesDlg::findMatches"
                     << QString::number(a).leftJustified(5)
                     << bItems.at(b).fPath
                ; //*/
            break;
        }
    }
}

void FindDuplicatesDlg::buildResults()
{
/*
    The results vector matrix: results[a][b] is an R item.
        a = index for datamodel item
        b = index for bItems (list of all B images)

        R parameters:
            int bId;
            QString fPath;
            bool sameType;
            bool sameCreationDate;
            bool sameAspect;
            bool sameDuration;
            bool sameMeta;
            double delta;
            int sameMetaId;
            int samePixelsId;

    Example: is datamodel item a type the same as bItems type
             results[a][b].sameType
*/
    // initialize result vector [A indexs][BItems]
    results.clear();
    results.resize(dm->sf->rowCount());
    for (auto& vec : results) {
        vec.resize(bItems.count());
    }

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

void::FindDuplicatesDlg::reportResults()
{
    qDebug() << "\n" << "FindDuplicatesDlg::reportResults"
             << "  A count =" << dm->sf->rowCount() << "B count =" << bItems.count();
    QString s = " ";
    QString rpt;
    int aDigits = QString::number(dm->sf->rowCount()).length() + 1;
    int bDigits = QString::number(bItems.count()).length() + 1;
    for (int a = 0; a < dm->sf->rowCount(); a++) {
        QString fileNameA = dm->sf->index(a,G::NameColumn).data().toString().leftJustified(20);
        QString pathA = dm->sf->index(a, G::PathColumn).data(G::PathRole).toString();
        QString typeA = QFileInfo(pathA).suffix().toLower();
        QString dateA = dm->sf->index(a, G::CreatedColumn).data().toString();
        QString aspectA = QString::number(dm->sf->index(a, G::AspectRatioColumn).data().toDouble(),'f', 2);
        QString durationA = dm->sf->index(a, G::DurationColumn).data().toString();
        if (durationA.length() == 0) durationA = "00:00";
        for (int b = 0; b < bItems.count(); b++) {
            rpt = "";
            rpt += "  a " + QString::number(a).leftJustified(aDigits);
            rpt += "b " + QString::number(b).leftJustified(bDigits);
            QString match = results[a][b].sameMeta ? "true" : "false";
            rpt += "  Match " + match.leftJustified(5);
            if (ui->sameFileTypeCB->isChecked()) {
                QString sameType = results[a][b].sameType ? "true" : "false";
                QString typeB = bItems.at(b).type;
                rpt += "    TYPE " + sameType.leftJustified(6) + (typeA + s + typeB).leftJustified(9);
            }
            if (ui->sameCreationDateCB->isChecked()) {
                QString sameCreationDate = results[a][b].sameCreationDate ? "true" : "false";
                QString dateB = bItems.at(b).createdDate.leftJustified(23);     // might be blank date
                rpt += "    DATE " + sameCreationDate.leftJustified(6) + dateA + s + dateB;
            }
            if (ui->sameAspectCB->isChecked()) {
                QString sameAspect = results[a][b].sameAspect ? "true" : "false";
                QString aspectB = bItems.at(b).aspect;
                rpt += "    ASPECT "  + sameAspect.leftJustified(6) + (aspectA + s + aspectB).leftJustified(7);
            }
            if (ui->sameDurationCB->isChecked()) {
                QString sameDuration = results[a][b].sameDuration ? "true" : "false";
                QString durationB = bItems.at(b).duration;
                rpt += "    DURATION "  + sameDuration.leftJustified(6) + (durationA + s + durationB).leftJustified(7);
            }
            rpt += "    A: " + fileNameA;
            rpt += "B: " + bItems.at(b).fPath;

            qDebug().noquote() << rpt;
            /*
            qDebug().noquote()
                << "  " << "a =" << QString::number(a).rightJustified(4)
                << "b =" << QString::number(b).rightJustified(4)
                << "  SAME" << match.leftJustified(5)
                << "    TYPE" << (typeA + s + typeB).leftJustified(9) + s + sameType.leftJustified(5)
                << "    DATE" << dateA + s + dateB + s + sameCreationDate.leftJustified(5)
                << "    ASPECT" << (aspectA + s + aspectB).leftJustified(7) + s + sameAspect.leftJustified(5)
                << " A:" << fileNameA
                << "B:" << bItems.at(b).fPath
                ;  //*/
        }
    }
}

void FindDuplicatesDlg::on_compareBtn_clicked()
{
    isRunning = true;
    clear();
    buildBList();
    getMetadataBItems();
    findMatches();

    // debugging
    bool isDebug = false;
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
    if (currentMatch > 0) {
        currentMatch--;
        int a = ui->tv->currentIndex().row();
        int b = currentMatch;
        QString fPath = results[a][b].fPath;
        if (isDebug)
        qDebug() << "FindDuplicatesDlg::on_prevToolBtn_clicked currentMatch =" << currentMatch << "a =" << a << "b =" << b;
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


void FindDuplicatesDlg::on_nextToolBtn_clicked()
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


void FindDuplicatesDlg::on_tv_clicked(const QModelIndex &index)
{
    if (index.column() == 0) return;
    currentMatch = 0;
    // larger A image (candidate)
    if (isDebug)
    qDebug() << "FindDuplicatesDlg::on_tv_clicked  index =" << index;
    int a = index.row();
    QString fPath = dm->sf->index(a,0).data(G::PathRole).toString();
    QString mPath = model.index(a, 4).data().toString();
    qDebug() << "FindDuplicatesDlg::on_tv_clicked  row (a) ="
             << a << "col =" << index.column()
             << "fPath =" << fPath
             << "match =" << mPath
        ;
    QImage image;
    preview(fPath, image);
    pA = QPixmap::fromImage(image);
    previewLongSide = ui->candidateLbl->width();
    previewSize = QSize(ui->candidateLbl->width(), ui->candidateLbl->height());
    ui->candidateLbl->setPixmap(pA.scaled(previewSize, Qt::KeepAspectRatio, Qt::SmoothTransformation));

    // larger B image (collection) best match

    // if debugging and buildResults
//    int b = 0;
//    fPath = results[a][b].fPath;
//    preview(fPath, image);
    preview(mPath, image);
    pB = QPixmap::fromImage(image);
    ui->matchLbl->setPixmap(pB.scaled(previewSize, Qt::KeepAspectRatio, Qt::SmoothTransformation));

    // best match delta
//    ui->deltaLbl->setText(QString::number(results[a][b].delta));
//    ui->currentLbl->setText(currentBString(b));
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

