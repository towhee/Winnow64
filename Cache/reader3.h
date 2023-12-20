#ifndef READER3_H
#define READER3_H

#include <QRunnable>
#include <QtWidgets>
#include <QMutex>
#include <QThread>
#include <QWaitCondition>
#include "Main/global.h"
#include "Metadata/metadata.h"
#include "Metadata/imagemetadata.h"
#include "Datamodel/datamodel.h"
#include "Image/thumb.h"
#include "Cache/imagecache.h"

class Reader3 : public QRunnable
{
    Q_OBJECT
public:
    Reader3(QObject *parent,
            const QString fPath,
            const int instance,
            const bool isReadIcon,
            DataModel *dm,
            ImageCache *imageCache);

protected:
    void run() Q_DECL_OVERRIDE;

signals:
    void addToDatamodel(ImageMetadata m, QString src);
    void setIcon(QModelIndex dmIdx, const QPixmap pm, int fromInstance, QString src);
    void addToImageCache(ImageMetadata m, int instance);
    void done(int threadId);

private:
    bool readMetadata();
    void readIcon();
    inline bool instanceOk();
    bool abort = false;

    QString fPath;
    int instance;
    bool isReadIcon = true;
    DataModel *dm;
    ImageCache *imageCache;
    FrameDecoder *frameDecoder;
    Thumb *thumb;
    QModelIndex dmIdx = QModelIndex();
    Metadata *metadata;
    QPixmap pm;
    bool pending = false;
    bool loadedIcon = false;
    int metaReadCount;
    bool isVideo;
    bool isDebug;
};

#endif // READER3_H
