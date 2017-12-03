#include "zoomdlg.h"
#include "ui_zoomdlg.h"

ZoomDlg::ZoomDlg(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::ZoomDlg)
{
    ui->setupUi(this);
}

ZoomDlg::~ZoomDlg()
{
    delete ui;
}
