#ifndef ICONCACHE_H
#define ICONCACHE_H

#include <QObject>
#include <QMutex>
#include <QThread>
#include <QWaitCondition>
#include "Main/global.h"
#include "Datamodel/datamodel.h"
#include "Metadata/metadata.h"
#include "Image/thumb.h"

class IconCache : public QObject
{
    Q_OBJECT
public:
    explicit IconCache(QObject *parent, DataModel *dm, Metadata *metadata);
    ~IconCache() override;

    bool abort;
    bool isRunning = false;
    bool isRestart;
    int iconChunkSize;
    int firstIconRow;
    int lastIconRow;

signals:
    void addToDatamodel(ImageMetadata m);
    void setIcon(QModelIndex dmIdx, QPixmap &pm, int instance);

public slots:
    void stop();
    void start();
    void read(int sfRow = 0, QString src = "");

private:
    void iconMax(QPixmap &thumb);
    QMutex mutex;
    DataModel *dm;
    Metadata *metadata;
    Thumb *thumb;
    int dmInstance;
    bool debugCaching;
};

#endif // ICONCACHE_H
