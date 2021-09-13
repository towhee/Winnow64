#ifndef IMAGEDECODER_H
#define IMAGEDECODER_H

//#include <QObject>
#include <QtWidgets>
#include <QMutex>
#include <QThread>
#include <QWaitCondition>
#include "Main/global.h"
#include "Cache/cachedata.h"
#include "Metadata/metadata.h"
#include "Datamodel/datamodel.h"
#include "Metadata/imagemetadata.h"
#include "ImageFormats/Jpeg/jpeg.h"
#include "Utilities/icc.h"
#ifdef Q_OS_WIN
#include "ImageFormats/Heic/heic.h"
#endif

class ImageDecoder : public QThread
{
    Q_OBJECT

public:
    ImageDecoder(QObject *parent,
                 int id,
                 DataModel *dm,
                 Metadata *metadata);
    void decode(ImageCacheData::CacheItem item);
//    void decode(QString fPath);
    void setReady();
    void stop();

    int threadId;
    QImage image;
    QString fPath;

    enum Status {
        Ready,
        Busy,
        Failed,
        Done
    } status;

protected:
    void run() Q_DECL_OVERRIDE;

signals:
    void done(int threadId, bool positionChange = false);

private:
    QMutex mutex;
    QWaitCondition condition;
    bool load();
    void decodeUsingQt();
    void rotate();
    void colorManage();
    bool abort = false;
    DataModel *dm;
    Metadata *metadata;
    ImageCacheData::CacheItem n;
    unsigned char *buf;
    QString ext;
    QString errImage = ":/images/badImage1.png";
};

#endif // IMAGEDECODER_H
