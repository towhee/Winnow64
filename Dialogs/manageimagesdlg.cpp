#include "manageimagesdlg.h"
#include "ui_manageimagesdlg.h"

ManageImagesDlg::ManageImagesDlg(QString title,
                                 QSettings *setting,
                                 QString settingPath,
                                 QWidget *parent) :
    QDialog(parent),
    ui(new Ui::ManageImagesDlg)
{
    ui->setupUi(this);
    setWindowTitle(title);
    setStyleSheet(G::css);
    ui->newBtn->setStyleSheet("QPushButton {min-width: 120px;}");
    ui->deleteBtn->setStyleSheet("QPushButton {min-width: 120px;}");
    ui->doneBtn->setStyleSheet("QPushButton {min-width: 120px;}");
//    ui->newBtn->setMinimumWidth(120);

    /* set widths and heights that are dependent on the display screen settings in case the
    user drags the dialog to another screen/monitor.  */
//    setScreenDependencies();

    QStringList hdrs;
    hdrs << "Image Name";
    ht = 50;
    ui->imageTable->setColumnCount(1);
//    ui->imageTable->setHorizontalHeaderLabels(hdrs);
    ui->imageTable->horizontalHeader()->setStretchLastSection(true);
    ui->imageTable->horizontalHeader()->hide();
    ui->imageTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->imageTable->verticalHeader()->setSectionsClickable(false);
    ui->imageTable->verticalHeader()->setDefaultSectionSize(ht);
    ui->imageTable->verticalHeader()->hide();
    ui->imageTable->setIconSize(QSize(ht, ht));

    // read list of images from QSettings
    this->setting = setting;
    this->settingPath = settingPath;
    setting->beginGroup(settingPath);
    QStringList imageList;
    imageList << setting->allKeys();
    setting->endGroup();

    qDebug() << __FUNCTION__ << settingPath << imageList;

    // populate table with QSettings existing data
    for (int row = 0; row < imageList.length(); ++row) {
        QString key = settingPath + "/" + imageList.at(row);
        qDebug() << __FUNCTION__ << key;
        QByteArray ba = setting->value(key).toByteArray();
        QPixmap pm;
        pm.loadFromData(ba);
//        QPixmap pm80 = pm.scaledToHeight(ht);
        QIcon icon(pm.scaledToWidth(ht));
//        qDebug() << __FUNCTION__
//                 << "pm80 height =" << pm80.height()
//                 << "icon size =" << icon.actualSize(QSize(80,80));
        QTableWidgetItem *item = new QTableWidgetItem();
        item->setIcon(icon);
        item->setText(imageList.at(row));
        ui->imageTable->insertRow(row);
        ui->imageTable->setItem(row, 0, item);
    }
}

ManageImagesDlg::~ManageImagesDlg()
{
    delete ui;
}

void ManageImagesDlg::extractTile(QPixmap &src)
{
    PatternDlg patternDlg(this, src);
}

void ManageImagesDlg::on_newBtn_clicked()
{
    QString fPath = QFileDialog::getOpenFileName(this, tr("Select an image"), "/home");
    QFile file(fPath);
    if (!file.exists()) return;
    if (file.isOpen()) {
        QString msg = "Whoops.  The file is already open in another process.  \n"
                      "Close the file and try again.  Press ESC to continue.";
        G::popUp->showPopup(msg, 0);
        return;
    }
    QPixmap src(fPath);

    // Graphics
    if (settingPath == "Embel/Tiles") {

    }

    // Assign a unique name to the image and save in QSettings
    QFileInfo info(fPath);
    QString name = info.baseName();
    QByteArray graphicBa;
    QBuffer buffer(&graphicBa);
    buffer.open(QIODevice::WriteOnly);
    src.save(&buffer, "PNG");
    setting->beginGroup("Embel/Graphics");
        QStringList list = setting->allKeys();
        Utilities::uniqueInList(name, list);
        qDebug() << __FUNCTION__ << "Name to use =" << name;
        setting->setValue(name, graphicBa);
    setting->endGroup();

    // add to imageTable
    QIcon icon(src.scaledToWidth(ht));
    QTableWidgetItem *item = new QTableWidgetItem();
    item->setIcon(icon);
    item->setText(name);
    int row = ui->imageTable->model()->rowCount();
    ui->imageTable->insertRow(row);
    ui->imageTable->setItem(row, 0, item);

    // select new item row
    ui->imageTable->selectRow(row);

    // sort table
    ui->imageTable->setSortingEnabled(true);
    ui->imageTable->sortItems(0);
}

void ManageImagesDlg::on_deleteBtn_clicked()
{
    qDebug() << __FUNCTION__ << ui->imageTable->selectedItems();
    for (int i = 0; i < ui->imageTable->selectedItems().length(); ++i) {
        QString name = ui->imageTable->selectedItems().at(i)->text();
        QString sKey = settingPath + "/" + name;
        int row = ui->imageTable->selectedItems().at(i)->row();
        qDebug() << __FUNCTION__ << i
                 << name
                 << row
                 << sKey
                    ;

        // remove from QSettings
        setting->remove(sKey);

        // remove row in table
        ui->imageTable->model()->removeRow(row);
    }
}

void ManageImagesDlg::on_doneBtn_clicked()
{
    accept();
}
