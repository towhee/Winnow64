#ifndef INGEST_H
#define INGEST_H

#include <QThread>
#include <QMutex>
#include <QWaitCondition>
#include "Datamodel/datamodel.h"
//#include "Metadata/metadata.h"
#include "Main/global.h"
#include "Dialogs/ingesterrors.h"

class Ingest : public QThread
{
    Q_OBJECT

public:
    Ingest(QWidget *parent,
           bool &combineRawJpg,
           bool &combinedIncludeJpg,
           bool &integrityCheck,
           bool &ingestIncludeXmpSidecar,
           bool &isBackup,
           int &seqStart,
           Metadata *metadata,
           DataModel *dm,
           QString &folderPath,
           QString &folderPath2,
           QMap<QString, QString>&filenameTemplates,
           int &filenameTemplateSelected);
    void commence();

protected:
    void run() Q_DECL_OVERRIDE;

signals:
    void updateProgress(int progress);
    void ingestFinished();

private:
    QMutex mutex;
    QWaitCondition condition;
    bool abort;
    Metadata *metadata;
    DataModel *dm;
    const bool &combineRawJpg;
    const bool &combinedIncludeJpg;
    const bool &integrityCheck;
    const bool &ingestIncludeXmpSidecar;
    const bool &isBackup;
    int &seqStart;
    QString folderPath;
    QString folderPath2;
    QFileInfoList pickList;
    QMap<QString, QString> &filenameTemplatesMap;
    int &filenameTemplateSelected;
    QMap<QString, QString> exampleMap;
    QString currentToken;
    int tokenStart;
    int tokenEnd;
    int seqWidth;
    int seqNum;


    void getPicks();
    QString parseTokenString(QFileInfo info, QString tokenString);
    void renameIfExists(QString &destination, QString &baseName, QString dotSuffix);
    bool isToken(QString tokenString, int pos);
    void initExampleMap();
};

#endif // INGEST_H
