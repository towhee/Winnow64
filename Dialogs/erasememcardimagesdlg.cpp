#include "erasememcardimagesdlg.h"
#include "ui_erasememcardimagesdlg.h"
#include <QDebug>

class EraseMemCardDelegate : public QStyledItemDelegate
{
public:
    explicit EraseMemCardDelegate(QObject *parent = nullptr) : QStyledItemDelegate(parent) { }
    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex  &/*index*/) const override
    {
        int height = qRound(G::strFontSize.toInt() * 1.7 * G::ptToPx);
        return QSize(option.rect.width(), height);
    }
};

EraseMemCardDlg::EraseMemCardDlg(QWidget *parent, QStringList &usbDrives, QString &selectedDrive) :
    QDialog(parent),
    ui(new Ui::EraseMemCardDlg),
    usbDrives(usbDrives),
    selectedDrive(selectedDrive)
{
    ui->setupUi(this);

    // adjust row height in list
    ui->usbList->setItemDelegate(new EraseMemCardDelegate(ui->usbList));

    setStyleSheet(G::css);

    // load list of usb drives
    foreach (const QString &s, usbDrives) {
        ui->usbList->addItem(s);
    }
    ui->usbList->setCurrentRow(0);

    #ifdef Q_OS_WIN
        Win::setTitleBarColor(winId(), G::backgroundColor);
    #endif
}

EraseMemCardDlg::~EraseMemCardDlg()
{
    delete ui;
}

void EraseMemCardDlg::on_eraseBtn_clicked()
{
    selectedDrive = ui->usbList->currentItem()->text();
    accept();
}

void EraseMemCardDlg::on_cancelBtn_clicked()
{
    reject();
}

