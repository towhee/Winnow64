#include "managetilesdlg.h"
#include "ui_managetilesdlg.h"
#include "Main/global.h"

/*
    Delete and rename tiles.

    Tiles are small pixmaps that are saved as QByteArray in QSettings.  When the dialog is
    instantiated the tiles are read from QSettings and two objects are populated with the
    tile list: QStringList tiles and QComboBox ui->tileBox;

    The QStringList tiles function maintains the prior status of ui->tileBox. When a tile name
    (text) in ui->tileBox is edited, comparing to the same index in tiles determines if the
    tile name has changed.

    If the tile name has changed then the QSettings tile copied to the new name and the old
    QSettings tile is deleted.  The QStringList tiles is updated to match the ui->tileBox
    stringList.
*/

ManageTilesDlg::ManageTilesDlg(QSettings *setting, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::ManageTilesDlg)
{
    ui->setupUi(this);
    setStyleSheet(G::css);

    // change ui->tileBox model to QStringList to facilitate debugging
    tileBoxModel = new QStringListModel;
    ui->tileBox->setModel(tileBoxModel);

    // read list of tiles from QSettings
    this->setting = setting;
    setting->beginGroup("Embel/Tiles");
    tiles << setting->allKeys();
    setting->endGroup();

    // load combobox with list of tiles
    ui->tileBox->addItems(tiles);
    ui->tileBox->setEditable(true);

    connect(ui->tileBox, &QComboBox::currentTextChanged, this, &ManageTilesDlg::textChange);
    connect(ui->tileBox->lineEdit(), &QLineEdit::editingFinished,
            this, &ManageTilesDlg::editingFinished);
    connect(ui->tileBox, QOverload<int>::of(&QComboBox::activated),
            [=](int index){activate(index);});
    textHasBeenEdited = false;
    textEditedIndex = 0;
    activate(0);
}

ManageTilesDlg::~ManageTilesDlg()
{
    delete ui;
}

void ManageTilesDlg::editingFinished()
{
/*
   When a tile name is edited rename the QSettings and tiles for the item.
*/
    if (!textHasBeenEdited) return;

    // rename setting and tiles
    int index = ui->tileBox->currentIndex();
    QString oldKey = tiles.at(textEditedIndex);
    QString newKey = ui->tileBox->lineEdit()->text();
    qDebug() << "ManageTilesDlg::editingFinished" << "tiles   " << tiles;
    qDebug() << "ManageTilesDlg::editingFinished" << "tileBox " << tileBoxModel->stringList();
    qDebug() << "ManageTilesDlg::editingFinished"
             << "oldKey =" << oldKey
             << "newKey =" << newKey
             << "textHasBeenEdited =" << textHasBeenEdited
             << "textEditedIndex =" << textEditedIndex
             << "index =" << index
             << "ui->tileBox->currentIndex() =" << ui->tileBox->currentIndex()
                ;

    // get QSettings value (QByteArray of tile image) from old key
    QByteArray ba = setting->value("Embel/Tiles/" + oldKey).toByteArray();
    // QSettings copy to new key
    setting->setValue("Embel/Tiles/" + newKey, ba);
    // QSettings remove old key
    setting->remove("Embel/Tiles/" + oldKey);
    // tiles replace old key to new key
    tiles[index] = newKey;

    textHasBeenEdited = false;
}

void ManageTilesDlg::textChange(QString text)
{
/*
    Triggered when the tileBox text changes, either when a new item is selected or when the
    text is edited. If it is a new item then the text in tiles and tileBox for the index will
    be the same. If it is not then set a flag (textHasBeenEdited = false) and the slots
    "activate" and "editingFinished" will ignore signals.
*/
    // if extract new tile then index may not have been built
    if (ui->tileBox->count() <= 0) return;

    // has text changed or just enter pressed - compare to tiles
    int index = ui->tileBox->currentIndex();
    QString tilesText = tiles.at(index);
    if (tilesText == text) {
        textHasBeenEdited = false;
    }
    else {
        textHasBeenEdited = true;
        textEditedIndex = index;
        ui->tileBox->setItemText(index, text);
    }
    /*
    qDebug();
    qDebug() << "ManageTilesDlg::" << "tiles   " << tiles;
    qDebug() << "ManageTilesDlg::" << "tileBox " << tileBoxModel->stringList();
    qDebug() << "ManageTilesDlg::"
             << "text =" << text
             << "tilesText =" << tilesText
             << "textHasBeenEdited =" << textHasBeenEdited
             << "textEditedIndex =" << textEditedIndex
             << "ui->tileBox->currentIndex() =" << ui->tileBox->currentIndex()
                ;
//                */
}

void ManageTilesDlg::activate(int index)
{
/*
    Show pixmap when the tileBox selection changes.
*/
    // if trigged by textChange but the text is being edited then not a new tile selection
    if (textHasBeenEdited) return;
    if (index >= tiles.size()) {
        qDebug() << "ManageTilesDlg::activate"
                 << "Invalidindex =" << index;
        return;
    }
    QString sKey = "Embel/Tiles/" + tiles.at(index);
    /*
    qDebug() << "ManageTilesDlg::"
             << "textHasBeenEdited =" << textHasBeenEdited
             << "textEditedIndex =" << textEditedIndex
             << "index =" << index
             << "ui->tileBox->currentIndex() =" << ui->tileBox->currentIndex()
             << "setting key =" << sKey
                ;
//                */

    // get the new tile
    QByteArray tileBa = setting->value(sKey).toByteArray();
    QPixmap pm;
    pm.loadFromData(tileBa);
    ui->tileLabel->setPixmap(pm);
}

void ManageTilesDlg::on_deleteBtn_clicked()
{
/*
    Delete the selected tile.
*/
    int index = ui->tileBox->currentIndex();
    QString tileName = tiles.at(index);
    int ret = (QMessageBox::warning(this, "Delete Tile", "Confirm delete tile " + tileName + "                     ",
                             QMessageBox::Cancel | QMessageBox::Ok));
    if (ret == QMessageBox::Cancel) return;
    QString sKey = "Embel/Tiles/" + tileName;
    /*
    qDebug();
    qDebug() << "ManageTilesDlg::" << "tiles   " << tiles;
    qDebug() << "ManageTilesDlg::" << "tileBox " << tileBoxModel->stringList();
    qDebug() << "ManageTilesDlg::"
             << "index =" << index
             << "sKey =" << sKey
                ;
//                */
    // note ui->tileBox item must be removed last as it triggers textChange
    tiles.removeAt(index);
    setting->remove(sKey);
    ui->tileBox->removeItem(index);
}

void ManageTilesDlg::on_closeBtn_clicked()
{
    accept();
}

void ManageTilesDlg::on_newBtn_clicked()
{
    emit extractTile();

    QString currentTile = ui->tileBox->currentText();
    tiles.clear();

    // reread list of tiles from QSettings in case new tiles
    setting->beginGroup("Embel/Tiles");
    tiles << setting->allKeys();
    setting->endGroup();

    ui->tileBox->clear();
    ui->tileBox->addItems(tiles);
    ui->tileBox->setCurrentText(currentTile);
}
