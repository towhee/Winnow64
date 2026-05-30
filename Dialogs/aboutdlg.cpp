#include "aboutdlg.h"
#include "ui_aboutdlg.h"
#include "Main/global.h"

AboutDlg::AboutDlg(QWidget *parent,
                   QString &version,
                   QString &qtVersion,
                   QString &compileDate) :
    QDialog(parent),
    ui(new Ui::About)
{
    ui->setupUi(this);
    ui->qtVersion->setText(qtVersion);
    ui->version->setText(version);
    ui->compileDate->setText(compileDate);
    QFont titleFont = ui->title->font();
    titleFont.setPixelSize(static_cast<int>(G::strFontSize.toInt() * G::ptToPx));
    ui->title->setFont(titleFont);
    adjustSize();
    #ifdef Q_OS_WIN
    Win::setTitleBarColor(winId(), G::backgroundColor);
    #endif
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
    QDesktopServices::openUrl(QUrl("http://winnow.ca/"));
//    QDesktopServices::openUrl(QUrl("http://165.227.46.158/winnow/winnow.html?subject=Winnow&body=Hi Rory"));
}
