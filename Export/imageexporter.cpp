#include "Export/imageexporter.h"

#include <QColorSpace>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImageWriter>
#include <QTimer>

#include "Datamodel/datamodel.h"
#include "Main/global.h"
#include "Metadata/ExifTool.h"
#include "Metadata/metadata.h"
#include "Utilities/tokenfilename.h"
#include "Utilities/utilities.h"

/*
    See imageexporter.h. The batch is a chain, not a loop: startNext() asks the pixel
    source for one image and returns, and onImageReady() saves it and queues the next
    through the event loop. That single structural choice is what keeps the GUI alive and
    abort responsive across a long raw export -- there is no point in the batch where the
    exporter is holding the thread for more than one image's save.
*/

ImageExporter::ImageExporter(DataModel *dm, Metadata *metadata, QObject *parent)
    : QObject(parent), dm(dm), metadata(metadata) {}

ImageExporter::~ImageExporter() {}

void ImageExporter::setPixelSource(PixelSource source)
{
    pixelSource = std::move(source);
}

void ImageExporter::run(const QStringList &srcPaths, const ExportSettings &settings)
{
    if (G::isLogger) G::log("ImageExporter::run");
    if (running) return;                     // one batch at a time
    if (!pixelSource) {
        G::issue("Warning", "No pixel source set.", "ImageExporter::run");
        return;
    }

    s = settings;
    paths = srcPaths;
    index = 0;
    aborting = false;
    result = Result();

    if (paths.isEmpty()) {
        emit finished(result);
        return;
    }

    running = true;
    emit progress(0, paths.count());
    startNext();
}

void ImageExporter::abort()
{
    if (!running) return;
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
            ++index;
            emit progress(index, paths.count());
            QTimer::singleShot(0, this, [this]() { startNext(); });
            return;
        }
    }

    pixelSource(fPath, [this](bool ok, const QImage &img) {
        /* `done` may arrive on a worker thread; get back onto ours first. */
        QMetaObject::invokeMethod(this, [this, ok, img]() { onImageReady(ok, img); });
    });
}

void ImageExporter::onImageReady(bool ok, const QImage &img)
{
    if (!running) return;                    // finished/aborted while the render ran
    const QString fPath = paths.at(index);

    if (!ok || img.isNull()) {
        result.failed << fPath;
        G::issue("Warning", "Could not render " + fPath, "ImageExporter::onImageReady");
    }
    else {
        const QString folder = destinationFolder(fPath, s, /*create*/true);
        if (folder.isEmpty()) {
            result.failed << fPath;
            G::issue("Warning", "Could not create the export folder for " + fPath,
                     "ImageExporter::onImageReady");
        }
        else {
            QString dst = folder + "/" +
                          destinationBaseName(fPath, s, index + 1) + "." + s.extension();
            /* Overwrite writes over it; UniqueName appends _1, _2 ... (the same helper
               every other Winnow write path uses). Skip was handled in startNext(). */
            if (s.exists != ExportSettings::Overwrite && QFile::exists(dst))
                Utilities::uniqueFilePath(dst);

            const QImage out = resized(img);
            if (save(out, dst)) {
                if (s.copyMetadata || s.embedThumbnail)
                    copySourceMetadata(fPath, dst, out);
                result.written << dst;
                if (!result.folders.contains(folder)) result.folders << folder;
            }
            else {
                result.failed << fPath;
                G::issue("Warning", "Could not save " + dst, "ImageExporter::onImageReady");
            }
        }
    }

    ++index;
    emit progress(index, paths.count());
    QTimer::singleShot(0, this, [this]() { startNext(); });
}

void ImageExporter::finish()
{
    running = false;
    result.aborted = aborting;
    aborting = false;
    emit finished(result);
}

QString ImageExporter::destinationFolder(const QString &fPath,
                                         const ExportSettings &settings, bool create) const
{
    if (settings.dest == ExportSettings::SourceFolder)
        return QFileInfo(fPath).dir().path();

    if (settings.dest == ExportSettings::SubfolderOfSource) {
        const QString parent = QFileInfo(fPath).dir().path();
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
    const QFileInfo info(fPath);
    QString base = info.baseName();
    if (!settings.tokenTemplate.isEmpty() && dm) {
        const ImageMetadata m = dm->imMetadata(fPath);
        const QString parsed =
            TokenFileName::parse(m, info, settings.tokenTemplate, seq);
        if (!parsed.isEmpty()) base = parsed;
    }
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
        const QString parent = QFileInfo(fPath).dir().path();
        if (seenParents.contains(parent)) continue;     // same dest as an earlier file
        seenParents.insert(parent);
        if (loaded(destinationFolder(fPath, settings, /*create*/false))) return true;
    }
    return false;
}

QImage ImageExporter::resized(const QImage &src) const
{
    if (s.sizing == ExportSettings::FullSize || src.isNull()) return src;

    const int longEdge = qMax(src.width(), src.height());
    double factor = 1.0;
    if (s.sizing == ExportSettings::LongEdge) {
        if (longEdge <= 0) return src;
        factor = double(s.longEdgePx) / double(longEdge);
    }
    else {
        factor = double(s.percent) / 100.0;
    }
    if (s.dontEnlarge && factor > 1.0) return src;
    if (qFuzzyCompare(factor, 1.0)) return src;

    const int w = qMax(1, int(src.width() * factor + 0.5));
    const int h = qMax(1, int(src.height() * factor + 0.5));
    /* Smooth is the right filter for a downscale and this is not an interactive path. */
    return src.scaled(w, h, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
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

bool ImageExporter::save(const QImage &img, const QString &dstPath) const
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
                                       const QImage &img)
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
    the render itself; it is behind a checkbox for that reason.
*/
    if (G::isLogger) G::log("ImageExporter::copySourceMetadata");

    ExifTool et;
    et.setOverWrite(true);
    if (s.copyMetadata) et.copyAllTags(srcPath, dstPath, /*excludeIcc*/ true);
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
