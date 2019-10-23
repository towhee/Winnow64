#ifndef LOADUSBDLG_H
#define LOADUSBDLG_H

#include <QDialog>
#include <QtWidgets>

namespace Ui {
class LoadUsbDlg;
}

class LoadUsbDlg : public QDialog
{
    Q_OBJECT

public:
    explicit LoadUsbDlg(QWidget *parent, QStringList &usbDrives, QString &selectedDrive);
    ~LoadUsbDlg();

protected:
    void keyPressEvent(QKeyEvent *event);
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
