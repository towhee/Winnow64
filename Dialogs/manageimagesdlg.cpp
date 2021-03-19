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

    // populate table with QSettings existing data
    for (int row = 0; row < imageList.length(); ++row) {
        QString key = settingPath + "/" + imageList.at(row);
        QByteArray ba = setting->value(key).toByteArray();
        QPixmap pm;
        pm.loadFromData(ba);
        QIcon icon(pm/*.scaledToWidth(ht)*/);
        QTableWidgetItem *item = new QTableWidgetItem();
        item->setIcon(icon);
        item->setText(imageList.at(row));
        ui->imageTable->insertRow(row);
        ui->imageTable->setItem(row, 0, item);
    }

    connect(ui->imageTable, &QTableWidget::itemDoubleClicked,
            this, &ManageImagesDlg::itemDoubleClicked);

    connect(ui->imageTable, &QTableWidget::itemChanged,
            this, &ManageImagesDlg::itemChanged);

    isInitializing = false;
}

ManageImagesDlg::~ManageImagesDlg()
{
    delete ui;
}

void ManageImagesDlg::extractTile(QPixmap &src)
{
    PatternDlg patternDlg(this, src);

}

void ManageImagesDlg::save(QPixmap *pm)
{
    // Assign a unique name to the image and save in QSettings
    QFileInfo info(fPath);
    QString name = info.baseName();
    QByteArray graphicBa;
    QBuffer buffer(&graphicBa);
    buffer.open(QIODevice::WriteOnly);
    pm->save(&buffer, "PNG");
    setting->beginGroup(settingPath);
        QStringList list = setting->allKeys();
        Utilities::uniqueInList(name, list);
        setting->setValue(name, graphicBa);
    setting->endGroup();

    // add to imageTable
    QIcon icon(pm->scaledToWidth(ht));
    QTableWidgetItem *item = new QTableWidgetItem();
    item->setIcon(icon);
    item->setText(name);
    int row = ui->imageTable->model()->rowCount();
    ui->imageTable->insertRow(row);
    ui->imageTable->setItem(row, 0, item);

    // sort table
    ui->imageTable->setSortingEnabled(true);
    ui->imageTable->sortItems(0);

    // select new item row
    ui->imageTable->selectRow(item->row());

    // let user know
    QString msg = "Image has been saved as " + name + ".";
    G::popUp->showPopup(msg, 1000);
}

void ManageImagesDlg::itemDoubleClicked(QTableWidgetItem *item)
{
/*
    itemDoubleClicked initiates editing.  The text is preserved prior to edit changes.
*/
    if (isInitializing) return;
    prevName = item->text();
    qDebug() << __FUNCTION__ << item << prevName;
}

void ManageImagesDlg::itemChanged(QTableWidgetItem *item)
{
/*
    This is triggered after editing item text by Enter Key or focus leaving the item.

    Updates required:
        QSettings
        EmbelProperties::updateTileList updates
            EmbelProperties::tileList
            EmbelProperties::borderTileObjectEditor
        EmbelProperties::updateGraphicList updates
            EmbelProperties::graphicList
            EmbelProperties::borderTileObjectEditor
*/
    if (isInitializing) return;

    // Update QSettings
    QString oldKey = settingPath + "/" + prevName;
    QString newKey = settingPath + "/" + item->text();
    // get QSettings value (QByteArray of tile image) from old key
    QByteArray ba = setting->value(oldKey).toByteArray();
    // QSettings copy to new key
    setting->setValue(newKey, ba);
    // QSettings remove old key
    setting->remove(oldKey);

    // Updates in EmbelProperties (updateTileList or updateGraphicList)
    emit itemRenamed(prevName, item->text());
}

void ManageImagesDlg::on_newBtn_clicked()
{
    fPath = QFileDialog::getOpenFileName(this, tr("Select an image"), "/home");
    QFile file(fPath);
    if (!file.exists()) return;
    if (file.isOpen()) {
        QString msg = "Whoops.  The file is already open in another process.  \n"
                      "Close the file and try again.  Press ESC to continue.";
        G::popUp->showPopup(msg, 0);
        return;
    }
    QPixmap src(fPath);

    // If a tile
    if (settingPath == "Embel/Tiles") {
        PatternDlg *patternDlg = new PatternDlg(this, src);
        connect(patternDlg, &PatternDlg::saveTile, this, &ManageImagesDlg::save);
        patternDlg->exec();
    }
    else save(&src);
}

void ManageImagesDlg::on_deleteBtn_clicked()
{
    QList<QTableWidgetItem *> toDeleteList;
    toDeleteList << ui->imageTable->selectedItems();

    QString deleteMsg;
    for (int i = 0; i < toDeleteList.length(); ++i) {
        deleteMsg += toDeleteList.at(i)->text() + "\n";
    }
    deleteMsg += "                                    ";
    int ret = (QMessageBox::warning(this, "Delete these item(s)?", deleteMsg,
                             QMessageBox::Cancel | QMessageBox::Ok));
    if (ret == QMessageBox::Cancel) return;

    for (int i = 0; i < toDeleteList.length(); ++i) {
        QString name = toDeleteList.at(i)->text();
        QString sKey = settingPath + "/" + name;
        int row = toDeleteList.at(i)->row();
        /*
        qDebug() << __FUNCTION__ << i
                 << name
                 << row
                 << sKey
                    ;
//                    */

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
