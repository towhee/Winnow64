#include "aboutdlg.h"
#include "ui_aboutdlg.h"

AboutDlg::AboutDlg(QWidget *parent,
                   QString &version,
                   QString &qtVersion) :
    QDialog(parent),
    ui(new Ui::About)
{
    ui->setupUi(this);
    ui->qtVersion->setText(qtVersion);
    ui->version->setText(version);
}

AboutDlg::~AboutDlg()
{
    delete ui;
}

void AboutDlg::on_okButton_clicked()
{
    accept();
}

void AboutDlg::on_email_linkActivated(const QString &link)
{
    QString preventcompilerWarning = link;
    QDesktopServices::openUrl(QUrl("mailto:winnowimageviewer@outlook.com"));
}

void AboutDlg::on_webSite_linkActivated(const QString &link)
{
    QString preventcompilerWarning = link;
    QDesktopServices::openUrl(QUrl("http://165.227.46.158/winnow/winnow.html?subject=Winnow&body=Hi Rory"));
}
