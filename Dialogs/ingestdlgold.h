#ifndef INGESTDLGOLD_H
#define INGESTDLGOLD_H

#include <QtWidgets>
#include "Views/iconview.h"
#include "Datamodel/datamodel.h"
#include "tokendlg.h"

//#include "ui_ingestautopath.h"
//#include "ui_helpingest.h"
#include "Utilities/utilities.h"
#include "Dialogs/ingesterrors.h"
#include "Dialogs/editlistdlg.h"

namespace Ui {
class IngestDlg;
}

class IngestDlgOld : public QDialog
{
    Q_OBJECT

public:
    explicit IngestDlgOld(QWidget *parent,
                       bool &combineRawJpg,
                       bool &autoEjectUsb,
                       bool &integrityCheck,
                       bool &ingestIncludeXmpSidecar,
                       bool &isBackup,
                       bool &gotoIngestFolder,
                       Metadata *metadata,
                       DataModel *dm,
                       QString &ingestRootFolder,
                       QString &ingestRootFolder2,
                       QString &manualFolderPath,
                       QString &manualFolderPath2,
                       QString &baseFolderDescription,
                       QMap<QString, QString>&pathTemplates,
                       QMap<QString, QString>&filenameTemplates,
                       int &pathTemplateSelected,
                       int &pathTemplateSelected2,
                       int &filenameTemplateSelected,
                       QStringList &ingestDescriptionCompleter,
                       bool &isAuto,
                       QString css);
    ~IngestDlgOld() override;
    void test();

//private:
    Ui::IngestDlg *ui;
    void initTokenList();
    void initExampleMap();
    bool isToken(QString tokenString, int pos);
    QString parseTokenString(QFileInfo info, QString tokenString);
//    static void backgroundIngest(IngestDlg *d);
    void ingest();
    void buildFileNameSequence();
    void updateExistingSequence();
    int getSequenceStart(const QString &path);
    void getAvailableStorageMB();
    void quitIfNotEnoughSpace();
    void updateEnabledState();
    void renameIfExists(QString &destination, QString &baseName, QString dotSuffix);
    void getPicks();
    bool parametersOk();
    void fontSize();

    bool isInitializing;
    QString css;
    QString tabWidgetBackgroundColorName;

    QFileSystemModel fileSystemModel;
    Metadata *metadata;
    DataModel *dm;
    bool &isAuto;
    bool &combineRawJpg;
    bool &autoEjectUsb;
    bool &integrityCheck;
    bool &ingestIncludeXmpSidecar;
    bool &isBackup;
    bool &gotoIngestFolder;
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

    QString drivePath;      // path name to first /
    QString drive2Path;     // path name to first /
    QString drive;          // pretty drive name
    QString drive2;         // pretty drive name
    QString folderPath; // rootFolderPath + fromRootToBaseFolder + baseFolderDescription + "/"
    QString folderPath2; // rootFolderPath + fromRootToBaseFolder + baseFolderDescription + "/"
    QString fromRootToBaseFolder;
    QString fromRootToBaseFolder2;
    QString &baseFolderDescription;             // auto reuse if same source folder as previous
    QString baseFolderDescription2;
    QStringList& ingestDescriptionCompleter;

    QStringListModel *ingestDescriptionListModel;

    // manually create path to ingest
    QString &manualFolderPath;
    QString &manualFolderPath2;

    bool autoDriveAvailable;
    bool autoDrive2Available;
    bool manualDriveAvailable;
    bool manualDrive2Available;
    bool invalidDrive;

    // memory required for picks
    double picksMB;

    // available MB on destination drive
    double availableMB;
    double availableMB2;

    // text styles
    QString normalText;
    QString redText;

    // list of files not copied
    QStringList failedToCopy;
    QStringList integrityFailure;

    // file name creation
    int& filenameTemplateSelected;
    QString fileSuffix;

    // checkbox in Backup location tab
    QCheckBox *isBackupChkBox;

    int seqWidth;
    int seqStart;
    int seqNum;

    QDateTime createdDate;

    int fileCount;
    float fileMB;

    QString currentToken;
    int tokenStart, tokenEnd;

private slots:
    void updateFolderPaths();

    void on_autoRadio_toggled(bool checked);

    void on_autoIngestTab_currentChanged(int);
    void on_selectFolderBtn_clicked();
    void on_selectFolderBtn_2_clicked();
    void on_selectRootFolderBtn_2_clicked();
    void on_selectRootFolderBtn_clicked();
    void on_pathTemplatesCB_currentIndexChanged(const QString &arg1);
    void on_pathTemplatesCB_2_currentIndexChanged(const QString &arg1);
    void on_pathTemplatesBtn_clicked();
    void on_pathTemplatesBtn_2_clicked();
    void on_descriptionLineEdit_textChanged(const QString &arg1);
    void on_descriptionLineEdit_2_textChanged(const QString);
    void on_editDescriptionListBtn_clicked();
    void on_editDescriptionListBtn_2_clicked();

    void on_filenameTemplatesBtn_clicked();
    void on_filenameTemplatesCB_currentIndexChanged(const QString &arg1);
    void on_spinBoxStartNumber_valueChanged(const QString);

    void on_combinedIncludeJpgChk_clicked();
    void on_ejectChk_stateChanged(int);
    void on_integrityChk_stateChanged(int);
    void on_includeXmpChk_stateChanged(int);
    void on_backupChk_stateChanged(int arg1);
    void on_isBackupChkBox_stateChanged(int arg1);

    void on_cancelBtn_clicked();
    void on_okBtn_clicked();
    void on_helpBtn_clicked();

    void on_openIngestFolderChk_stateChanged(int arg1);

protected:
    void resizeEvent(QResizeEvent *event) override;
    void moveEvent(QMoveEvent *event) override;
    void showEvent(QShowEvent *event) override;

signals:
    void updateIngestHistory(QString folderPath);
    void revealIngestLocation(QString fPath);
    void updateProgress(int value);

};

#endif // INGESTDLGOLD_H
