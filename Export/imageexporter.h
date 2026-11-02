#ifndef IMAGEEXPORTER_H
#define IMAGEEXPORTER_H

#include <QElapsedTimer>
#include <QImage>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QThreadPool>
#include <functional>
#include "Export/exportsettings.h"
#include "Develop/outputtransform.h"

class DataModel;
class Metadata;

/*
    The shared image-export engine: it owns everything about writing images OUT of Winnow
    that is not the generation of the pixels themselves.

        destination folder resolution (and subfolder creation)
        file naming: token template, suffix, overwrite / unique / skip
        resizing to the requested long edge or percentage
        saving at the requested format, bit depth and quality/compression
        tagging the output colour space
        copying metadata + ICC + thumbnail from the source with ExifTool
        progress reporting and abort
        collecting what was written, skipped and failed

    WHERE THE PIXELS COME FROM is injected, because Winnow has three different answers and
    they should not each own a copy of the list above:

        Develop    -- developCompositeStack: the full recipe, masks, spots and geometry
        Plain      -- the browse decode, for File > Save Preview as
        Embellish  -- MW::embellishedPixelSource wraps either one in the template

    ASYNCHRONOUS BY CONTRACT. A single full-resolution raw render is ~1s, so a synchronous
    batch would freeze the GUI for minutes. The PixelSource is therefore handed a
    completion callback rather than returning an image: the provider decides where its
    work runs, and the exporter drives one image at a time, yielding to the event loop
    between them. That keeps progress painting and abort responsive throughout, and lets
    the Develop provider interleave its GUI-thread steps (datamodel reads, mask
    prerequisites) with its worker-thread steps (decode, composite).

    Threading: `done` may be invoked from any thread -- the exporter marshals itself back
    to its own thread before touching any state. The encode and the ExifTool metadata
    copy run on the exporter's own worker (savePool); everything else runs on the thread
    the exporter lives on.

    NO DIALOG WAITS ON IT. ExportDlg only collects settings and closes on Export; the run
    reports through stage() / progress() to MW's status-bar "Export" row, Esc stops it,
    and a second export started meanwhile queues behind the first.
*/
class ImageExporter : public QObject
{
    Q_OBJECT

public:
    /* Deliver the finished, full-size image for fPath by invoking `done` exactly once.
       ok == false skips the file (it is recorded as failed and the batch continues). */
    using Done = std::function<void(bool ok, const QImage &img)>;
    using PixelSource = std::function<void(const QString &fPath, Done done)>;

    /* What the image in flight is doing, for the status-bar row. Render is the pixel
       source; Save and Metadata run on the exporter's worker thread. */
    enum class Stage { Render = 0, Save = 1, Metadata = 2 };
    static constexpr int kStages = 3;

    struct Result {
        QStringList written;    // destination paths successfully saved
        QStringList skipped;    // existed already, with Exists::Skip
        QStringList failed;     // render or save failed
        bool        aborted = false;
        /* The folders written to, for the caller's folder-view refresh. */
        QStringList folders;
        /* The settings this batch ran under -- the finished() handler's copy, so it
           never has to read activeSettings() after the next queued batch has begun. */
        ExportSettings settings;
    };

    /* [ExportTime] probe: one console line per image with the time each stage took
       (render, and inside it decode / denoise / masks / composite; save; metadata) and
       one for the catalog update. Here rather than in Main/global.h so flipping it does
       not rebuild the whole app. */
    static inline bool reportTime = true;

    ImageExporter(DataModel *dm, Metadata *metadata, QObject *parent = nullptr);
    ~ImageExporter() override;

    /* Export every path in srcPaths, with pixels from source. Returns immediately: the
       batch runs through the event loop and reports via stage() / progress() /
       finished(). A run() while a batch is active is QUEUED behind it -- the export
       dialog closes as soon as Export is pressed, so a second export can be started
       before the first is done. */
    void run(const QStringList &srcPaths, const ExportSettings &settings,
             PixelSource source);

    bool isRunning() const { return running; }
    int  queuedBatches() const { return int(queue.size()); }

    /* The settings the ACTIVE batch is running under. A PixelSource needs them (the
       develop source renders 8- or 16-bit to match the chosen format). */
    const ExportSettings &activeSettings() const { return s; }

    /* The export layer's published OutputSpace -> the render layer's colour space. The
       two enums are deliberately separate (Develop must not depend on Export), so this
       is the one place they are tied together -- a PixelSource that renders through
       OutputTransform asks here, and save() tags with the matching QColorSpace. */
    static OutputTransform::Space renderSpace(OutputSpace space);

    /* The size settings would write an image whose rendered size is src. src unchanged
       when no resize applies. A PixelSource with a ready-made smaller picture (the
       devPreview) asks this to decide whether that picture is big enough. */
    static QSize OutputSize(const QSize &src, const ExportSettings &settings);

    /* The destination path settings would produce for fPath, with no side effects beyond
       resolving the name -- the dialog uses it for its live "Example:" line. seq feeds
       the {XX...} sequence tokens (1 for a representative example). */
    QString previewDestination(const QString &fPath, const ExportSettings &settings,
                               int seq = 1) const;

    /* True when this export would write into a folder the datamodel currently has
       loaded -- exactly the condition under which addToFolderView means anything (see
       MW::onExportFinished). The dialog asks so it can disable that checkbox rather than
       offer a setting with no effect. Creates nothing. */
    bool willTouchLoadedFolder(const QStringList &srcPaths,
                               const ExportSettings &settings) const;

    /* True when fPath (a row key: a version resolves to its source file) is a raw file
       -- the files ExportSettings::CameraPreview applies to. */
    bool isRaw(const QString &fPath) const;

public slots:
    /* Stop after the image currently in flight, and drop any queued batches. Safe to
       call when not running. */
    void abort();

signals:
    /* The image at index (0-based) of total has entered stage. */
    void stage(int index, int total, ImageExporter::Stage stage, const QString &fPath);
    void progress(int done, int total);
    void finished(const ImageExporter::Result &result);

private:
    struct Batch {
        QStringList    paths;
        ExportSettings settings;
        PixelSource    source;
    };
    /* The worker's report for one image. */
    struct Saved {
        bool    ok = false;
        QString dst;
        qint64  saveMs = 0;
        qint64  metaMs = 0;
    };

    void start(Batch batch);                 // make batch the active one
    void startNext();                        // request the next image from the source
    void onImageReady(bool ok, const QImage &img);
    void onImageSaved(const Saved &saved, const QString &folder);
    void advance();                          // count the image done, queue the next
    void finish();

    /* Destination folder for fPath under `settings`. With create == true the subfolder is
       made if it does not exist; the preview path passes false so a dialog keystroke
       never creates a directory. Empty on failure (unwritable / could not create). */
    QString destinationFolder(const QString &fPath, const ExportSettings &settings,
                              bool create) const;
    /* Base file name (no extension) for fPath: the token template if one is set, else the
       original base name, plus the suffix. seq numbers the {XX...} sequence tokens. */
    QString destinationBaseName(const QString &fPath, const ExportSettings &settings,
                                int seq) const;

    /* WORKER THREAD. Static and handed their settings so nothing they touch belongs to
       the exporter: resize, encode and ExifTool are the seconds after the render that
       used to freeze the GUI (a 50 MP q100 JPEG plus an ExifTool rewrite of it). */
    static QImage resized(const QImage &src, const ExportSettings &settings);
    static bool   save(const QImage &img, const QString &dstPath,
                       const ExportSettings &settings);
    static void   copySourceMetadata(const QString &srcPath, const QString &dstPath,
                                     const QImage &img, const ExportSettings &settings);

    DataModel *dm;
    Metadata  *metadata;

    PixelSource pixelSource;
    ExportSettings s;
    QStringList paths;
    QList<Batch> queue;              // batches waiting behind the active one
    int         index = 0;          // the image currently being exported
    bool        running = false;
    bool        aborting = false;
    Result      result;
    QElapsedTimer renderTimer;      // [ExportTime] this image's render
    /* One thread: images are saved in order, and a batch never has more than one image
       held in memory past its render. Waited on in the destructor. */
    QThreadPool savePool;
};

#endif // IMAGEEXPORTER_H
