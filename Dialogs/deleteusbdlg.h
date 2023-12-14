#ifndef DELETEUSBDLG_H
#define DELETEUSBDLG_H

#include <QDialog>
#include <QtWidgets>
#include "Main/global.h"
#ifdef Q_OS_WIN
#include "Utilities/win.h"
#endif

namespace Ui {
class DeleteUsbDlg;
}

class DeleteUsbDlg : public QDialog
{
    Q_OBJECT

public:
    explicit DeleteUsbDlg(QWidget *parent, QStringList &usbDrives, QString &selectedDrive);
    ~DeleteUsbDlg() override;

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private slots:
    void on_usbList_itemClicked(QListWidgetItem *item);

private:
    Ui::DeleteUsbDlg *ui;
    QStringList &usbDrives;
    QString &selectedDrive;
    void setScreenDependencies();
};

#endif // DELETEUSBDLG_H
