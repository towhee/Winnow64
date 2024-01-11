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
    setStyleSheet(QStringLiteral("background-image: url(:/images/dragfoldershere.png)"));
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

VisCmpDlg::VisCmpDlg(DataModel *dm, Metadata *metadata, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::VisCmpDlg),
    dm(dm),
    metadata(metadata)
{
    autonomousImage = new AutonomousImage(metadata);
    int id = 0; // dummy variable req'd by ImaeDecoder
    imageDecoder = new ImageDecoder(this, id, dm, metadata);
    ui->setupUi(this);
    QImage prevIm(":/images/prev.png");
    QImage nextIm(":/images/next.png");
    ui->prevToolBtn->setIcon(QIcon(QPixmap::fromImage(prevIm.scaled(16,16))));
    ui->nextToolBtn->setIcon(QIcon(QPixmap::fromImage(nextIm.scaled(16,16))));
    setupModel();
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
    model.setHorizontalHeaderItem(2, new QStandardItem("Icon"));
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
    int w0 = fm.boundingRect("=Icon=").width();
    int w1 = fm.boundingRect("=Dup=").width();
    int w2 = fm.boundingRect("=Factor=").width();
    int w3 = ui->tv->width() - w0 - w1 - w2;
    ui->tv->setColumnWidth(0, w0);
    ui->tv->setColumnWidth(1, w1);
    ui->tv->setColumnWidth(2, w2);
    ui->tv->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
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

void VisCmpDlg::buildBItemsList(QString dPath)
{
    QDir *dir = new QDir;
    QStringList *fileFilters = new QStringList;
    foreach (const QString &str, metadata->supportedFormats) {
        fileFilters->append("*." + str);
    }
    dir->setNameFilters(*fileFilters);
    dir->setFilter(QDir::Files);
    dir->setPath(dPath);
    //list.clear();
    B bItem;
    for (int i = 0; i < dir->entryInfoList().size(); i++) {
        bItem.fPath = dir->entryInfoList().at(i).filePath();
        QImage image;
        bool isThumb = true;
        autonomousImage->thumbNail(bItem.fPath, image, G::maxIconSize);
        bItem.im = image;
        bItems.append(bItem);

        qDebug() << "VisCmpDlg::buildBItemsList" << bItem.fPath;
        reportRGB(image);
    }
}

int VisCmpDlg::reportRGB(QImage &im)
{
    int w = im.width();
    int h = im.height();
    qDebug() << "VisCmpDlg::reportRGB:"
             << "w =" << w
             << "h =" << h
        ;
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
    qDebug() << "VisCmpDlg::compareRGB:"
             << "w =" << w
             << "h =" << h
             << "aW =" << imA.width()
             << "aH =" << imA.height()
             << "bW =" << imB.width()
             << "bH =" << imB.height()
        ;
    int deltaRGB = 0;
    for (int x = 0; x < w; x++) {
        for (int y = 0; y < h; y++) {
            QColor p1 = imA.pixelColor(x,y);
            QColor p2 = imB.pixelColor(x,y);
            deltaRGB += qAbs(p1.red() - p2.red());
            deltaRGB += qAbs(p1.green() - p2.green());
            deltaRGB += qAbs(p1.blue() - p2.blue());
            ///*
            if (x < 15 && y < 1)
            qDebug() << "  " << x
                     << "\t red:" << p1.red() << p2.red()
                     << "\t green:" << p1.green() << p2.green()
                     << "\t blue:" << p1.blue() << p2.blue()
                     << "delta =" << deltaRGB
                ;
            //*/
        }
    }
    deltaRGB = deltaRGB / (w * h * 3) * 100;
    return deltaRGB;
}

void VisCmpDlg::on_compareBtn_clicked()
{

    // build list of images to compare to
    for (int cF = 0; cF < ui->cmpToFolders->count(); cF++) {
        QString cmpFolder = ui->cmpToFolders->item(cF)->text();
        buildBItemsList(cmpFolder);
    }
    if (bItems.size() == 0) {
        G::popUp->showPopup("No compare to images...");
        return;
    }
    qDebug() << "VisCmpDlg::on_compareBtn_clicked  bItems.size() =" << bItems.size();

    // initialize result vector{A indexs][BItems]
    result.clear();
    result.resize(dm->sf->rowCount());
    for(auto& vec : result) {
        vec.resize(bItems.count());
    }

    // iterate filtered datamodel
    for (int a = 0; a < dm->sf->rowCount(); a++) {
        QString dmFPath = dm->sf->index(a,0).data(G::PathRole).toString();
        QString dmFName = dm->sf->index(a,G::NameColumn).data().toString();
        qDebug() << "VisCmpDlg::on_compareBtn_clicked"
                 << "dRow =" << a
                 << "dmFile =" << dmFPath;
        QVariant var = dm->sf->index(a,0).data(Qt::DecorationRole);
        QIcon icon = var.value<QIcon>();
        QImage imA = icon.pixmap(icon.actualSize(QSize(256, 256))).toImage();

        // compare to each thumb in bList
        for (int b = 0; b < bItems.size(); b++) {
            QImage imB = bItems.at(b).im;
            int deltaRGB = compareRGB(imA, imB);
            //double deltaHue = visCmpImagesHues(imA, imB);
            R item;
            item.index = b;
            item.fPath = bItems.at(b).fPath;
            item.delta = deltaRGB;
            result[a][b] = item;
            /*
            qDebug() << "VisCmpDlg::on_compareBtn_clicked"
                     << "a =" << a
                     << "b =" << b
                     << "delta =" << deltaRGB
                     << "aFName =" << dmFName.leftJustified(20)
                     << "bFPath =" << bItems.at(b).fPath
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

    for(auto& vec : result) {
        std::sort(vec.begin(), vec.end(), lessDelta);
    }

    // report deltas
    qDebug() << "VisCmpDlg::on_compareBtn_clicked report deltas: a b delta bIndex";
    for (int a = 0; a < dm->sf->rowCount(); a++)
        for (int b = 0; b < bItems.size(); b++)
            qDebug() << "  " << a << b << result[a][b].delta << result[a][b].index;

    // update results table
    for (int a = 0; a < dm->sf->rowCount(); a++)
        model.setData(model.index(a,1), result[a][0].delta);
}

void VisCmpDlg::on_prevToolBtn_clicked()
{

}


void VisCmpDlg::on_nextToolBtn_clicked()
{

}


void VisCmpDlg::on_tv_clicked(const QModelIndex &index)
{
    // larger A image (candidate)
    int a = index.row();
    QString fPath = dm->sf->index(a,0).data(G::PathRole).toString();
    qDebug() << "VisCmpDlg::on_tv_clicked  row (a) =" << a << "fPath =" << fPath;
    QImage image;
    preview(fPath, image);
    previewLongSide = ui->candidateLbl->width();
    qDebug() << "previewLongSide =" << previewLongSide;
    QSize size(previewLongSide, previewLongSide);
    ui->candidateLbl->setPixmap(QPixmap::fromImage(image.scaled(size, Qt::KeepAspectRatio)));

    // larger B image (collection) best match
    int b = 0;
    fPath = result[a][b].fPath;
    preview(fPath, image);
    ui->matchLbl->setPixmap(QPixmap::fromImage(image.scaled(size, Qt::KeepAspectRatio)));

    // best match delta
    ui->deltaLbl->setText(QString::number(result[a][b].delta));
}

