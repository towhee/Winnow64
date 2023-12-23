#ifndef ERASEMEMCARDDLG_H
#define ERASEMEMCARDDLG_H

#include <QDialog>
#include <QtWidgets>
#include "Main/global.h"
#ifdef Q_OS_WIN
#include "Utilities/win.h"
#endif

namespace Ui {
class EraseMemCardDlg;
}

class EraseMemCardDlg : public QDialog
{
    Q_OBJECT

public:
    explicit EraseMemCardDlg(QWidget *parent, QStringList &usbDrives, QString &selectedDrive);
    ~EraseMemCardDlg() override;

private slots:
    void on_eraseBtn_clicked();
    void on_cancelBtn_clicked();

private:
    Ui::EraseMemCardDlg *ui;
    QStringList &usbDrives;
    QString &selectedDrive;
};

#endif // ERASEMEMCARDDLG_H
