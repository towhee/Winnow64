#include "processdlg.h"
#include "ui_processdlg.h"

processdlg::processdlg(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::processdlg)
{
    ui->setupUi(this);
}

processdlg::~processdlg()
{
    delete ui;
}

void processdlg::on_addBtn_clicked()
{

}

void processdlg::on_removeBtn_clicked()
{

}
