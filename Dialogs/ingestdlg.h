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
                       Metadata *metadata,
                       DataModel *dm,
                       QString ingestRootFolder,
                       QMap<QString, QString>& pathTemplates,
                       QMap<QString, QString>& filenameTemplates,
                       int& pathTemplateSelected,
                       int& filenameTemplateSelected,
                       QStringList &ingestDescriptionCompleter,
                       bool isAuto);
    ~IngestDlg();

private slots:
    void updateFolderPath();
    void on_selectFolderBtn_clicked();
    void on_selectRootFolderBtn_clicked();
    void on_spinBoxStartNumber_valueChanged(const QString);
    void on_descriptionLineEdit_textChanged(const QString &);
    void on_autoRadio_toggled(bool checked);
    void on_pathTemplatesBtn_clicked();
    void on_pathTemplatesCB_currentIndexChanged(const QString &arg1);
    void on_filenameTemplatesBtn_clicked();
    void on_filenameTemplatesCB_currentIndexChanged(const QString &arg1);
    void on_cancelBtn_clicked();
    void on_okBtn_clicked();
    void on_helpBtn_clicked();

    void on_combinedIncludeJpgChk_clicked();

    void on_manualRadio_toggled(bool checked);

    void on_ejectChk_stateChanged(int arg1);

signals:
    void updateIngestParameters(QString rootFolderPath, bool isAuto);

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
    void updateStyleOfFolderLabels();
    void renameIfExists(QString &destination, QString &fileName, QString &dotSuffix);
    void getPicks();

    bool isInitializing;
    bool isAuto;

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

    int& pathTemplateSelected;
    int& filenameTemplateSelected;
    QStringList& ingestDescriptionCompleter;

    QString folderPath; // rootFolderPath + fromRootToBaseFolder + baseFolderDescription + "/"
    QString defaultRootFolderPath;

    QString rootFolderPath;
    QString fromRootToBaseFolder;
    QString baseFolderDescription;
    QString folderBase;

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
