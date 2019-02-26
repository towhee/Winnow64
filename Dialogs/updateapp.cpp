#include "updateapp.h"
#include "ui_updateapp.h"

UpdateApp::UpdateApp(QString &version, QString &css, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::UpdateApp),
    css(css)
{
    ui->setupUi(this);

    ui->versionLabel->setText("You currently have version " + version);

    // req'd for hyperlink to open browser
    ui->viewVersionLabel->setTextInteractionFlags(Qt::TextBrowserInteraction);
    ui->viewVersionLabel->setOpenExternalLinks(true);
}

UpdateApp::~UpdateApp()
{
    delete ui;
}

void UpdateApp::showEvent(QShowEvent *event)
{
    setStyleSheet(css);
    QDialog::showEvent(event);
}


void UpdateApp::on_yesBtn_clicked()
{
    accept();
}

void UpdateApp::on_cancelBtn_clicked()
{
    reject();
}
