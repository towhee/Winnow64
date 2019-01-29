#ifndef INGESTDLG_H
#define INGESTDLG_H

#include <QtWidgets>
#include "Views/thumbview.h"
#include "Datamodel/datamodel.h"
#include "tokendlg.h"

#include "ui_helpingest.h"
#include "Utilities/utilities.h"

namespace Ui {
class IngestDlg;
}

class IngestDlg : public QDialog
{
    Q_OBJECT

public:
    explicit IngestDlg(QWidget *parent,
                       bool &combineRawJpg,
                       bool &autoEjectUsb,
                       bool &isBackup,
                       Metadata *metadata,
                       DataModel *dm,
                       QString &ingestRootFolder,
                       QString &ingestRootFolder2,
                       QString &manualFolderPath,
                       QMap<QString, QString>&pathTemplates,
                       QMap<QString, QString>&filenameTemplates,
                       int &pathTemplateSelected,
                       int &pathTemplateSelected2,
                       int &filenameTemplateSelected,
                       QStringList &ingestDescriptionCompleter,
                       bool &isAuto,
                       QString css);
    ~IngestDlg();

private slots:
    void updateFolderPath();

    void on_autoRadio_toggled(bool checked);
    void on_manualRadio_toggled(bool checked);

    void on_selectFolderBtn_clicked();
    void on_selectRootFolderBtn_2_clicked();
    void on_selectRootFolderBtn_clicked();
    void on_pathTemplatesCB_currentIndexChanged(const QString &arg1);
    void on_pathTemplatesCB_2_currentIndexChanged(const QString &arg1);
    void on_pathTemplatesBtn_clicked();
    void on_pathTemplatesBtn_2_clicked();
    void on_descriptionLineEdit_textChanged(const QString &);

    void on_filenameTemplatesBtn_clicked();
    void on_filenameTemplatesCB_currentIndexChanged(const QString &arg1);
    void on_spinBoxStartNumber_valueChanged(const QString);

    void on_combinedIncludeJpgChk_clicked();
    void on_ejectChk_stateChanged(int arg1);
    void on_backupChk_stateChanged(int arg1);

    void on_cancelBtn_clicked();
    void on_okBtn_clicked();
    void on_helpBtn_clicked();

    void on_autoIngestTab_currentChanged(int index);

    void on_descriptionLineEdit_2_textChanged(const QString &arg1);

signals:
    void updateIngestParameters(QString rootFolderPath, QString manualFolderPath, bool isAuto);
    void updateIngestHistory(QString folderPath);

private:
    Ui::IngestDlg *ui;
    void initTokenList();
    void initExampleMap();
    bool isToken(QString tokenString, int pos);
    QString parseTokenString(QFileInfo info, QString tokenString);
    void accept();
    void buildFileNameSequence();
    void updateExistingSequence();
    int getSequenceStart(const QString &path);
    void updateEnabledState();
    void renameIfExists(QString &destination, QString &baseName, QString dotSuffix);
    void getPicks();
    bool autoParametersOk();

    bool isInitializing;
    bool isAuto;
    bool isBackup;

    QFileSystemModel fileSystemModel;
    Metadata *metadata;
    DataModel *dm;
    bool &combineRawJpg;
    bool &autoEjectUsb;
    QFileInfoList pickList;

    QStringList tokens;
    QMap<QString, QString> exampleMap;
    QMap<QString, QString> &pathTemplatesMap;
    QMap<QString, QString> &filenameTemplatesMap;

    // auto create path to ingest
    int &pathTemplateSelected;
    int &pathTemplateSelected2;
    QString &rootFolderPath;
    QString &rootFolderPath2;

    QString folderPath; // rootFolderPath + fromRootToBaseFolder + baseFolderDescription + "/"
    QString folderPath2; // rootFolderPath + fromRootToBaseFolder + baseFolderDescription + "/"
    QString fromRootToBaseFolder;
    QString fromRootToBaseFolder2;
    QString baseFolderDescription;
    QString baseFolderDescription2;
    QStringList& ingestDescriptionCompleter;

    // manually create path to ingest
    QString &manualFolderPath;


    // file name creation
    int& filenameTemplateSelected;
    QString fileSuffix;

    int seqWidth;
    int seqStart;
    int seqNum;

    QDateTime createdDate;

    int fileCount;
    float fileMB;

    QString currentToken;
    int tokenStart, tokenEnd;
};

#endif // INGESTDLG_H
