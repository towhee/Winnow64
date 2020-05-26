#ifndef THUMB_H
#define THUMB_H

#include <QObject>
#include <QtWidgets>
#include "Metadata/metadata.h"
#include "Metadata/imagemetadata.h"
#include "Datamodel/datamodel.h"
#include "ImageFormats/Tiff/tiff.h"

class Thumb : public QObject
{
    Q_OBJECT
public:
    explicit Thumb(QObject *parent, DataModel *dm, Metadata *metadata);
    bool loadThumb(QString &fPath, QImage &image);

private:
    DataModel *dm;
    Metadata *metadata;
    QString err;
    QSize thumbMax;

    bool loadFromJpgData(QString &fPath, QImage &image);
    bool loadFromTiffData(QString &fPath, QImage &image);
    bool loadFromEntireFile(QString &fPath, QImage &image);
    void checkOrientation(QString &fPath, QImage &image);

    void track(QString fPath, QString msg);
};

#endif // THUMB_H
