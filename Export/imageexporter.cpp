#include "Export/imageexporter.h"
#include "Utilities/fileops.h"
#include "Utilities/versionkey.h"

#include <QColorSpace>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImageWriter>
#include <QPointer>
#include <QTimer>

#include "Datamodel/datamodel.h"
#include "Main/global.h"
#include "Metadata/ExifTool.h"
#include "Metadata/metadata.h"
#include "Utilities/tokenfilename.h"
#include "Utilities/utilities.h"

/*
    See imageexporter.h. The batch is a chain, not a loop: startNext() asks the pixel
    source for one image and returns, onImageReady() hands the encode and metadata copy
    to the worker, and onImageSaved() records the result and queues the next image
    through the event loop. That structural choice is what keeps the GUI alive and abort
    responsive across a long raw export -- the exporter never holds the GUI thread for
    any of an image's seconds-long steps.
*/

ImageExporter::ImageExporter(DataModel *dm, Metadata *metadata, QObject *parent)
    : QObject(parent), dm(dm), metadata(metadata)
{
    savePool.setMaxThreadCount(1);
}

ImageExporter::~ImageExporter()
{
    /* A save in flight posts its result back to this object; let it land (it is
       discarded with the object) rather than run against a half-destroyed one. */
    savePool.waitForDone();
}

bool ImageExporter::isRaw(const QString &fPath) const
{
    /* Metadata::hasJpg is Winnow's raw list: the formats whose preview is an embedded
       JPEG. */
    if (!metadata) return false;
    const QString ext = QFileInfo(VersionKey::sourceOf(fPath)).suffix().toLower();
    return metadata->hasJpg.contains(ext);
}

void ImageExporter::run(const QStringList &srcPaths, const ExportSettings &settings,
                        PixelSource source)
{
    if (G::isLogger) G::log("ImageExporter::run");
    if (!source) {
        G::issue("Warning", "No pixel source set.", "ImageExporter::run");
        return;
    }
    Batch batch{srcPaths, settings, std::move(source)};
    if (running) {                           // one batch at a time: wait behind it
        queue.append(std::move(batch));
        return;
    }
    start(std::move(batch));
}

void ImageExporter::start(Batch batch)
{
    s = batch.settings;
    paths = batch.paths;
    pixelSource = std::move(batch.source);
    index = 0;
    aborting = false;
    result = Result();
    result.settings = s;

    running = true;
    if (paths.isEmpty()) {
        finish();
        return;
    }
    emit progress(0, paths.count());
    startNext();
}

void ImageExporter::abort()
{
    if (!running) return;
    queue.clear();                           // Esc means stop exporting, not skip ahead
    aborting = true;                         // the in-flight image still completes
}

void ImageExporter::startNext()
{
    if (aborting || index >= paths.count()) {
        finish();
        return;
    }
    const QString fPath = paths.at(index);

    /* Exists::Skip is decided BEFORE the render: there is no point paying a full raw
       decode for an image whose destination we are not going to write. */
    if (s.exists == ExportSettings::Skip) {
        const QString dst = previewDestination(fPath, s, index + 1);
        if (!dst.isEmpty() && QFile::exists(dst)) {
            result.skipped << dst;
            advance();
            return;
        }
    }

    emit stage(index, paths.count(), Stage::Render, fPath);
    renderTimer.start();
    pixelSource(fPath, [this](bool ok, const QImage &img) {
        /* `done` may arrive on a worker thread; get back onto ours first. */
        QMetaObject::invokeMethod(this, [this, ok, img]() { onImageReady(ok, img); });
    });
}

void ImageExporter::onImageReady(bool ok, const QImage &img)
{
    if (!running) return;                    // finished/aborted while the render ran
    const QString fPath = paths.at(index);
    const qint64 renderMs = renderTimer.elapsed();

    if (!ok || img.isNull()) {
        result.failed << fPath;
        G::issue("Warning", "Could not render " + fPath, "ImageExporter::onImageReady");
        advance();
        return;
    }

    const QString folder = destinationFolder(fPath, s, /*create*/true);
    if (folder.isEmpty()) {
        result.failed << fPath;
        G::issue("Warning", "Could not create the export folder for " + fPath,
                 "ImageExporter::onImageReady");
        advance();
        return;
    }
    QString dst = folder + "/" + destinationBaseName(fPath, s, index + 1) + "." +
                  s.extension();
    /* Overwrite writes over it; UniqueName appends _1, _2 ... (the same helper every
       other Winnow write path uses). Skip was handled in startNext(). Resolved HERE,
       before the save is handed off, so the name is fixed on this thread. */
    if (s.exists != ExportSettings::Overwrite && QFile::exists(dst))
        Utilities::uniqueFilePath(dst);

    /* The encode and the ExifTool round trip go to the worker: on a 50 MP image they are
       seconds the GUI used to be frozen for. The stage signals are queued back here so
       the status bar can say which one is running. */
    const ExportSettings settings = s;
    const QString src = VersionKey::sourceOf(fPath);       // the file, for its tags
    const int i = index;
    const int total = paths.count();
    QPointer<ImageExporter> self(this);
    emit stage(i, total, Stage::Save, fPath);
    savePool.start([self, img, dst, src, settings, folder, fPath, i, total, renderMs]() {
        Saved saved;
        saved.dst = dst;
        QElapsedTimer t;
        t.start();
        const QImage out = resized(img, settings);
        saved.ok = save(out, dst, settings);
        saved.saveMs = t.restart();
        if (saved.ok && (settings.copyMetadata || settings.embedThumbnail)) {
            if (self)
                QMetaObject::invokeMethod(self, [self, i, total, fPath]() {
                    if (self) emit self->stage(i, total, Stage::Metadata, fPath);
                });
            copySourceMetadata(src, dst, out, settings);
            saved.metaMs = t.elapsed();
        }
        if (reportTime)
            qDebug().noquote() << QString("[ExportTime] %1  render %2 ms  save %3 ms  "
                                          "metadata %4 ms  (%5x%6)")
                                      .arg(QFileInfo(fPath).fileName())
                                      .arg(renderMs).arg(saved.saveMs).arg(saved.metaMs)
                                      .arg(out.width()).arg(out.height());
        if (self)
            QMetaObject::invokeMethod(self, [self, saved, folder]() {
                if (self) self->onImageSaved(saved, folder);
            });
    });
}

void ImageExporter::onImageSaved(const Saved &saved, const QString &folder)
{
    if (!running) return;
    if (saved.ok) {
        result.written << saved.dst;
        FileOps::onCreated({saved.dst});
        if (!result.folders.contains(folder)) result.folders << folder;
    }
    else {
        result.failed << paths.at(index);
        G::issue("Warning", "Could not save " + saved.dst, "ImageExporter::onImageSaved");
    }
    advance();
}

void ImageExporter::advance()
{
    ++index;
    emit progress(index, paths.count());
    QTimer::singleShot(0, this, [this]() { startNext(); });
}

void ImageExporter::finish()
{
    result.aborted = aborting;
    aborting = false;
    /* Not running while finished() is handled, so the handler can tell "all done" (clear
       the status row) from "a queued batch follows". The next queued batch starts from
       the event loop after it -- unless the handler started one itself. */
    running = false;
    emit finished(result);
    if (!running && !queue.isEmpty()) {
        Batch next = queue.takeFirst();
        QTimer::singleShot(0, this, [this, next]() mutable {
            if (running) { queue.prepend(std::move(next)); return; }
            start(std::move(next));
        });
    }
}

QString ImageExporter::destinationFolder(const QString &fPath,
                                         const ExportSettings &settings, bool create) const
{
    if (settings.dest == ExportSettings::SourceFolder)
        return QFileInfo(VersionKey::sourceOf(fPath)).dir().path();

    if (settings.dest == ExportSettings::SubfolderOfSource) {
        const QString parent = QFileInfo(VersionKey::sourceOf(fPath)).dir().path();
        const QString name = settings.subfolderName.trimmed();
        if (name.isEmpty()) return parent;              // no name: write alongside
        const QString folder = parent + "/" + name;
        if (create && !QFileInfo::exists(folder) && !QDir(parent).mkpath(name))
            return QString();
        return folder;
    }
    const QString folder = settings.folderPath;
    if (folder.isEmpty()) return QString();
    if (create && !QFileInfo::exists(folder) && !QDir().mkpath(folder)) return QString();
    return folder;
}

QString ImageExporter::destinationBaseName(const QString &fPath,
                                           const ExportSettings &settings, int seq) const
{
/*
    fPath is a row KEY. A version (virtual copy, Utilities/versionkey.h) is named from
    its SOURCE file plus its own name -- IMG_1-B&W.jpg, or IMG_1-v2.jpg until it is
    named -- so exporting a master and its versions together writes one file each
    instead of each overwriting the last. A template that places {VERSION} itself gets
    no automatic suffix.
*/
    const QFileInfo info(VersionKey::sourceOf(fPath));
    QString version;
    if (VersionKey::isVersion(fPath)) {
        if (dm) {
            const int row = dm->rowFromKey(fPath);
            if (row >= 0)
                version = dm->index(row, G::PathColumn).data(G::VersionNameRole).toString();
        }
        if (version.isEmpty()) version = QString("v%1").arg(VersionKey::idOf(fPath));
        // a file name cannot carry every character a version name can
        static const QRegularExpression bad(R"([\\/:*?"<>|\x00-\x1F])");
        version.replace(bad, "_");
    }
    QString base = info.baseName();
    bool placed = false;
    if (!settings.tokenTemplate.isEmpty() && dm) {
        const ImageMetadata m = dm->imMetadata(fPath);
        const QString parsed =
            TokenFileName::parse(m, info, settings.tokenTemplate, seq, version);
        if (!parsed.isEmpty()) base = parsed;
        placed = settings.tokenTemplate.contains("{VERSION}");
    }
    if (!version.isEmpty() && !placed) base += "-" + version;
    return base + settings.suffix;
}

QString ImageExporter::previewDestination(const QString &fPath,
                                          const ExportSettings &settings, int seq) const
{
/*
    The path run() would write for fPath, with no side effects: no folder is created, and
    the unique-name walk is not run (it depends on what earlier images in the batch have
    already written). The dialog uses this for its live example line, and startNext() for
    the Exists::Skip test -- which is why it must agree with what onImageReady() builds.
*/
    const QString folder = destinationFolder(fPath, settings, /*create*/false);
    const QString name = destinationBaseName(fPath, settings, seq) + "." +
                         settings.extension();
    return folder.isEmpty() ? name : folder + "/" + name;
}

bool ImageExporter::willTouchLoadedFolder(const QStringList &srcPaths,
                                          const ExportSettings &settings) const
{
/*
    Which folders this export would write into, tested against the datamodel's loaded
    folders. Cheap enough to call on every keystroke in the dialog because the number of
    DISTINCT destinations is small even for a huge selection: a chosen folder is one, and
    a subfolder-of-source is one per source folder.

    DataModel::isFolderLoaded owns the comparison, so what the dialog ENABLES and what
    MW::onExportFinished later ACTS on cannot drift apart -- a folder typed with a
    trailing slash must not be "loaded" for one of them and not the other.
*/
    if (!dm) return false;

    auto loaded = [this](const QString &folder) { return dm->isFolderLoaded(folder); };

    if (settings.dest == ExportSettings::ChosenFolder)
        return loaded(destinationFolder(QString(), settings, /*create*/false));

    /* Source folder / subfolder of source: one destination per SOURCE folder. */
    QSet<QString> seenParents;
    for (const QString &fPath : srcPaths) {
        const QString parent = QFileInfo(VersionKey::sourceOf(fPath)).dir().path();
        if (seenParents.contains(parent)) continue;     // same dest as an earlier file
        seenParents.insert(parent);
        if (loaded(destinationFolder(fPath, settings, /*create*/false))) return true;
    }
    return false;
}

QSize ImageExporter::OutputSize(const QSize &src, const ExportSettings &settings)
{
    if (settings.sizing == ExportSettings::FullSize || src.isEmpty()) return src;

    const int longEdge = qMax(src.width(), src.height());
    double factor = 1.0;
    if (settings.sizing == ExportSettings::LongEdge)
        factor = double(settings.longEdgePx) / double(longEdge);
    else
        factor = double(settings.percent) / 100.0;
    if (settings.dontEnlarge && factor > 1.0) return src;
    if (qFuzzyCompare(factor, 1.0)) return src;

    return QSize(qMax(1, int(src.width() * factor + 0.5)),
                 qMax(1, int(src.height() * factor + 0.5)));
}

QImage ImageExporter::resized(const QImage &src, const ExportSettings &settings)
{
    if (src.isNull()) return src;
    const QSize size = OutputSize(src.size(), settings);
    if (size == src.size()) return src;
    /* Smooth is the right filter for a downscale and this is not an interactive path. */
    return src.scaled(size, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
}

OutputTransform::Space ImageExporter::renderSpace(OutputSpace space)
{
    switch (space) {
    case OutputSpace::DisplayP3: return OutputTransform::Space::DisplayP3;
    case OutputSpace::AdobeRGB:  return OutputTransform::Space::AdobeRGB;
    case OutputSpace::sRGB:      break;
    }
    return OutputTransform::Space::sRGB;
}

bool ImageExporter::save(const QImage &img, const QString &dstPath,
                         const ExportSettings &s)
{
    if (img.isNull()) return false;

    QImage out = img;

    /* Bit depth. A 16-bit render going to an 8-bit-only format (JPEG, HEIC, BMP) is
       converted down here rather than handed to the writer, so the conversion is explicit
       and the writer never has to guess. */
    const ExportFormat fmt = ExportFormats::find(s.format);
    const bool deep = out.format() == QImage::Format_RGBX64 ||
                      out.format() == QImage::Format_RGBA64 ||
                      out.format() == QImage::Format_RGBA64_Premultiplied;
    if (deep && !(fmt.depth16 && s.bitDepth == 16))
        out = out.convertToFormat(QImage::Format_RGB888);

    /*
        Colour space. The file must be TAGGED, or every viewer guesses -- exports used to
        be untagged entirely.

        Two kinds of pixels arrive here and they need opposite treatment:

          Develop  -- OutputTransform already ENCODED them in the chosen space and
                      developPixelSource tagged them as such, so the tag below matches and
                      no conversion happens.
          Preview  -- the plain browse decode (File > Save Preview as). Those pixels are
                      whatever the decoder produced, so they are CONVERTED to the target.
                      An untagged decode is treated as sRGB, which is what untagged means
                      in practice.

        Doing this by tag rather than by which PixelSource ran keeps the rule local: an
        image that already is the target costs nothing, and one that is not gets a real
        conversion instead of a lie in its header.
    */
    const QColorSpace target = OutputTransform::ColorSpaceOf(renderSpace(s.space));
    if (!out.colorSpace().isValid()) out.setColorSpace(QColorSpace::SRgb);
    if (out.colorSpace() != target) out.convertToColorSpace(target);

    QImageWriter writer(dstPath, ExportFormats::writerFormat(s.format));
    if (fmt.hasQuality) writer.setQuality(s.quality);
    /* Qt's TIFF handler reads compression as 0 = none, 1 = LZW. */
    if (fmt.hasTiffComp) writer.setCompression(s.tiffCompression);

    if (!writer.write(out)) {
        G::issue("Warning", "QImageWriter: " + writer.errorString(),
                 "ImageExporter::save", -1, dstPath);
        return false;
    }
    return true;
}

void ImageExporter::copySourceMetadata(const QString &srcPath, const QString &dstPath,
                                       const QImage &img, const ExportSettings &s)
{
/*
    Copy the source's tags onto the exported file, and optionally embed a thumbnail --
    the same ExifTool calls EmbelExport makes. Without this an export carries no camera,
    lens, date or copyright, which is what File > Save Preview as has always (silently)
    done.

    NOT the ICC profile, deliberately. The exported pixels are in the space save() chose
    and tagged, which is generally NOT the source's: a raw's profile describes the camera
    space, and a JPEG's describes whatever it was shot in -- neither survives the develop
    pipeline. Copying it over Qt's embedded profile would leave the file describing itself
    wrongly, which is worse than no profile at all. So the copy explicitly EXCLUDES
    icc_profile rather than relying on -all:all's block-tag behaviour.

    ExifTool is a per-file process round trip, so this is the slowest step per image after
    the render itself; it is behind a checkbox for that reason. WORKER THREAD (savePool):
    the ExifTool object, and the QProcess inside it, are created and finished here, and
    only blocking waits are used, so no event loop is needed on this thread.
*/
    ExifTool et;
    et.setOverWrite(true);
    /* Every pixel source hands over UPRIGHT pixels (a raw is rotated in the develop
       render, everything else comes from the rotated decode), so the source's Orientation
       must not come along: a portrait raw exported as a portrait TIFF still tagged 8 is
       rotated a second time by any reader that honours the tag. */
    /*
        The source's EMBEDDED IMAGES must not come along either. -all:all copies IFD1,
        which is the camera's ThumbnailImage: a raw's as-shot 160x120 picture, not the
        developed one, and not even the export's aspect ratio. Winnow (and most viewers)
        build the icon from that thumbnail, so an export with a brightened develop showed
        the dark as-shot thumb. Embed Thumbnail writes a correct one below.

        EXIF ColorSpace is copied too, and the source's (a raw says sRGB) contradicts
        the profile save() embedded. It is set to what the pixels are: 1 for sRGB,
        0xFFFF (Uncalibrated) otherwise, with the DCF InteropIndex R03 for Adobe RGB.
    */
    QStringList extra = {"--IFD1:all", "--ThumbnailImage", "--PreviewImage",
                         "--JpgFromRaw", "--OtherImage"};
    switch (s.space) {
    case OutputSpace::sRGB:
        extra << "-ExifIFD:ColorSpace#=1" << "-InteropIndex=R98";
        break;
    case OutputSpace::AdobeRGB:
        extra << "-ExifIFD:ColorSpace#=65535" << "-InteropIndex=R03";
        break;
    case OutputSpace::DisplayP3:
        extra << "-ExifIFD:ColorSpace#=65535" << "-InteropIndex=";
        break;
    }
    if (s.copyMetadata)
        et.copyAllTags(srcPath, dstPath, /*excludeIcc*/ true, /*upright*/ true, extra);
    if (s.embedThumbnail && !img.isNull()) {
        /* addThumb needs a thumbnail FILE; write one beside the export and remove it
           again, as EmbelExport does. */
        const QFileInfo info(dstPath);
        const QString thumbPath =
            info.dir().path() + "/" + info.completeBaseName() + "_thumb.jpg";
        const QImage thumb = img.scaled(160, 160, Qt::KeepAspectRatio,
                                        Qt::SmoothTransformation);
        if (thumb.save(thumbPath, "JPG", 60)) {
            et.addThumb(thumbPath, dstPath);
            QFile::remove(thumbPath);
        }
    }
    et.close();
}
