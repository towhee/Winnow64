#ifndef IMAGEDECODER_H
#define IMAGEDECODER_H

#include <QtWidgets>
#include <QMutex>
#include <QThread>
#include <QWaitCondition>
#include <atomic>
#include <functional>
#include "Cache/cachedata.h"
#include "Metadata/metadata.h"
#include "Metadata/imagemetadata.h"
#include "Datamodel/datamodel.h"
#include "Utilities/icc.h"
#include "Develop/editparams.h"
#ifdef Q_OS_WIN
#include "ImageFormats/Heic/heic.h"
#endif

struct WorkingImage;

class ImageDecoder : public QObject
{
    Q_OBJECT
public:
    ImageDecoder(int id, DataModel *dm, Metadata *metadata);
    ~ImageDecoder() override;
    /* progress (when set) is forwarded to the RAW demosaic (in-house/Winnow engine) for
       per-tile status-bar feedback -- used by MW::ensureDevelopWork to show demosaic
       progress on develop-open. Inert for non-raw formats and the Apple engine. */
    bool decodeIndependent(QImage &img, Metadata *metadata, ImageMetadata &m,
                           const std::function<void(int, int)> &progress = {});

    /* Decode the raw sensor file straight to a pre-develop WorkingImage, uncached (does NOT touch
       WorkingImageCache), for the "Denoise raw" base. m must carry fPath + rawInfo + ISONum.
       denoiseRaw runs the PMRID pre-demosaic denoiser (in-house/Winnow engine only). Returns
       nullptr if the format has no in-house decoder or the decode fails. Thread-safe: consults only
       the supplied m (no DataModel access), so callable from the develop render pool. progress (when
       set) is forwarded to the PMRID denoiser for per-tile status-bar feedback. outClean (when
       non-null AND denoiseRaw) also receives the CLEAN pre-develop base (pre-PMRID) from the same
       decode, so one UnpackCfa yields both bases MW::ensureRawDenoise needs. */
    std::shared_ptr<const WorkingImage> decodeRawWorking(const ImageMetadata &m, bool denoiseRaw,
                                                         const std::function<void(int, int)> &progress = {},
                                                         std::shared_ptr<const WorkingImage> *outClean = nullptr);

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
    /* Progress sink for the RAW demosaic, set by decodeIndependent and forwarded to
       RawFormat::Decode by load(). Empty for all other decode paths. */
    std::function<void(int, int)> decodeProgress;

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
    /* Per-tile progress of the in-house RAW demosaic (cache-mode decode). ImageCache
       relays it to MW, which shows a "Demosaic" status-bar row for the current image
       while a Winnow raw decodes with Auto-run denoise off (MW::onDemosaicProgress). */
    void demosaicProgress(int sfRow, QString fPath, int done, int total);

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
