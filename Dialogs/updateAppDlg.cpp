#include "updateAppDlg.h"
#include "ui_updateAppDlg.h"

UpdateAppDlg::UpdateAppDlg(const QString &newVersion, const QString &currentVersion,
                           const QString &notesUrl, QString &css, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::UpdateAppDlg),
    css(css)
{
    ui->setupUi(this);

    ui->versionLabel->setText("Winnow " + newVersion + " is available.  "
                              "You have version " + currentVersion + ".");

    // req'd for hyperlink to open browser
    ui->viewVersionLabel->setTextInteractionFlags(Qt::TextBrowserInteraction);
    ui->viewVersionLabel->setOpenExternalLinks(true);
    if (!notesUrl.isEmpty()) {
        ui->viewVersionLabel->setText(
            "<html><head/><body><p>You can view the release notes "
            "<a href=\"" + notesUrl + "\"><span style=\" text-decoration: underline; "
            "color:#ffffff;\">here</span></a>.</p></body></html>");
    }
    else {
        ui->viewVersionLabel->hide();
    }

    #ifdef Q_OS_WIN
        Win::setTitleBarColor(winId(), G::backgroundColor);
    #endif
}

UpdateAppDlg::~UpdateAppDlg()
{
    delete ui;
}

void UpdateAppDlg::showEvent(QShowEvent *event)
{
    setStyleSheet(css);
    QDialog::showEvent(event);
}

void UpdateAppDlg::on_yesBtn_clicked()
{
    accept();
}

void UpdateAppDlg::on_cancelBtn_clicked()
{
    reject();
}

void UpdateAppDlg::setInstallMode(bool inPlace, const QString &reason)
{
    if (inPlace) {
        ui->yesBtn->setText("Install");
        ui->descriptionLabel->setText(
            "Install downloads the update, quits Winnow, installs the new version and "
            "reopens Winnow.");
    }
    else {
        ui->yesBtn->setText("Download");
        QString text = "Download saves the installer to your Downloads folder and opens "
                       "it.  Quit Winnow, then install the new version.";
        if (!reason.isEmpty())
            text = reason + "  " + text;
        ui->descriptionLabel->setText(text);
    }
}

bool UpdateAppDlg::skipRequested() const
{
    return ui->dontAskChk->isChecked();
}
