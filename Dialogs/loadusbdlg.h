#ifndef LOADUSBDLG_H
#define LOADUSBDLG_H

#include <QDialog>
#include <QtWidgets>
#ifdef Q_OS_WIN
#include "Utilities/win.h"
#endif

//class LoadUsbDelegate : public QStyledItemDelegate
////class LoadUsbDelegate : public QAbstractItemDelegate
//{
//public:
//    explicit LoadUsbDelegate(QObject *parent = nullptr);
////    void paint ( QPainter * painter, const QStyleOptionViewItem & option, const QModelIndex & index ) const;
//    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex  &/*index*/) const
//    {
//        int height = qRound(G::strFontSize.toInt() * 1.7 * G::ptToPx);
//        return QSize(option.rect.width(), height);
//    }
//};

namespace Ui {
class LoadUsbDlg;
}

class LoadUsbDlg : public QDialog
{
    Q_OBJECT

public:
    explicit LoadUsbDlg(QWidget *parent, QStringList &usbDrives, QString &selectedDrive);
    ~LoadUsbDlg() override;

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private slots:
    void on_usbList_itemClicked(QListWidgetItem *item);

private:
    Ui::LoadUsbDlg *ui;
    QStringList &usbDrives;
    QString &selectedDrive;
    void setScreenDependencies();
};

#endif // LOADUSBDLG_H
