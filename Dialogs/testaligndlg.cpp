#include "testaligndlg.h"
#include "ui_testaligndlg.h"

TestAlignDlg::TestAlignDlg(QImage *target, QImage *image, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::TestAlignDlg)
{
    ui->setupUi(this);
    QPixmap p;
    p = QPixmap::fromImage(*target);
    ui->targetLbl->setPixmap(p.scaled(150, 150, Qt::KeepAspectRatio));
    ui->alignLbl->setPixmap(QPixmap::fromImage(*image));
}

TestAlignDlg::~TestAlignDlg()
{
    delete ui;
}

void TestAlignDlg::on_buttonBox_accepted()
{

}
