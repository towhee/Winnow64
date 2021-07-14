#ifndef THUMB_H
#define THUMB_H

#include <QObject>
#include <QtWidgets>
#include "Main/global.h"
#include "Metadata/metadata.h"
#include "Metadata/imagemetadata.h"
#include "Datamodel/datamodel.h"
#include "ImageFormats/TIFF/tiff.h"

class Thumb : public QObject
{
    Q_OBJECT
public:
    explicit Thumb(QObject *parent, DataModel *dm, Metadata *metadata);
    bool loadThumb(QString &fPath, QImage &image, QString src);
    void insertThumbnails(QModelIndexList &selection);
    bool insertingThumbnails = false;
    bool abort = false;

private:
    DataModel *dm;
    Metadata *metadata;
    QString err;
    QSize thumbMax;

    bool loadFromJpgData(QString &fPath, QImage &image);
    bool loadFromTiffData(QString &fPath, QImage &image);
    bool loadFromEntireFile(QString &fPath, QImage &image, int row);
    void checkOrientation(QString &fPath, QImage &image);
};

#endif // THUMB_H
