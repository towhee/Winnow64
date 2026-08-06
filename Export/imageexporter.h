#ifndef IMAGEEXPORTER_H
#define IMAGEEXPORTER_H

#include <QImage>
#include <QObject>
#include <QString>
#include <QStringList>
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
        Embellish  -- (later) the embellish scene render

    ASYNCHRONOUS BY CONTRACT. A single full-resolution raw render is ~1s, so a synchronous
    batch would freeze the GUI for minutes. The PixelSource is therefore handed a
    completion callback rather than returning an image: the provider decides where its
    work runs, and the exporter drives one image at a time, yielding to the event loop
    between them. That keeps progress painting and abort responsive throughout, and lets
    the Develop provider interleave its GUI-thread steps (datamodel reads, mask
    prerequisites) with its worker-thread steps (decode, composite).

    Threading: `done` may be invoked from any thread -- the exporter marshals itself back
    to its own thread before touching any state. Everything else here runs on the thread
    the exporter lives on.
*/
class ImageExporter : public QObject
{
    Q_OBJECT

public:
    /* Deliver the finished, full-size image for fPath by invoking `done` exactly once.
       ok == false skips the file (it is recorded as failed and the batch continues). */
    using Done = std::function<void(bool ok, const QImage &img)>;
    using PixelSource = std::function<void(const QString &fPath, Done done)>;

    struct Result {
        QStringList written;    // destination paths successfully saved
        QStringList skipped;    // existed already, with Exists::Skip
        QStringList failed;     // render or save failed
        bool        aborted = false;
        /* The folders written to, for the caller's folder-view refresh. */
        QStringList folders;
    };

    ImageExporter(DataModel *dm, Metadata *metadata, QObject *parent = nullptr);
    ~ImageExporter() override;

    void setPixelSource(PixelSource source);

    /* Export every path in srcPaths. Returns immediately: the batch runs through the
       event loop and reports via progress() / finished(). Calling run() while a batch is
       already active is ignored. */
    void run(const QStringList &srcPaths, const ExportSettings &settings);

    bool isRunning() const { return running; }

    /* The settings the ACTIVE batch is running under. A PixelSource needs them (the
       develop source renders 8- or 16-bit to match the chosen format) and so does the
       finished() handler, and reading them here is what keeps callers from having to
       shadow a copy that can drift out of step with the run. */
    const ExportSettings &activeSettings() const { return s; }

    /* The export layer's published OutputSpace -> the render layer's colour space. The
       two enums are deliberately separate (Develop must not depend on Export), so this
       is the one place they are tied together -- a PixelSource that renders through
       OutputTransform asks here, and save() tags with the matching QColorSpace. */
    static OutputTransform::Space renderSpace(OutputSpace space);

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

public slots:
    /* Stop after the image currently in flight. Safe to call when not running. */
    void abort();

signals:
    void progress(int done, int total);
    void finished(const ImageExporter::Result &result);

private:
    void startNext();                        // request the next image from the source
    void onImageReady(bool ok, const QImage &img);
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
    /* Apply the sizing rules. Returns src unchanged when no resize applies. */
    QImage  resized(const QImage &src) const;
    /* Write img to dstPath at the configured format / depth / quality. */
    bool    save(const QImage &img, const QString &dstPath) const;
    /* ExifTool: copy tags + ICC from src to dst, and optionally embed a thumbnail. */
    void    copySourceMetadata(const QString &srcPath, const QString &dstPath,
                               const QImage &img);

    DataModel *dm;
    Metadata  *metadata;

    PixelSource pixelSource;
    ExportSettings s;
    QStringList paths;
    int         index = 0;          // the image currently being exported
    bool        running = false;
    bool        aborting = false;
    Result      result;
};

#endif // IMAGEEXPORTER_H
