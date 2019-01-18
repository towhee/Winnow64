#include "updateapp.h"
#include "ui_updateapp.h"

UpdateApp::UpdateApp(QString &notes, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::UpdateApp)
{
    ui->setupUi(this);
    ui->textBrowser->setHtml(notes);
}

UpdateApp::~UpdateApp()
{
    delete ui;
}

void UpdateApp::on_buttonBox_accepted()
{
//    return true;
}

void UpdateApp::on_buttonBox_rejected()
{
//    return false;
}
