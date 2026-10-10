#ifndef THUMB_H
#define THUMB_H

#include <QObject>
#include <QtWidgets>
#include "Metadata/metadata.h"
#include "Metadata/imagemetadata.h"
#include "Datamodel/datamodel.h"
#include "Cache/framedecoder.h"
#include "ImageFormats/Tiff/tiff.h"

class Thumb : public QObject
{
    Q_OBJECT
public:
    explicit Thumb(DataModel *dm, FrameDecoder *frameDecoder);
    ~Thumb() override;
    void abortProcessing();
    bool loadThumb(QString &fPath, int dmRow, QImage &image,
                   int instance, const ImageMetadata &m, QString src);
    void presetOffset(uint offset, uint length);
    /*  True when the last loadThumb answered with the DEVELOPED thumbnail from the
        sidecar (loadDevThumb) rather than the camera's picture. Reader::readIcon must
        never put such an image into ThumbCache, which holds camera thumbnails only. */
    bool lastWasDevThumb = false;
    /*  The icon for a VERSION row (key = path + "/#v" + id, Utilities/versionkey.h):
        the version's own developed preview when current, else the source file's
        original camera thumbnail. metadata is the caller's (reader-local) Metadata,
        used only to walk the source's header on a thumbnail-cache miss. */
    bool loadVersionThumb(const QString &key, int dmRow, QImage &image, int instance,
                          Metadata *metadata, QString src);
    void insertThumbnailsInJpg(QModelIndexList &selection);
    bool insertingThumbnails = false;

    enum Status {
        None,
        Success,
        Fail,
        Open
    };

signals:
    void videoFrameDecode(QString fPath, int longSide, QString source,
                          int dmRow, int dmInstance);
    void setValDm(int sfRow, int sfCol, QVariant value, int instance, QString src,
                    int role = Qt::EditRole); // not used
    void setValSf(int sfRow, int sfCol, QVariant value, int instance, QString src,
                  int role = Qt::EditRole); // not used
    void getFrame(QString fPath);

private:
    mutable QMutex mutex;
    QWaitCondition idleCondition;
    void setIdle();
    void setBusy();
    bool abort = false;
    bool idle = true;

    DataModel *dm;
    Metadata *metadata;
    FrameDecoder *frameDecoder;     // shared, owned by MetaRead
    int dmRow;
    QString err;
    QSize thumbMax;
    int instance;
    QFileDevice::Permissions oldPermissions;

    /* fullSize = the IMAGE's dimensions (invalid when only the embedded
       preview was decoded); previewSize = the preview's own. See the .cpp. */
    void setImageDimensions(QString &fPath, QSize fullSize, QSize previewSize, int row);
    /* Cached developed thumbnail from the XMP sidecar. Tried before every other source
       in loadThumb; see Cache/devpreviewcache.h for the system it belongs to. */
    bool loadDevThumb(QString &fPath, QImage &image);
    Status loadFromJpgData(QString &fPath, QImage &image);
    Status loadFromTiff(QString &fPath, QImage &image, int row, const ImageMetadata &m);
    Status loadFromHeic(QString &fPath, QImage &image);
    Status loadFromImageIO(QString &fPath, QImage &image);
    /* knownFull: the metadata read's dimensions, which win over anything
       QImageReader reports. See the .cpp. */
    Status loadFromEntireFile(QString &fPath, QImage &image, int row,
                              QSize knownFull = QSize());
    void loadFromVideo(QString &fPath, int dmRow);
    void checkOrientation(QImage &image, int orientation, int rotationDegrees);

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
