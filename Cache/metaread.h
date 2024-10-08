#ifndef METAREAD_H
#define METAREAD_H

#include <QtWidgets>
#include <QObject>
#include <QMutex>
#include <QThread>
#include <QWaitCondition>
#include "Datamodel/datamodel.h"
#include "Metadata/metadata.h"
#include "Image/thumb.h"
#include "Utilities/utilities.h"

class MetaRead : public QThread
{
    Q_OBJECT

public:
    MetaRead(QObject *parent,
             DataModel *dm,
             Metadata *metadata,
             FrameDecoder *frameDecoder);
    ~MetaRead() override;

    bool stop();
    QString diagnostics();
    QString reportMetaCache();
    void cleanupIcons();
    void resetTrigger();

    int iconChunkSize;
    int firstIconRow;
    int lastIconRow;

    bool showProgressInStatusbar = true;

    bool abort;

signals:
    void stopped(QString src);
    void updateScroll();
    void runStatus(bool/*isRunning*/, bool/*showCacheLabel*/, QString/*calledBy*/);
    void centralMsg(QString message);
    void updateProgressInFilter(int progress);
    void updateProgressInStatusbar(int progress, int total);

    void addToDatamodel(ImageMetadata m, QString src);
    void setIcon(QModelIndex dmIdx, const QPixmap pm, int fromInstance, QString src);
    void addToImageCache(ImageMetadata m, int instance);

    void fileSelectionChange(QModelIndex sfIdx,
                             QModelIndex idx2 = QModelIndex(),
                             bool clearSelection = false,
                             QString src = "MetaRead::triggerCheck");
    void done();               // not being used - req'd?

public slots:
    void initialize();
    void setStartRow(int row, bool fileSelectionChanged, QString src = "");

protected:
    void run() Q_DECL_OVERRIDE;

private:
    void triggerCheck();
    void triggerFileSelectionChange();
    void read(int startRow);
    void readRow(int sfRow);
    bool readMetadata(QModelIndex sfIdx, QString fPath);
    void readIcon(QModelIndex sfIdx, QString fPath);

    QMutex mutex;
    QWaitCondition condition;
//    bool abort;
    bool abortCleanup;
    bool interrupted;
    int interruptedRow;

    DataModel *dm;
    Metadata *metadata;
    FrameDecoder *frameDecoder;
    Thumb *thumb;
    int instance;
    int sfRowCount;
    int dmRowCount;
    int metaReadCount;
    double expansionFactor = 1.2;
    int iconLimit;                          // iconChunkSize * expansionFactor
    int imageCacheTriggerCount;
    bool hasBeenTriggered;
    bool okToTrigger;                       // signal MW::fileSelectionChange

    bool fileSelectionChanged;              // triggers imageCache if true
    int startRow = 0;
    int targetRow = 0;
    int lastRow;
    int triggerCount;
    QString src;

    QList<int> rowsWithIcon;

    bool isDebug = false;
    int debugRow;
    QElapsedTimer t;
    QElapsedTimer tAbort;
    quint32 ms;
};

#endif // METAREAD_H
