#ifndef TESTALIGNDLG_H
#define TESTALIGNDLG_H

#include <QDialog>
#ifdef Q_OS_WIN
#include "Utilities/win.h"
#endif

namespace Ui {
class TestAlignDlg;
}

class TestAlignDlg : public QDialog
{
    Q_OBJECT

public:
    explicit TestAlignDlg(QImage *target, QImage *image, QWidget *parent = 0);
    ~TestAlignDlg();

private slots:
    void on_buttonBox_accepted();

private:
    Ui::TestAlignDlg *ui;
};

#endif // TESTALIGNDLG_H
