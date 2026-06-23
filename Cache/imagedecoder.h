#ifndef IMAGEDECODER_H
#define IMAGEDECODER_H

#include <QtWidgets>
#include <QMutex>
#include <QThread>
#include <QWaitCondition>
#include <atomic>
#include "Cache/cachedata.h"
#include "Metadata/metadata.h"
#include "Metadata/imagemetadata.h"
#include "Datamodel/datamodel.h"
#include "Utilities/icc.h"
#include "Develop/editparams.h"
#ifdef Q_OS_WIN
#include "ImageFormats/Heic/heic.h"
#endif

class ImageDecoder : public QObject
{
    Q_OBJECT
public:
    ImageDecoder(int id, DataModel *dm, Metadata *metadata);
    ~ImageDecoder() override;
    bool decodeIndependent(QImage &img, Metadata *metadata, ImageMetadata &m);
    bool isRunning() const;
    void setIdle();
    void setBusy();
    bool isIdle();
    bool isBusy();

    int threadId;
    int sfRow;
    std::atomic<int> instance{0};
    QImage image;
    QString fPath;
    QString errMsg;
    qint64 nsToDecode;

    enum Status {
        Undefined,
        Success,
        Abort,
        Invalid,
        Failed,
        Video,
        InstanceClash,
        NoDir,
        NoFile,
        BlankFilePath,
        NoMetadata,
        FileOpen
    } status;

    QStringList statusText {
        "Undefined",
        "Success",
        "Abort",
        "Invalid",
        "Failed",
        "Video",
        "InstanceClash",
        "NoDir",
        "NoFile",
        "BlankFilePath",
        "NoMetadata",
        "FileOpen"
    };

    enum decoders {
        QtImage,        // use QImage::load or QImage::loadFromData
        QtTiff,         // use QTTIFFHANDLER code with nonQt libtiff
        LibTiff,        // use libtiff and convert to QImage
        TurboJpg,       // use turbojpg to decode jpegs
        MacOS,          // use MacOS to decode heic
        LibHeif,        // use libheif to decode heic
        Rory,           // use my decoder
        Raw             // full-sensor demosaic via the RawFormat pipeline
    } decoderToUse;

    QStringList decodersText {
        "QtImage",
        "QtTiff",
        "LibTiff",
        "TurboJpg",
        "MacOS",
        "LibHeif",
        "Rory",
        "Raw"
    };

public slots:
    void decode(int sfRow, int instance);
    void abortProcessing();
    void stop();

signals:
    void setValSf(int sfRow, int sfCol, QVariant value, int instance, QString src = "MW",
                  int role = Qt::EditRole, int align = Qt::AlignLeft);
    // doneStatus / doneSfRow / doneImage / doneFPath snapshot the decoder state at emit time.
    // Qt copies these into the queued event, so the consumer on imageCacheThread sees a
    // stable view even if the decoder has already started the next decode() on its thread.
    void done(int threadId, int doneStatus, int doneSfRow,
              QImage doneImage, QString doneFPath, qint64 doneMsToDecode);

private:
    QThread decoderThread;
    QAtomicInt running {0}; // 0 = not running, 1 = running

    mutable QMutex mutex;
    QWaitCondition idleCondition;
    void setIdle(bool v);

    bool load();
    bool quit();
    void decodeUsingQt();
    void rotate();
    void applyDevelop();
    void colorManage();
    bool idle = true;
    QAtomicInt abort {0};
    DataModel *dm;
    Metadata *metadata;

    /*
    Independent mode is set by decodeIndependent(). When true, load(), rotate()
    and colorManage() read per-file values (ext, offsets, orientation, icc)
    from indMeta instead of the live DataModel (dm->sf), so a decode can run
    even after the DataModel has moved to a different folder. The normal
    caching path (decode()) leaves this false and is unaffected.
    */
    bool isIndependent = false;
    ImageMetadata indMeta;

    /*
    Per-image develop adjustments. Identity (no edits) by default, so applyDevelop() is a
    no-op until parametric edits are stored per image (datamodel/sidecar, deferred) and set
    here. developApplied guards against a second pass: the RAW path develops internally (in
    linear float, inside RawFormat::Decode), so applyDevelop() must skip those.
    */
    EditParams editParams;
    bool developApplied = false;
    // ImageCacheData::CacheItem n;
    unsigned char *buf;
    QString ext;
    QString errImage = ":/images/badImage1.png";
    bool isDebug = false;
    bool isLog   = false;
};

#endif // IMAGEDECODER_H
