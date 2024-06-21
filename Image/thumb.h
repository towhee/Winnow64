#ifndef THUMB_H
#define THUMB_H

#include <QObject>
#include <QtWidgets>
#include "Main/global.h"
#include "Metadata/metadata.h"
#include "Metadata/imagemetadata.h"
#include "Datamodel/datamodel.h"
#include "Cache/framedecoder.h"
#include "ImageFormats/Tiff/tiff.h"

class Thumb : public QObject
{
    Q_OBJECT
    QThread frameDecoderthread;
public:
    explicit Thumb(DataModel *dm, Metadata *metadata,
                   FrameDecoder *frameDecoder);
    bool loadThumb(QString &fPath, QImage &image, int instance, QString src);
    void insertThumbnails(QList<int> &missingJpg);
    void insertThumbnailsInJpg(QModelIndexList &selection);
    bool insertingThumbnails = false;
    bool abort = false;

    enum Status {
        None,
        Success,
        Fail,
        Open
    };

signals:
    void videoFrameDecode(QString fPath, int longSide, QString source,
                          QModelIndex dmIdx, int dmInstance);
    void setValue(QModelIndex dmIdx, QVariant value, int instance, QString src,
                  int role = Qt::EditRole, int align = Qt::AlignCenter);
    void getFrame(QString fPath);

private:
    DataModel *dm;
    Metadata *metadata;
    FrameDecoder *frameDecoder;
    int dmRow;
    QString err;
    QSize thumbMax;
    int instance;
    QFileDevice::Permissions oldPermissions;

    void setImageDimensions(QString &fPath, QImage &image, int row);
    Status loadFromJpgData(QString &fPath, QImage &image);
    Status loadFromTiff(QString &fPath, QImage &image, int row);
    Status loadFromHeic(QString &fPath, QImage &image);
    Status loadFromEntireFile(QString &fPath, QImage &image, int row);
    void loadFromVideo(QString &fPath, int dmRow);
    void checkOrientation(QString &fPath, QImage &image);

    // status flags
    bool isThumbOffset;
    bool isThumbLength;
    bool isDimensions;
    bool isAspectRatio;
    bool isEmbeddedThumb;

    uint offsetThumb = 0;
    uint lengthThumb = 0;

    bool isDebug;
};

#endif // THUMB_H
