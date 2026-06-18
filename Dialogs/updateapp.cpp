#include "updateapp.h"
#include "ui_updateapp.h"

UpdateApp::UpdateApp(const QString &newVersion, const QString &currentVersion,
                     const QString &notesUrl, QString &css, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::UpdateApp),
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
            "<html><head/><body><p>You can view the version update notes "
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

void UpdateApp::on_skipBtn_clicked()
{
    skip = true;
    reject();
}
