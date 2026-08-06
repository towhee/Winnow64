#include "Export/exportpresets.h"
#include "Main/global.h"

/*
    See exportpresets.h. Reads / writes named export presets in QSettings.

    Unlike a develop preset, an export preset is COMPLETE: read() starts from a
    default-constructed ExportSettings and overwrites each field from the stored value,
    so a key added after a preset was saved keeps its struct default instead of reading
    back as 0/empty. That is the whole forward-compatibility story -- see the version /
    migration contract in the header before changing any key name below.
*/

namespace {
constexpr const char *kRoot = "Export Presets";
constexpr const char *kLast = "Export Settings/Last";
}

ExportPresets::ExportPresets(QSettings *setting, QObject *parent)
    : QObject(parent), setting(setting) {}

QString ExportPresets::groupOf(const QString &name) const
{
    return QString("%1/%2").arg(kRoot, name);
}

QStringList ExportPresets::names() const
{
    if (!setting) return QStringList();
    setting->beginGroup(kRoot);
    QStringList list = setting->childGroups();
    setting->endGroup();
    list.sort(Qt::CaseInsensitive);
    return list;
}

bool ExportPresets::contains(const QString &name) const
{
    return names().contains(name, Qt::CaseInsensitive);
}

/*
    Read one settings GROUP into an ExportSettings. Shared by read() (a named preset) and
    readLast() (the unnamed "what the dialog last used" slot) -- the two differ only in
    which group they open, so the field list lives here once. Adding a field means adding
    one line here and one in writeGroup; keep the two in step.
*/
static ExportSettings readGroup(QSettings *setting, const QString &group)
{
    ExportSettings s;                       // defaults: an absent key keeps this value
    if (!setting) return s;
    setting->beginGroup(group);

    s.dest            = ExportSettings::Dest(
                            setting->value("dest", int(s.dest)).toInt());
    s.subfolderName   = setting->value("subfolderName", s.subfolderName).toString();
    s.folderPath      = setting->value("folderPath", s.folderPath).toString();
    s.addToFolderView = setting->value("addToFolderView", s.addToFolderView).toBool();

    s.tokenTemplate   = setting->value("tokenTemplate", s.tokenTemplate).toString();
    s.suffix          = setting->value("suffix", s.suffix).toString();
    s.exists          = ExportSettings::Exists(
                            setting->value("exists", int(s.exists)).toInt());

    s.format          = setting->value("format", s.format).toString();
    s.bitDepth        = setting->value("bitDepth", s.bitDepth).toInt();
    s.quality         = setting->value("quality", s.quality).toInt();
    s.tiffCompression = setting->value("tiffCompression", s.tiffCompression).toInt();
    s.space           = OutputSpace(setting->value("space", int(s.space)).toInt());
    s.copyMetadata    = setting->value("copyMetadata", s.copyMetadata).toBool();
    s.embedThumbnail  = setting->value("embedThumbnail", s.embedThumbnail).toBool();

    s.sizing          = ExportSettings::Size(
                            setting->value("sizing", int(s.sizing)).toInt());
    s.longEdgePx      = setting->value("longEdgePx", s.longEdgePx).toInt();
    s.percent         = setting->value("percent", s.percent).toInt();
    s.dontEnlarge     = setting->value("dontEnlarge", s.dontEnlarge).toBool();

    setting->endGroup();

    /* Clamp what a hand-edited or corrupt QSettings could otherwise make nonsensical.
       These are user-writable files; a bad quality or a zero long edge must not reach
       QImageWriter or the scaler. */
    s.bitDepth        = (s.bitDepth == 16) ? 16 : 8;
    s.quality         = qBound(1, s.quality, 100);
    s.tiffCompression = (s.tiffCompression == 0) ? 0 : 1;
    s.longEdgePx      = qBound(16, s.longEdgePx, 60000);
    s.percent         = qBound(1, s.percent, 100);
    return s;
}

static void writeGroup(QSettings *setting, const QString &group, const ExportSettings &s)
{
    if (!setting) return;
    setting->beginGroup(group);

    setting->setValue("version", ExportPresets::kVersion);

    setting->setValue("dest", int(s.dest));
    setting->setValue("subfolderName", s.subfolderName);
    setting->setValue("folderPath", s.folderPath);
    setting->setValue("addToFolderView", s.addToFolderView);

    setting->setValue("tokenTemplate", s.tokenTemplate);
    setting->setValue("suffix", s.suffix);
    setting->setValue("exists", int(s.exists));

    setting->setValue("format", s.format);
    setting->setValue("bitDepth", s.bitDepth);
    setting->setValue("quality", s.quality);
    setting->setValue("tiffCompression", s.tiffCompression);
    setting->setValue("space", int(s.space));
    setting->setValue("copyMetadata", s.copyMetadata);
    setting->setValue("embedThumbnail", s.embedThumbnail);

    setting->setValue("sizing", int(s.sizing));
    setting->setValue("longEdgePx", s.longEdgePx);
    setting->setValue("percent", s.percent);
    setting->setValue("dontEnlarge", s.dontEnlarge);

    setting->endGroup();
}

ExportSettings ExportPresets::read(const QString &name) const
{
    if (!setting || name.isEmpty()) return ExportSettings();

    /* Convert anything older than kVersion BEFORE the values are read into the struct.
       Presets written before the version key existed are v1 by definition. */
    setting->beginGroup(groupOf(name));
    const int v = setting->value("version", 1).toInt();
    setting->endGroup();
    if (v != kVersion) migrate(name, v);

    return readGroup(setting, groupOf(name));
}

void ExportPresets::write(const QString &name, const ExportSettings &s)
{
    if (!setting || name.isEmpty()) return;
    writeGroup(setting, groupOf(name), s);
    emit presetsChanged();
}

bool ExportPresets::rename(const QString &from, const QString &to)
{
    if (!setting || from.isEmpty() || to.isEmpty() || from == to) return false;
    if (!contains(from) || contains(to)) return false;
    const ExportSettings s = read(from);
    writeGroup(setting, groupOf(to), s);
    setting->remove(groupOf(from));
    emit presetsChanged();
    return true;
}

void ExportPresets::remove(const QString &name)
{
    if (!setting || name.isEmpty()) return;
    setting->remove(groupOf(name));
    emit presetsChanged();
}

ExportSettings ExportPresets::readLast() const
{
    return readGroup(setting, kLast);
}

void ExportPresets::writeLast(const ExportSettings &s)
{
    /* No presetsChanged(): this slot is not a named preset and is not in the menu. */
    writeGroup(setting, kLast, s);
}

void ExportPresets::migrate(const QString &name, int fromVersion) const
{
/*
    Convert an older stored schema in place. Nothing to do at v1 -- this exists so the
    first real migration has an obvious home, and so the version read in read() is not
    silently ignored. When a key ever has to change meaning: convert here, then write the
    new version key.
*/
    Q_UNUSED(name)
    Q_UNUSED(fromVersion)
}
