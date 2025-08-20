#ifndef THUMB_H
#define THUMB_H

#include <QObject>
#include <QtWidgets>
#include "Metadata/metadata.h"
#include "Metadata/imagemetadata.h"
#include "Datamodel/datamodel.h"
#include "Cache/framedecoder.h"
#include "Cache/tiffthumbdecoder.h"
#include "ImageFormats/Tiff/tiff.h"

class Thumb : public QObject
{
    Q_OBJECT
public:
    explicit Thumb(DataModel *dm);
    ~Thumb() override;
    void abortProcessing();
    bool loadThumb(QString &fPath, QModelIndex dmIdx, QImage &image,
                   int instance, QString src);
    void presetOffset(uint offset, uint length);
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
                          int dmRow, int dmInstance);
    void setValueDm(QModelIndex dmIdx, QVariant value, int instance, QString src,
                  int role = Qt::EditRole, int align = Qt::AlignCenter);
    void setValueSf(QModelIndex sfIdx, QVariant value, int instance, QString src,
                    int role = Qt::EditRole, int align = Qt::AlignLeft); // not used
    void getFrame(QString fPath);

private:
    DataModel *dm;
    Metadata *metadata;
    FrameDecoder *frameDecoder;
    QThread *frameDecoderthread;
    // TiffThumbDecoder *tiffThumbDecoder;
    // QThread *tiffThumbDecoderThread;
    int dmRow;
    QString err;
    QSize thumbMax;
    int instance;
    QFileDevice::Permissions oldPermissions;

    void setImageDimensions(QString &fPath, QImage &image, int row);
    Status loadFromJpgData(QString &fPath, QImage &image);
    Status loadFromTiff(QString &fPath, QImage &image, int row, ImageMetadata &m);
    Status loadFromHeic(QString &fPath, QImage &image);
    Status loadFromEntireFile(QString &fPath, QImage &image, int row);
    void loadFromVideo(QString &fPath, int dmRow);
    void checkOrientation(QString &fPath, QImage &image);

    // status flags
    bool isPresetOffset = false;
    bool isThumbOffset;
    bool isThumbLength;
    bool isDimensions;
    bool isAspectRatio;
    bool isEmbeddedThumb;

    uint offsetThumb = 0;
    uint lengthThumb = 0;

    bool isDebug;
    int col0Width = 40;
};

#endif // THUMB_H
