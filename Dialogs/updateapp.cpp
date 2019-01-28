#include "updateapp.h"
#include "ui_updateapp.h"

UpdateApp::UpdateApp(QString &version, QString &css, QWidget *parent) :
    css(css),
    QDialog(parent),
    ui(new Ui::UpdateApp)
{
    ui->setupUi(this);

    ui->versionLabel->setText("You currently have version " + version);
//    ui->buttonBox->button(QDialogButtonBox::Yes)->setDefault(false);

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
}

void UpdateApp::on_buttonBox_accepted()
{
//    return true;
}

void UpdateApp::on_buttonBox_rejected()
{
//    return false;
}
