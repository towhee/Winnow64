#ifndef AUTONOMOUSIMAGE_H
#define AUTONOMOUSIMAGE_H

#include <QObject>
#include <QtWidgets>
#include "Main/global.h"
#include "Metadata/metadata.h"
#include "Metadata/imagemetadata.h"
#include "Datamodel/datamodel.h"
#include "Cache/framedecoder.h"
#include "ImageFormats/Tiff/tiff.h"

class AutonomousImage : public QObject
{
    Q_OBJECT
    QThread frameDecoderthread;
public:
    explicit AutonomousImage(Metadata *metadata, FrameDecoder *frameDecoder);
    bool image(QString &fPath, QImage &image, int longSide, QString source);
    //bool image(QString &fPath, QImage &image, int longSide, ImageMetadata m);

signals:
    void videoFrameDecode(QString fPath, int longSide, QString source,
                          int dmRow, int dmInstance);
    void setValue(QModelIndex dmIdx, QVariant value, int instance, QString src,
                  int role = Qt::EditRole, int align = Qt::AlignCenter);
    void getFrame(QString fPath);

private:
    DataModel *dm;
    Metadata *metadata;
    ImageMetadata *m;
    FrameDecoder *frameDecoder;
    QString err;
    QSize thumbMax;
    int instance;
    int longSide;
    QString source;
    QFileDevice::Permissions oldPermissions;

    bool loadFromJpgData(QString &fPath, QImage &image);
    bool loadFromTiff(QString &fPath, QImage &image);
    bool loadFromHeic(QString &fPath, QImage &image);
    bool loadFromEntireFile(QString &fPath, QImage &image);
    void loadFromVideo(QString &fPath);
    void checkOrientation(QString &fPath, QImage &image);

    // status flags
    bool isThumbOffset;
    bool isThumbLength;
    bool isDimensions;
    bool isEmbeddedJpg;

    uint offsetThumb = 0;
    uint lengthThumb = 0;
    double aspectRatio = 0;

    bool isDebug;
};

#endif // AUTONOMOUSIMAGE_H
