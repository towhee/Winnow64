#include "managegraphicsdlg.h"
#include "ui_managegraphicsdlg.h"
#include "Main/global.h"

/*
    Delete and rename graphics.

    Graphics are small pixmaps that are saved as QByteArray in QSettings.  When the dialog is
    instantiated the graphics are read from QSettings and two objects are populated with the
    graphic list: QStringList graphics and QComboBox ui->graphicsBox;

    The QStringList graphics function maintains the prior status of ui->graphicsBox. When a
    graphic name (text) in ui->graphicsBox is edited, comparing to the same index in graphics
    determines if the graphic name has changed.

    If the graphic name has changed then the QSettings graphic copied to the new name and the
    old QSettings graphic is deleted. The QStringList graphics is updated to match the
    ui->graphicsBox stringList.
*/

ManageGraphicsDlg::ManageGraphicsDlg(QSettings *setting, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::ManageGraphicsDlg)
{
    ui->setupUi(this);
    setStyleSheet(G::css);

    // change ui->graphicsBox model to QStringList to facilitate debugging
//    graphicsBoxModel = new QStringListModel;
//    ui->graphicsBox->setModel(graphicsBoxModel);

    // read list of graphics from QSettings
    this->setting = setting;
    setting->beginGroup("Embel/Graphics");
    qDebug() << __FUNCTION__ << setting->allKeys();
    graphicsList << setting->allKeys();
    setting->endGroup();
    qDebug() << __FUNCTION__ << graphicsList;

    // load combobox with list of graphics
    ui->graphicsBox->addItems(graphicsList);
    ui->graphicsBox->setEditable(true);

    connect(ui->graphicsBox, &QComboBox::currentTextChanged, this, &ManageGraphicsDlg::textChange);
    connect(ui->graphicsBox->lineEdit(), &QLineEdit::editingFinished,
            this, &ManageGraphicsDlg::editingFinished);
    connect(ui->graphicsBox, QOverload<int>::of(&QComboBox::activated),
            [=](int index){activate(index);});
    textHasBeenEdited = false;
    textEditedIndex = 0;
    activate(0);
}

ManageGraphicsDlg::~ManageGraphicsDlg()
{
    delete ui;
}

void ManageGraphicsDlg::editingFinished()
{
/*
   When a graphic name is edited rename the QSettings and graphics for the item.
*/
    if (!textHasBeenEdited) return;

    // rename setting and graphics
    int index = ui->graphicsBox->currentIndex();
    QString oldKey = graphicsList.at(textEditedIndex);
    QString newKey = ui->graphicsBox->lineEdit()->text();
    qDebug() << __FUNCTION__ << "graphicss   " << graphicsList;
//    qDebug() << __FUNCTION__ << "graphicsBox " << graphicsBoxModel->stringList();
    qDebug() << __FUNCTION__
             << "oldKey =" << oldKey
             << "newKey =" << newKey
             << "textHasBeenEdited =" << textHasBeenEdited
             << "textEditedIndex =" << textEditedIndex
             << "index =" << index
             << "ui->graphicsBox->currentIndex() =" << ui->graphicsBox->currentIndex()
                ;

    // get QSettings value (QByteArray of graphic image) from old key
    QByteArray ba = setting->value("Embel/Graphics/" + oldKey).toByteArray();
    // QSettings copy to new key
    setting->setValue("Embel/Graphics/" + newKey, ba);
    // QSettings remove old key
    setting->remove("Embel/Graphics/" + oldKey);
    // graphics replace old key to new key
    graphicsList[index] = newKey;

    textHasBeenEdited = false;
}

void ManageGraphicsDlg::textChange(QString text)
{
/*
    Triggered when the graphicsBox text changes, either when a new item is selected or when
    the text is edited. If it is a new item then the text in graphics and graphicsBox for the
    index will be the same. If it is not then set a flag (textHasBeenEdited = false) and the
    slots "activate" and "editingFinished" will ignore signals.
*/
    // if extract new graphic then index may not have been built
    if (ui->graphicsBox->count() <= 0) return;

    // has text changed or just enter pressed - compare to graphics
    int index = ui->graphicsBox->currentIndex();
    QString graphicsText = graphicsList.at(index);
    if (graphicsText == text) {
        textHasBeenEdited = false;
    }
    else {
        textHasBeenEdited = true;
        textEditedIndex = index;
        ui->graphicsBox->setItemText(index, text);
    }
    /*
    qDebug();
    qDebug() << __FUNCTION__ << "graphics   " << graphics;
    qDebug() << __FUNCTION__ << "graphicsBox " << graphicsBoxModel->stringList();
    qDebug() << __FUNCTION__
             << "text =" << text
             << "graphicsText =" << graphicsText
             << "textHasBeenEdited =" << textHasBeenEdited
             << "textEditedIndex =" << textEditedIndex
             << "ui->graphicsBox->currentIndex() =" << ui->graphicsBox->currentIndex()
                ;
//                */
}

void ManageGraphicsDlg::activate(int index)
{
/*
    Show pixmap when the graphicsBox selection changes.
*/
    // if trigged be textChange but the text is being edited then not a new graphic selection
qDebug() << __FUNCTION__ << "textHasBeenEdited =" << textHasBeenEdited;
//    if (textHasBeenEdited) return;
    if (index >= graphicsList.size()) {
        qDebug() << __FUNCTION__
                 << "Invalidindex =" << index;
        return;
    }
    QString sKey = "Embel/Graphics/" + graphicsList.at(index);
    /*
    qDebug() << __FUNCTION__
             << "textHasBeenEdited =" << textHasBeenEdited
             << "textEditedIndex =" << textEditedIndex
             << "index =" << index
             << "ui->graphicsBox->currentIndex() =" << ui->graphicsBox->currentIndex()
             << "setting key =" << sKey
                ;
//                */

    // get the new graphic
    QByteArray graphicBa = setting->value(sKey).toByteArray();
    QPixmap pm;
    pm.loadFromData(graphicBa);
    ui->graphicsLabel->setPixmap(pm);
}

void ManageGraphicsDlg::on_deleteBtn_clicked()
{
/*
    Delete the selected graphic.
*/
    int index = ui->graphicsBox->currentIndex();
    QString graphicName = graphicsList.at(index);
    int ret = (QMessageBox::warning(this, "Delete Tile", "Confirm delete graphic " + graphicName + "                     ",
                             QMessageBox::Cancel | QMessageBox::Ok));
    if (ret == QMessageBox::Cancel) return;
    QString sKey = "Embel/Graphics/" + graphicName;
    /*
    qDebug();
    qDebug() << __FUNCTION__ << "graphics   " << graphics;
    qDebug() << __FUNCTION__ << "graphicsBox " << graphicsBoxModel->stringList();
    qDebug() << __FUNCTION__
             << "index =" << index
             << "sKey =" << sKey
                ;
//                */
    // note ui->graphicsBox item must be removed last as it triggers textChange
    graphicsList.removeAt(index);
    setting->remove(sKey);
    ui->graphicsBox->removeItem(index);

    index = ui->graphicsBox->currentIndex();
    activate(index);
}

void ManageGraphicsDlg::on_closeBtn_clicked()
{
    accept();
}

void ManageGraphicsDlg::on_newBtn_clicked()
{
    /* In EmbelProperties get new graphic, add to settings and update all Grapices items.
       Signal back to ManageGraphicsDlg::updateList()  */
    emit getGraphic();

//    updateList();
}

void ManageGraphicsDlg::updateList()
{
    QString currentTile = ui->graphicsBox->currentText();
    graphicsList.clear();
//    int count = graphicsBoxModel->rowCount();
//    graphicsBoxModel->removeRows(0, count);

    // reread list of graphics from QSettings in case new graphics
    setting->beginGroup("Embel/Graphics");
    graphicsList << setting->allKeys();
    setting->endGroup();

    qDebug() << __FUNCTION__ << graphicsList;

    ui->graphicsBox->clear();
    ui->graphicsBox->addItems(graphicsList);
    ui->graphicsBox->setCurrentText(currentTile);
}
