#ifndef IMAGEDECODER_H
#define IMAGEDECODER_H

#include <QtWidgets>
#include <QMutex>
#include <QThread>
#include <QWaitCondition>
#include "Cache/cachedata.h"
#include "Metadata/metadata.h"
#include "Metadata/imagemetadata.h"
#include "Datamodel/datamodel.h"
#include "Utilities/icc.h"
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
    int instance;
    QImage image;
    QString fPath;
    QString errMsg;

    enum Status {
        Undefined,
        Success,
        Abort,
        Invalid,
        Failed,
        Video,
        InstanceClash,
        NoDir,
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
        Rory            // use my decoder
    } decoderToUse;

    QStringList decodersText {
        "QtImage",
        "QtTiff",
        "LibTiff",
        "TurboJpg",
        "MacOS",
        "LibHeif",
        "Rory"
    };

public slots:
    void decode(int sfRow, int instance);
    void abortProcessing();
    void stop();

signals:
    void done(int threadId, bool positionChange = false);

private:
    QThread decoderThread;
    QAtomicInt running {0}; // 0 = not running, 1 = running
    QMutex mutex;
    QWaitCondition condition;
    bool load();
    bool quit();
    void decodeUsingQt();
    void rotate();
    void colorManage();
    bool idle = true;
    bool abort = false;
    DataModel *dm;
    Metadata *metadata;
    // ImageCacheData::CacheItem n;
    unsigned char *buf;
    QString ext;
    QString errImage = ":/images/badImage1.png";
    bool isDebug = false;
    bool isLog   = false;
};

#endif // IMAGEDECODER_H
