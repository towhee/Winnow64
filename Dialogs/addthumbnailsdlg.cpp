#include "addthumbnailsdlg.h"
#include "ui_addthumbnailsdlg.h"
#include "main/global.h"
#ifdef Q_OS_WIN
#include "Utilities/win.h"
#endif

AddThumbnailsDlg::AddThumbnailsDlg(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::AddThumbnailsDlg)
{
    ui->setupUi(this);
    setStyleSheet(G::css);
    ui->backupChk->setChecked(G::backupBeforeModifying);
    if (!G::modifySourceFiles) ui->addBtn->setEnabled(false);
    #ifdef Q_OS_WIN
        Win::setTitleBarColor(winId(), G::backgroundColor);
    #endif
}

AddThumbnailsDlg::~AddThumbnailsDlg()
{
    delete ui;
}

void AddThumbnailsDlg::on_backupChk_clicked()
{
    qDebug() << "AddThumbnailsDlg::on_cancelBtn_clicked";
    bool isBackup = ui->backupChk->isChecked();
    emit backup(isBackup);
}

void AddThumbnailsDlg::on_ignoreChk_clicked()
{
    qDebug() << "AddThumbnailsDlg::on_ignoreChk_clicked";
    emit ignore();
}

void AddThumbnailsDlg::on_cancelBtn_clicked()
{
    qDebug() << "AddThumbnailsDlg::on_cancelBtn_clicked";
    reject();
}

void AddThumbnailsDlg::on_addBtn_clicked()
{
    qDebug() << "AddThumbnailsDlg::on_addBtn_clicked";
    accept();
}
