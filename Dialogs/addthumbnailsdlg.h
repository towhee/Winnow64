#ifndef ADDTHUMBNAILSDLG_H
#define ADDTHUMBNAILSDLG_H

#include <QDialog>

namespace Ui {
class AddThumbnailsDlg;
}

class AddThumbnailsDlg : public QDialog
{
    Q_OBJECT

public:
    explicit AddThumbnailsDlg(QWidget *parent = nullptr);
    ~AddThumbnailsDlg();

signals:
    void ignore(bool ignore);
    void backup(bool isBackup);

private slots:
    void on_backupChk_clicked();
    void on_ignoreChk_clicked();
    void on_cancelBtn_clicked();
    void on_addBtn_clicked();

private:
    Ui::AddThumbnailsDlg *ui;
};

#endif // ADDTHUMBNAILSDLG_H
