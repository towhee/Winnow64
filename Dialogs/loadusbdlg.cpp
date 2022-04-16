#include "loadusbdlg.h"
#include "ui_loadusbdlg.h"
#include <QDebug>

LoadUsbDlg::LoadUsbDlg(QWidget *parent, QStringList &usbDrives, QString &selectedDrive) :
    QDialog(parent),
    ui(new Ui::LoadUsbDlg),
    usbDrives(usbDrives),
    selectedDrive(selectedDrive)
{
    ui->setupUi(this);

    // load list of usb drives
    foreach (const QString &s, usbDrives) {
        ui->usbList->addItem(s);
    }
    ui->usbList->setCurrentRow(0);

    setScreenDependencies();

//    // set row height
//    for (int i = 0; i < ui->usbList->count(); ++i) {
//        QListWidgetItem *item = ui->usbList->item(i);
//        item->setSizeHint(QSize(item->sizeHint().width(), 30));
//    }
}

LoadUsbDlg::~LoadUsbDlg()
{
    delete ui;
}

void LoadUsbDlg::on_usbList_itemClicked(QListWidgetItem *item)
{
    selectedDrive = item->text();
    accept();
}

void LoadUsbDlg::keyPressEvent(QKeyEvent *event)
{
    QDialog::keyPressEvent(event);
    if (event->key() == Qt::Key_Return) {
        selectedDrive = ui->usbList->currentItem()->text();
        accept();
    }
}

void LoadUsbDlg::paintEvent(QPaintEvent * /*event*/)
{
    setScreenDependencies();
//    QDialog::paintEvent(event);
}

void LoadUsbDlg::setScreenDependencies()
{
    QFontMetrics fm(this->font());
    int w = fm.boundingRect("==Select USB drive to view images=====").width();
    int h = fm.boundingRect("X").height();

    qDebug() << __PRETTY_FUNCTION__ << "w =" << w;

    // set row width/height
    for (int i = 0; i < ui->usbList->count(); ++i) {
        QListWidgetItem *item = ui->usbList->item(i);
        item->setSizeHint(QSize(w, h));
    }

    QRect r = ui->label->geometry();
    r.setWidth(w);
    ui->label->setGeometry(r);

//    resize(w, this->height());
//    adjustSize();
}
