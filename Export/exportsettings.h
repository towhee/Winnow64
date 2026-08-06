#ifndef EXPORTSETTINGS_H
#define EXPORTSETTINGS_H

#include <QImageWriter>
#include <QString>
#include <QStringList>
#include <QVector>

/*
    Everything the export engine needs to turn a rendered QImage into a file on disk:
    where it goes, what it is called, what format it is written in and how big it is.

    ExportSettings is deliberately source-agnostic. It says nothing about WHERE the pixels
    come from -- ImageExporter takes that as a callback -- so the same settings drive a
    Develop export, a plain "Save Preview as" and (later) an Embellish export. That is the
    point of the struct: Winnow grew three export loops with three overlapping and
    incompatible sets of options, and this is the one set.

    PERSISTENCE. The whole struct round-trips through QSettings (see ExportPresets), so
    every field name here is a PUBLISHED FORMAT key -- add freely, never rename or rescale
    in place. See Export/exportpresets.h for the versioning contract.
*/

/* Output colour space. This is the export layer's PUBLISHED preset key -- kept separate
   from OutputTransform::Space (which does the actual conversion) because Develop must not
   depend on Export and these numbers can never be renumbered.

   ImageExporter::renderSpace() is the one place the two are tied together. The develop
   source RENDERS in the chosen space (OutputTransform) and hands the image over tagged;
   the preview source hands over the browse decode and save() CONVERTS it. Either way the
   embedded profile describes the pixels actually written. */
enum class OutputSpace { sRGB = 0, DisplayP3 = 1, AdobeRGB = 2 };

/*
    One writable output format. The table below is Winnow's CURATED list -- Qt can write
    more (ico, tga, ppm, single-frame gif), but they are not formats anyone should be
    exporting photographs to, and offering them invites bad output.

    Availability is resolved at RUNTIME against QImageWriter::supportedImageFormats(),
    because the plugin set differs by platform and by deployment: HEIC writes on macOS via
    the bundled qmacheif plugin and is simply absent on a stock Windows build. Resolving
    it at runtime means a missing plugin drops the entry from the dialog instead of
    producing a save failure the user cannot act on.
*/
struct ExportFormat {
    QString key;          // QImageWriter format string, also the file extension
    QString label;        // what the dialog shows
    bool    depth16;      // can be written at 16 bits per channel
    bool    hasQuality;   // lossy: a 0..100 quality applies
    bool    hasTiffComp;  // offers the none/LZW compression choice
};

namespace ExportFormats {

/* The curated table, in dialog order. Not filtered -- see available(). */
inline QVector<ExportFormat> all()
{
    return {
        { "jpg",  "JPEG",       false, true,  false },
        { "png",  "PNG",        true,  false, false },
        { "tif",  "TIFF",       true,  false, true  },
        { "heic", "HEIC",       false, true,  false },
        { "bmp",  "BMP",        false, false, false },
    };
}

/* The curated table minus anything this build cannot actually write. */
inline QVector<ExportFormat> available()
{
    QStringList supported;
    const QList<QByteArray> fmts = QImageWriter::supportedImageFormats();
    for (const QByteArray &f : fmts) supported << QString::fromLatin1(f).toLower();

    QVector<ExportFormat> out;
    for (const ExportFormat &f : all()) {
        /* "tif" and "tiff" (and "heic"/"heif") are the same handler under different
           names, so accept either spelling as proof the plugin is present. */
        const bool ok = supported.contains(f.key)
                     || (f.key == "tif"  && supported.contains("tiff"))
                     || (f.key == "heic" && supported.contains("heif"));
        if (ok) out.append(f);
    }
    return out;
}

/* The table entry for key, or a JPEG-shaped fallback if it is not a known format. */
inline ExportFormat find(const QString &key)
{
    for (const ExportFormat &f : all())
        if (f.key == key) return f;
    return { "jpg", "JPEG", false, true, false };
}

/*
    The spelling QImageWriter actually knows for this format. Our keys double as file
    EXTENSIONS ("tif", "heic"), but a Qt image plugin registers whichever names it likes
    ("tiff", "heif"), and QImageWriter fails outright on a name it does not recognise. So
    resolve against what the build reports rather than assuming our spelling is accepted.
*/
inline QByteArray writerFormat(const QString &key)
{
    const QList<QByteArray> fmts = QImageWriter::supportedImageFormats();
    const QByteArray want = key.toLower().toLatin1();
    if (fmts.contains(want)) return want;
    /* Known alternate spellings of the same handler. */
    if (key == "tif"  && fmts.contains("tiff")) return QByteArray("tiff");
    if (key == "tiff" && fmts.contains("tif"))  return QByteArray("tif");
    if (key == "heic" && fmts.contains("heif")) return QByteArray("heif");
    if (key == "heif" && fmts.contains("heic")) return QByteArray("heic");
    return want;                    // let the writer report the failure
}

} // namespace ExportFormats

struct ExportSettings {
    /* ---- Destination ----
       The numbers are PUBLISHED (settings key "dest"): SourceFolder is appended as 2
       rather than inserted at the top where it reads in the dialog, because renumbering
       would turn every stored "subfolder" preset into something else. */
    enum Dest { SubfolderOfSource = 0, ChosenFolder = 1, SourceFolder = 2 };
    Dest    dest = ChosenFolder;
    QString subfolderName = "Developed";   // used when dest == SubfolderOfSource
    QString folderPath;                    // used when dest == ChosenFolder
    /* Insert the exported files into the datamodel when they land in a folder that is
       currently loaded, instead of rescanning that folder to pick them up. */
    bool    addToFolderView = false;

    /* ---- Naming ---- */
    QString tokenTemplate;                 // empty => the original base name
    QString suffix = "_dev";
    /* What to do when the destination file already exists. UniqueName appends _1, _2 ...
       (Utilities::uniqueFilePath), matching what every other Winnow write path does. */
    enum Exists { Overwrite, UniqueName, Skip };
    Exists  exists = UniqueName;

    /* ---- Format ---- */
    QString format = "jpg";                // ExportFormat::key
    int     bitDepth = 8;                  // 8 or 16; honoured only if the format allows
    int     quality = 92;                  // 0..100, lossy formats only
    int     tiffCompression = 1;           // QImageWriter::setCompression: 0 none, 1 LZW
    OutputSpace space = OutputSpace::sRGB;
    bool    copyMetadata = true;           // ExifTool: copy tags + ICC from the source
    bool    embedThumbnail = false;        // ExifTool: write an embedded thumbnail

    /* ---- Sizing ---- */
    enum Size { FullSize, LongEdge, Percent };
    Size    sizing = FullSize;
    int     longEdgePx = 3000;
    int     percent = 50;
    bool    dontEnlarge = true;            // never scale a small image UP to the target

    /* True when the chosen format can actually deliver the chosen depth. */
    bool wants16Bit() const
    {
        return bitDepth == 16 && ExportFormats::find(format).depth16;
    }

    /* The extension to write: the format key for every entry in the table. */
    QString extension() const { return format; }
};

#endif // EXPORTSETTINGS_H
