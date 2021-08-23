#ifndef CACHEIMAGE_H
#define CACHEIMAGE_H

#include <QObject>
#include <QtWidgets>
#include <QHash>
#include "Metadata/metadata.h"
#include "Datamodel/datamodel.h"
#include "Metadata/imagemetadata.h"
#include "Cache/imagedecoder.h"
//#ifdef Q_OS_WIN
#include "Utilities/icc.h"
//#endif
#include "ImageFormats/Jpeg/jpeg.h"
#ifdef Q_OS_WIN
// rgh remove heic
#include "ImageFormats/Heic/heic.h"
#endif

class CacheImage : public QObject
{
    Q_OBJECT
public:
    explicit CacheImage(QObject *parent,
                        DataModel *dm,
                        Metadata *metadata);
    bool load(QString &fPath, ImageDecoder *decoder);

private:
    DataModel *dm;
    Metadata *metadata;
    QMutex mutex;

    bool loadFromHeic(QString &fPath, QImage &image);
};

#endif // CACHEIMAGE_H
