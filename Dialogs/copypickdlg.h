#ifndef COPYPICKDLG_H
#define COPYPICKDLG_H

#include <QDialog>
#include <QtWidgets>
#include "thumbview.h"
#include "tokendlg.h"

#include "ui_helpingest.h"

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
                         QMap<QString, QString>& pathTemplates,
                         QMap<QString, QString>& filenameTemplates,
                         int& pathTemplateSelected,
                         int& filenameTemplateSelected,
                         bool isAuto);
    ~CopyPickDlg();

private slots:
    void updateFolderPath();
    void on_selectFolderBtn_clicked();
    void on_selectRootFolderBtn_clicked();
    void on_spinBoxStartNumber_valueChanged(const QString &arg1);
    void on_descriptionLineEdit_textChanged(const QString &);
    void on_autoRadio_toggled(bool checked);
    void on_pathTemplatesBtn_clicked();

    void on_pathTemplatesCB_currentIndexChanged(const QString &arg1);

    void on_filenameTemplatesBtn_clicked();

    void on_filenameTemplatesCB_currentIndexChanged(const QString &arg1);

    void on_cancelBtn_clicked();

    void on_okBtn_clicked();

    void on_helpBtn_clicked();

signals:
    void updateIngestParameters(QString rootFolderPath, bool isAuto);

private:
    Ui::CopyPickDlg *ui;
    void initTokenMap();
    bool isToken(const QMap<QString,QString>& map, QString tokenString, int pos);
    QString parseTokenString(QFileInfo info, const QMap<QString,QString>& map, QString tokenString);
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

    QMap<QString, QString> tokenMap;
    QMap<QString, QString> &pathTemplatesMap;
    QMap<QString, QString> &filenameTemplatesMap;

    int& pathTemplateSelected;
    int& filenameTemplateSelected;

    QString folderPath; // rootFolderPath + folderBase + folderDescription
    QString defaultRootFolderPath;

    QString rootFolderPath;
    QString pathToBaseFolder;
    QString folderBase;
    QString baseFolderDescription;
    QString fileNameDatePrefix;
    QString fileNameSequence;
    QString fileSuffix;

    QString created;
    QString year;
    QString month;
    QDateTime createdDate;

    int fileCount;
    float fileMB;

    QString currentToken;
    int tokenStart, tokenEnd;
};

#endif // COPYPICKDLG_H
