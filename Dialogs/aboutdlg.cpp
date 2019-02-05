#include "aboutdlg.h"
#include "ui_aboutdlg.h"

AboutDlg::AboutDlg(QWidget *parent,
                   QString &version,
                   QString &qtVersion,
                   QString &website) :
    QDialog(parent),
    ui(new Ui::About)
{
    ui->setupUi(this);
//    ui->webSite->setText(website);
    ui->qtVersion->setText(qtVersion);
    ui->version->setText(version);
//    ui->version->setTextFormat(Qt::RichText);
//    ui->version->setTextInteractionFlags(Qt::TextBrowserInteraction);
//    ui->version->setOpenExternalLinks(true);
//    QDesktopServices::openUrl(QUrl("http://165.227.46.158/winnow/winnow.html"));
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
