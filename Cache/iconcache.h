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
#include "Cache/cachedata.h"

class IconCache : public QObject
{
    Q_OBJECT
public:
    explicit IconCache(QObject *parent,
                       DataModel *dm,
                       Metadata *metadata,
                       IconCacheData *iconCacheData);
    ~IconCache() override;

    bool abort;
    bool isRunning = false;
    bool isRestart;
    int iconChunkSize;
    int firstIconRow;
    int lastIconRow;

signals:
    void addToDatamodel(ImageMetadata m);
    void addToIconCache(int dmRow);
    void removeFromIconCache(int dmRow);
    void setIcon(QModelIndex dmIdx, QPixmap &pm, int instance, QString src);

public slots:
    void stop();
    void start();
    void read(int sfRow = 0, QString src = "");

private:
    void cleanupIcons();
    void iconMax(QPixmap &thumb);
    QMutex mutex;
    DataModel *dm;
    Metadata *metadata;
    Thumb *thumb;
    IconCacheData *iconCacheData;
    int dmInstance;
    QPixmap nullPm;
    bool debugCaching;
};

#endif // ICONCACHE_H
