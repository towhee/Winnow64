#ifndef RENAMEFILE_H
#define RENAMEFILE_H

#include <QObject>
#include <QtWidgets>
#include "Metadata/metadata.h"
#include "Datamodel/datamodel.h"
#include "Cache/imagecache.h"
#include "tokendlg.h"

namespace Ui {
    class RenameFiles;
}

class RenameFileDlg : public QDialog
{
    Q_OBJECT
public:
    explicit RenameFileDlg(QWidget *parent,
                           QString &folderPath,
                           QStringList &selection,
                           QMap<QString, QString>& filenameTemplates,
                           DataModel *dm,
                           Metadata *metadata,
                           ImageCache *imageCache
                           );

private:
    Ui::RenameFiles *ui;
    void renameFileBase(QString oldBase, QString newBase);
    void makeExistingBaseUnique(QString newBase);
    void renameDatamodel(QString oldPath, QString newPath, QString newName);
    void renameAllSharingBaseName(QString oldBase, QString newBase);
    void appendAllSharingBaseName(QString path);
    void resolveNameConflicts();
    void rename();
    void initTokenList();
    void initExampleMap();
    void updateExistingSequence();
    int getSequenceStart(const QString &path);
    bool isToken(QString tokenString, int pos);
    QString parseTokenString(QFileInfo info, QString tokenString);
    void updateExample();
    void diagFiles();
    void diagFilesToRename();
    void diagBaseNames();
    void diagDatamodel();
    void diagBaseNamesUsed();

    DataModel *dm;
    Metadata *metadata;
    ImageCache *imageCache;
    QString &folderPath;
    QList<QString> baseNames;
    QList<QString> baseNamesUsed;
    QList<QList<QString>> filesToRename;    // fPath,originalBasename,done
//    QList<QString> filesToRename;
    QStringList &selection;
    QModelIndexList selectionIndexes;
    QMap<QString, QString> &filenameTemplatesMap;
    QMap<QString, QString> exampleMap;
    QStringList tokens;
    QDateTime createdDate;
    QString currentToken;
    int tokenStart;
    int tokenEnd;
    int seqWidth;
    int seqNum;
    QDate seqDate;

    bool isDebug;

private slots:
    void on_okBtn_clicked();
    void on_filenameTemplatesBtn_clicked();
    void on_filenameTemplatesCB_currentTextChanged(const QString &arg1);
    void on_spinBoxStartNumber_textChanged(const QString /* &arg1 */);
signals:

};

#endif // RENAMEFILE_H
