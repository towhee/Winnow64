#ifndef COPYPICKDLG_H
#define COPYPICKDLG_H

#include <QDialog>
#include "thumbview.h"

namespace Ui {
class CopyPickDlg;
}

class CopyPickDlg : public QDialog
{
    Q_OBJECT

public:
    explicit CopyPickDlg(QWidget *parent,
                         QFileInfoList &imageList,
                         Metadata *metadata);
    ~CopyPickDlg();

private slots:
    void on_selectFolderBtn_clicked();
    void on_spinBoxStartNumber_valueChanged(const QString &arg1);
    void updateFolderPath();
    void on_descriptionLineEdit_textChanged(const QString &arg1);

private:
    Ui::CopyPickDlg *ui;
    void accept();
    int getSequenceStart(const QString &path);
    QStringList *existFiles;
    QFileInfoList pickList;
    Metadata *metadata;
    QString folderPath;
    QString folderDescription;
    QString fileNameDatePrefix;
    int fileCount;
    float fileMB;
};

#endif // COPYPICKDLG_H
