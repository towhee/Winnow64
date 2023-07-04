#ifndef RENAMEFILE_H
#define RENAMEFILE_H

#include <QObject>
#include <QtWidgets>
#include "main/global.h"
#include "Metadata/Metadata.h"
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
    void appendAllSharingBaseName(QString path);
    void rename();
    void initTokenList();
    void initExampleMap();
    void updateExistingSequence();
    int getSequenceStart(const QString &path);
    bool isToken(QString tokenString, int pos);
    QString parseTokenString(QFileInfo info, QString tokenString);
    void updateExample();
    void debugShowDM(QString title);

    DataModel *dm;
    Metadata *metadata;
    ImageCache *imageCache;
    QString &folderPath;
    QList<QString> baseNamesUsed;
    QList<QList<QString>> filesToRename;    // fPath,originalBasename
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
