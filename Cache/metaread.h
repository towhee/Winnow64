#ifndef METAREAD_H
#define METAREAD_H

#include <QtWidgets>
#include <QMutex>
#include <QThread>
#include <QWaitCondition>
#include "Main/global.h"
#include "Datamodel/datamodel.h"
#include "Metadata/metadata.h"
//#include "Metadata/imagemetadata.h"
#include "Cache/imagecache2.h"
#include "Image/thumb.h"

class MetaRead : public QThread
{
    Q_OBJECT

public:
    MetaRead(QObject *parent, DataModel *dm, ImageCache2 *imageCacheThread2);
    ~MetaRead() override;
    void stop();
    void read(int row = 0);
    void initialize();

protected:
    void run() Q_DECL_OVERRIDE;

signals:
    void completed();
    void add(ImageMetadata m);

private:
    QMutex mutex;
    QWaitCondition condition;
    bool abort;
    DataModel *dm;
    Metadata *metadata;
    ImageCache2 *imageCacheThread2;
    Thumb *thumb;
};

#endif // METAREAD_H
