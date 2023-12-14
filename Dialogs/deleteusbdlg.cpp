#include "deleteusbdlg.h"
#include "ui_deleteusbdlg.h"
#include <QDebug>

class DeleteUsbDelegate : public QStyledItemDelegate
{
public:
    explicit DeleteUsbDelegate(QObject *parent = nullptr) : QStyledItemDelegate(parent) { }
    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex  &/*index*/) const override
    {
        int height = qRound(G::strFontSize.toInt() * 1.7 * G::ptToPx);
        return QSize(option.rect.width(), height);
    }
};

DeleteUsbDlg::DeleteUsbDlg(QWidget *parent, QStringList &usbDrives, QString &selectedDrive) :
    QDialog(parent),
    ui(new Ui::DeleteUsbDlg),
    usbDrives(usbDrives),
    selectedDrive(selectedDrive)
{
    ui->setupUi(this);

    // adjust row height in list
    ui->usbList->setItemDelegate(new DeleteUsbDelegate(ui->usbList));

    setStyleSheet(G::css);

    // load list of usb drives
    foreach (const QString &s, usbDrives) {
        ui->usbList->addItem(s);
    }
    ui->usbList->setCurrentRow(0);

    setScreenDependencies();

    #ifdef Q_OS_WIN
        Win::setTitleBarColor(winId(), G::backgroundColor);
    #endif
}

DeleteUsbDlg::~DeleteUsbDlg()
{
    delete ui;
}

void DeleteUsbDlg::on_usbList_itemClicked(QListWidgetItem *item)
{
    selectedDrive = item->text();
    accept();
}

void DeleteUsbDlg::keyPressEvent(QKeyEvent *event)
{
    QDialog::keyPressEvent(event);
    if (event->key() == Qt::Key_Return) {
        selectedDrive = ui->usbList->currentItem()->text();
        accept();
    }
}

void DeleteUsbDlg::paintEvent(QPaintEvent * /*event*/)
{
    setScreenDependencies();
}

void DeleteUsbDlg::setScreenDependencies()
{
    QFontMetrics fm(this->font());
    int w = fm.boundingRect("==Select USB drive to view images=====").width();
    int h = fm.boundingRect("X").height();

    // set row width/height
    for (int i = 0; i < ui->usbList->count(); ++i) {
        QListWidgetItem *item = ui->usbList->item(i);
        item->setSizeHint(QSize(w, h));
    }

    QRect r = ui->label->geometry();
    r.setWidth(w);
    ui->label->setGeometry(r);
}
