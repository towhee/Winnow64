#ifndef COPYPICKDLG_H
#define COPYPICKDLG_H

#include <QDialog>
#include <QtWidgets>
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
                         Metadata *metadata,
                         QString ingestRootFolder);
    ~CopyPickDlg();

private slots:
    void on_selectFolderBtn_clicked();
    void on_selectParentFolderBtn_clicked();
    void on_spinBoxStartNumber_valueChanged(const QString &arg1);
    void updateFolderPath();
    void on_descriptionLineEdit_textChanged(const QString &arg1);

signals:
    void updateIngestRootFolder(QString rootFolderPath);

private:
    Ui::CopyPickDlg *ui;
    void accept();
    void updateExistingSequence();
    int getSequenceStart(const QString &path);

    QStringList *existFiles;
    QFileInfoList pickList;
    QFileSystemModel fileSystemModel;
    Metadata *metadata;
    QString folderPath; // rootFolderPath + folderBase + folderDescription
    QString defaultRootFolderPath;

    QString rootFolderPath;
    QString folderBase;
    QString folderDescription;
    QString fileNameDatePrefix;
    QString fileNameSequence;
    QString fileSuffix;

    QString dateTime;
    QString year;
    QString month;

    int fileCount;
    float fileMB;
};

#endif // COPYPICKDLG_H
