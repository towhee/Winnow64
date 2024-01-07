#include "viscmpdlg.h"
#include "ui_viscmpdlg.h"

VisCmpDlg::VisCmpDlg(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::VisCmpDlg)
{
    ui->setupUi(this);
}

VisCmpDlg::~VisCmpDlg()
{
    delete ui;
}
