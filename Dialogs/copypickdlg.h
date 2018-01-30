#ifndef COPYPICKDLG_H
#define COPYPICKDLG_H

#include <QDialog>
#include <QtWidgets>
#include "thumbview.h"
#include "tokendlg.h"

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
                         QString ingestRootFolder,
                         bool isAuto);
    ~CopyPickDlg();

private slots:
    void on_selectFolderBtn_clicked();
    void on_selectRootFolderBtn_clicked();
    void on_spinBoxStartNumber_valueChanged(const QString &arg1);
    void updateFolderPath();
    void on_descriptionLineEdit_textChanged(const QString &);

//    void on_autoRadio_clicked();

//    void on_manualRadio_clicked();

//    void on_descriptionLineEdit_selectionChanged();

    void on_autoRadio_toggled(bool checked);

    void on_tokenEditorBtn_clicked();

signals:
    void updateIngestParameters(QString rootFolderPath, bool isAuto);

private:
    Ui::CopyPickDlg *ui;
    void accept();
    void buildFileNameSequence();
    void updateExistingSequence();
    int getSequenceStart(const QString &path);
    void updateStyleOfFolderLabels();

    bool isInitializing;
    bool isAuto;

    QStringList *existFiles;
    QFileInfoList pickList;
    QFileSystemModel fileSystemModel;
    Metadata *metadata;
    QString folderPath; // rootFolderPath + folderBase + folderDescription
    QString defaultRootFolderPath;

    QString rootFolderPath;
    QString pathToBaseFolder;
    QString folderBase;
    QString folderDescription;
    QString fileNameDatePrefix;
    QString fileNameSequence;
    QString fileSuffix;

    QString created;
    QString year;
    QString month;

    int fileCount;
    float fileMB;
};

#endif // COPYPICKDLG_H
