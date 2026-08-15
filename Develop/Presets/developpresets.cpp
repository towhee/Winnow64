#include "Develop/Presets/developpresets.h"
#include "Main/global.h"

/*
    See developpresets.h. Reads / writes named develop presets in QSettings. Everything
    here is deliberately key-by-key: a preset holds only the settings the user ticked,
    and read() must not invent the rest.
*/

DevelopPresets::DevelopPresets(QSettings *setting, QObject *parent)
    : QObject(parent), setting(setting) {}

QStringList DevelopPresets::names() const
{
    if (!setting) return QStringList();
    setting->beginGroup(kRoot);
    QStringList list = setting->childGroups();
    setting->endGroup();
    list.sort(Qt::CaseInsensitive);
    return list;
}

bool DevelopPresets::contains(const QString &name) const
{
    return names().contains(name, Qt::CaseInsensitive);
}

DevelopPreset DevelopPresets::read(const QString &name) const
{
    return readGroup(kRoot, name);
}

DevelopPreset DevelopPresets::readGroup(const char *root, const QString &name) const
{
    DevelopPreset p;
    if (!setting || name.isEmpty()) return p;
    p.name = name;

    setting->beginGroup(root);
    setting->beginGroup(name);
    /* Presets written before the version key existed are v1 by definition -- v1 is the
       schema they were written against -- so the default here must stay 1 forever. */
    p.version     = setting->value("version", 1).toInt();
    p.sourceScope = setting->value("sourceScope").toString();
    p.sourceImage = setting->value("sourceImage").toString();

    /* Global: the per-image settings. childKeys() is what makes the merge work -- only
       the keys actually written are reported, and an absent key stays absent. */
    setting->beginGroup("Global");
    const QStringList gkeys = setting->childKeys();
    for (const QString &k : gkeys) p.globals.insert(k, setting->value(k));
    const int n = setting->beginReadArray("spots");
    for (int i = 0; i < n; ++i) {
        setting->setArrayIndex(i);
        FillSpot spot;
        spot.paramsJson = setting->value("params").toString();
        spot.enabled    = setting->value("enabled", true).toBool();
        p.spots.append(spot);
    }
    setting->endArray();
    setting->endGroup();                // Global

    /* Adjustments. Presets written by the first (save-only) build used "Global edits",
       when a preset could only come from the Global scope. */
    const QString edits =
        setting->childGroups().contains(kEdits) ? QString(kEdits) : QString(kEditsLegacy);
    setting->beginGroup(edits);
    const QStringList pkeys = setting->childKeys();
    for (const QString &k : pkeys) p.params.insert(k, setting->value(k));
    setting->endGroup();

    setting->endGroup();                // name
    setting->endGroup();                // root

    /* Convert anything older than kVersion BEFORE the values reach assignParam. */
    if (p.version != kVersion) migrate(p);
    return p;
}

void DevelopPresets::migrate(DevelopPreset &p)
{
    /* No conversions yet -- kVersion is still 1, so every stored preset is current and
       this only normalises the stamp. Each future bump appends one ordered step:

           if (p.version < 2) { ...rewrite the affected keys in p.params / p.globals...; }

       written so a v1 preset falls through every step in turn. A preset from a NEWER
       build than this one (version > kVersion) is left alone: assignParam already
       ignores keys it does not know, which degrades better than guessing backwards. */
    if (p.version > kVersion) return;
    p.version = kVersion;
}

void DevelopPresets::write(const DevelopPreset &p)
{
    if (!setting || p.name.isEmpty()) return;
    if (G::isLogger) G::log("DevelopPresets::write", p.name);
    writeGroup(kRoot, p);
    setting->sync();                    // flush now (QSettings otherwise defers it)
    emit changed();
}

void DevelopPresets::writeGroup(const char *root, const DevelopPreset &p)
{
    if (!setting || p.name.isEmpty()) return;

    setting->beginGroup(root);
    setting->beginGroup(p.name);
    setting->remove("");                // overwrite: clear any prior contents
    /* Always stamp the CURRENT schema, whatever version p arrived as: what is about to
       be written is by definition kVersion-shaped (read() has already migrated). */
    setting->setValue("version", kVersion);
    if (!p.sourceScope.isEmpty()) setting->setValue("sourceScope", p.sourceScope);
    if (!p.sourceImage.isEmpty()) setting->setValue("sourceImage", p.sourceImage);

    if (!p.globals.isEmpty() || !p.spots.isEmpty()) {
        setting->beginGroup("Global");
        for (auto it = p.globals.constBegin(); it != p.globals.constEnd(); ++it)
            setting->setValue(it.key(), it.value());
        if (!p.spots.isEmpty()) {
            setting->beginWriteArray("spots");
            for (int i = 0; i < p.spots.size(); ++i) {
                setting->setArrayIndex(i);
                setting->setValue("params", p.spots[i].paramsJson);
                setting->setValue("enabled", p.spots[i].enabled);
            }
            setting->endArray();
        }
        setting->endGroup();
    }

    if (!p.params.isEmpty()) {
        setting->beginGroup(kEdits);
        for (auto it = p.params.constBegin(); it != p.params.constEnd(); ++it)
            setting->setValue(it.key(), it.value());
        setting->endGroup();
    }

    setting->endGroup();                // name
    setting->endGroup();                // root
}

DevelopPreset DevelopPresets::readClipboard() const
{
    return readGroup(kClipRoot, kClipName);
}

void DevelopPresets::writeClipboard(const DevelopPreset &p)
{
    if (!setting) return;
    if (G::isLogger) G::log("DevelopPresets::writeClipboard");
    /* One slot, always the same key: every Copy replaces what was there. No changed()
       signal -- the Presets list does not show the buffer, so nothing to reload. */
    DevelopPreset buf = p;
    buf.name = kClipName;
    writeGroup(kClipRoot, buf);
    setting->sync();
}

bool DevelopPresets::hasClipboard() const
{
    if (!setting) return false;
    setting->beginGroup(kClipRoot);
    const bool has = setting->childGroups().contains(kClipName);
    setting->endGroup();
    /* A slot holding nothing (everything unticked) is "nothing copied": pasting it would
       do nothing, and Paste should read as unavailable rather than silently no-op. */
    return has && !readClipboard().isEmpty();
}

bool DevelopPresets::rename(const QString &from, const QString &to)
{
    if (!setting || from.isEmpty() || to.isEmpty() || from == to) return false;
    if (contains(to)) return false;
    /* Read-then-write is enough: a preset is small and read() captures everything the
       store holds, so there is no need to walk the QSettings tree by hand. */
    DevelopPreset p = read(from);
    if (p.name.isEmpty()) return false;
    p.name = to;
    setting->beginGroup(kRoot);
    setting->remove(from);
    setting->endGroup();
    write(p);                           // emits changed()
    return true;
}

void DevelopPresets::remove(const QString &name)
{
    if (!setting || name.isEmpty()) return;
    if (G::isLogger) G::log("DevelopPresets::remove", name);
    setting->beginGroup(kRoot);
    setting->remove(name);
    setting->endGroup();
    setting->sync();
    emit changed();
}

void DevelopPresets::assignParam(const QString &key, const QVariant &v, EditParams &p)
{
    /* RAW values under the EditStack JSON field names (see paramsToJson), NOT the dock's
       slider keys -- DevelopProperties::applyKeyToParams is the other direction and
       div-scales (0..100 -> 0..1). Keep the two apart. */
    const float f = v.toFloat();
    if      (key == "temp")                p.temp                = f;
    else if (key == "tint")                p.tint                = f;
    else if (key == "wbPreset")            p.wbPreset            = v.toInt();
    else if (key == "exposure")            p.exposure            = f;
    else if (key == "contrast")            p.contrast            = f;
    else if (key == "highlights")          p.highlights          = f;
    else if (key == "shadows")             p.shadows             = f;
    else if (key == "whites")              p.whites              = f;
    else if (key == "blacks")              p.blacks              = f;
    else if (key == "texture")             p.texture             = f;
    else if (key == "dehaze")              p.dehaze              = f;
    else if (key == "red")                 p.red                 = f;
    else if (key == "green")               p.green               = f;
    else if (key == "blue")                p.blue                = f;
    else if (key == "hue")                 p.hue                 = f;
    else if (key == "saturation")          p.saturation          = f;
    else if (key == "vibrance")            p.vibrance            = f;
    else if (key == "luminance")           p.luminance           = f;
    else if (key == "calRedHue")           p.calRedHue           = f;
    else if (key == "calRedSat")           p.calRedSat           = f;
    else if (key == "calGreenHue")         p.calGreenHue         = f;
    else if (key == "calGreenSat")         p.calGreenSat         = f;
    else if (key == "calBlueHue")          p.calBlueHue          = f;
    else if (key == "calBlueSat")          p.calBlueSat          = f;
    else if (key == "gradeShadowHue")      p.gradeShadowHue      = f;
    else if (key == "gradeShadowSat")      p.gradeShadowSat      = f;
    else if (key == "gradeShadowLum")      p.gradeShadowLum      = f;
    else if (key == "gradeMidHue")         p.gradeMidHue         = f;
    else if (key == "gradeMidSat")         p.gradeMidSat         = f;
    else if (key == "gradeMidLum")         p.gradeMidLum         = f;
    else if (key == "gradeHighHue")        p.gradeHighHue        = f;
    else if (key == "gradeHighSat")        p.gradeHighSat        = f;
    else if (key == "gradeHighLum")        p.gradeHighLum        = f;
    else if (key == "gradeGlobalHue")      p.gradeGlobalHue      = f;
    else if (key == "gradeGlobalSat")      p.gradeGlobalSat      = f;
    else if (key == "gradeGlobalLum")      p.gradeGlobalLum      = f;
    else if (key == "gradeBlending")       p.gradeBlending       = f;
    else if (key == "gradeBalance")        p.gradeBalance        = f;
    else if (key == "localDenoiseLuma")    p.localDenoiseLuma    = f;
    else if (key == "localDenoiseChroma")  p.localDenoiseChroma  = f;
    else if (key == "vignetteExposure")    p.vignetteExposure    = f;
    else if (key == "vignetteFeather")     p.vignetteFeather     = f;
    else if (key == "grainAmount")         p.grainAmount         = f;
    else if (key == "grainSize")           p.grainSize           = f;
    else if (key == "grainRoughness")      p.grainRoughness      = f;
    /* Global-only (always scope 0), listed here so one function covers every raw key. */
    else if (key == "denoiseLuma")         p.denoiseLuma         = f;
    else if (key == "denoiseChroma")       p.denoiseChroma       = f;
    else if (key == "toneShadowCenter")    p.toneShadowCenter    = f;
    else if (key == "toneCrossover")       p.toneCrossover       = f;
    else if (key == "toneHighlightCenter") p.toneHighlightCenter = f;
}
