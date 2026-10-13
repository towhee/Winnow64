#include "Develop/Presets/developpresets.h"
#include "Main/global.h"

#include <QSet>

/*
    See developpresets.h. Reads / writes named develop presets in QSettings. Everything
    here is deliberately key-by-key: a preset holds only the settings the user ticked,
    and read() must not invent the rest.
*/

DevelopPresets::DevelopPresets(QSettings *setting, QObject *parent)
    : QObject(parent), setting(setting)
{
    /* The copy buffer is SESSION SCOPED: wipe whatever the previous run left behind, so
       Paste stays unavailable until this session's first Copy. It lives in QSettings only
       because that is where the preset store already reads and writes -- persistence is a
       side effect of sharing the store, not a feature of the clipboard. */
    clearClipboard();
}

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

void DevelopPresets::write(const DevelopPreset &p, const QString &category)
{
    if (!setting || p.name.isEmpty()) return;
    if (G::isLogger) G::log("DevelopPresets::write", p.name);
    writeGroup(kRoot, p);
    /* A named category places the preset at its end -- unless it is already in that
       category, where re-saving it must not shuffle it. Without one, layout() files an
       unplaced preset under Winnow on its own. */
    if (!category.isEmpty()) {
        QVector<PresetCategory> cats = layout();
        int ci = indexOfCategory(cats, category);
        if (ci < 0 || !cats[ci].presets.contains(p.name)) {
            for (PresetCategory &c : cats) c.presets.removeAll(p.name);
            if (ci < 0) {
                cats.append({category, {}, false});
                ci = cats.size() - 1;
            }
            cats[ci].presets.append(p.name);
            saveLayout(cats);
        }
    }
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

void DevelopPresets::clearClipboard()
{
    if (!setting) return;
    if (G::isLogger) G::log("DevelopPresets::clearClipboard");
    /* remove("") inside the group drops the group itself, so hasClipboard() finds no slot
       at all rather than an empty one. No changed() signal -- the Presets list never
       showed the buffer, so there is nothing to reload. */
    setting->beginGroup(kClipRoot);
    setting->remove("");
    setting->endGroup();
    setting->sync();
}

bool DevelopPresets::rename(const QString &from, const QString &to)
{
    if (!setting || from.isEmpty() || to.isEmpty() || from == to) return false;
    if (contains(to)) return false;
    /* Read-then-write is enough: a preset is small and read() captures everything the
       store holds, so there is no need to walk the QSettings tree by hand. */
    DevelopPreset p = read(from);
    if (p.name.isEmpty()) return false;
    /* Keep its place: rename the layout entry in situ, captured BEFORE the old preset
       goes (layout() drops entries for presets that no longer exist). */
    QVector<PresetCategory> cats = layout();
    for (PresetCategory &c : cats)
        for (QString &n : c.presets)
            if (n == from) n = to;
    p.name = to;
    setting->beginGroup(kRoot);
    setting->remove(from);
    setting->endGroup();
    saveLayout(cats);
    write(p);                           // emits changed()
    return true;
}

void DevelopPresets::remove(const QString &name)
{
    if (!setting || name.isEmpty()) return;
    if (G::isLogger) G::log("DevelopPresets::remove", name);
    /* Drop its layout entry too, so a later preset of the same name starts fresh at
       the end of its category instead of reappearing in the deleted one's slot. */
    QVector<PresetCategory> cats = layout();
    for (PresetCategory &c : cats) c.presets.removeAll(name);
    setting->beginGroup(kRoot);
    setting->remove(name);
    setting->endGroup();
    saveLayout(cats);
    setting->sync();
    emit changed();
}

/* ---- Categories -------------------------------------------------------------------- */

int DevelopPresets::indexOfCategory(const QVector<PresetCategory> &cats, const QString &name)
{
    for (int i = 0; i < cats.size(); ++i)
        if (cats[i].name.compare(name, Qt::CaseInsensitive) == 0) return i;
    return -1;
}

QVector<PresetCategory> DevelopPresets::layout() const
{
/*
    The stored layout, reconciled against the presets that actually exist: entries for
    vanished presets and duplicates are dropped, Winnow is guaranteed (first, if it was
    somehow missing), and every preset not placed anywhere -- including all presets saved
    before categories existed -- is appended to Winnow in name order.
*/
    QVector<PresetCategory> cats;
    if (!setting) return cats;
    const QStringList all = names();
    QSet<QString> placed;

    setting->beginGroup(kLayoutRoot);
    const int n = setting->beginReadArray("categories");
    for (int i = 0; i < n; ++i) {
        setting->setArrayIndex(i);
        PresetCategory c;
        c.name = setting->value("name").toString().trimmed();
        c.collapsed = setting->value("collapsed", false).toBool();
        if (c.name.isEmpty() || indexOfCategory(cats, c.name) >= 0) continue;
        /* toStringList: a one-item list may read back as a plain QString on some
           backends, and an empty one as an invalid QVariant -- both convert correctly. */
        const QStringList stored = setting->value("presets").toStringList();
        for (const QString &p : stored) {
            if (p.isEmpty() || placed.contains(p) || !all.contains(p)) continue;
            c.presets.append(p);
            placed.insert(p);
        }
        cats.append(c);
    }
    setting->endArray();
    setting->endGroup();

    int wi = indexOfCategory(cats, kWinnowCategory);
    if (wi < 0) {
        cats.prepend({kWinnowCategory, {}, false});
        wi = 0;
    }
    cats[wi].name = kWinnowCategory;    // canonical spelling
    for (const QString &p : all)        // names() is already sorted
        if (!placed.contains(p)) cats[wi].presets.append(p);
    return cats;
}

void DevelopPresets::saveLayout(const QVector<PresetCategory> &cats)
{
    if (!setting) return;
    setting->beginGroup(kLayoutRoot);
    setting->remove("");
    setting->beginWriteArray("categories", cats.size());
    for (int i = 0; i < cats.size(); ++i) {
        setting->setArrayIndex(i);
        setting->setValue("name", cats[i].name);
        setting->setValue("presets", cats[i].presets);
        setting->setValue("collapsed", cats[i].collapsed);
    }
    setting->endArray();
    setting->endGroup();
    setting->sync();
}

QStringList DevelopPresets::categoryNames() const
{
    QStringList list;
    for (const PresetCategory &c : layout()) list.append(c.name);
    return list;
}

QString DevelopPresets::categoryOf(const QString &preset) const
{
    for (const PresetCategory &c : layout())
        if (c.presets.contains(preset)) return c.name;
    return QString();
}

bool DevelopPresets::hasCategory(const QString &name) const
{
    return indexOfCategory(layout(), name) >= 0;
}

bool DevelopPresets::addCategory(const QString &name)
{
    const QString n = name.trimmed();
    if (!setting || n.isEmpty()) return false;
    if (G::isLogger) G::log("DevelopPresets::addCategory", n);
    QVector<PresetCategory> cats = layout();
    if (indexOfCategory(cats, n) >= 0) return false;
    cats.append({n, {}, false});
    saveLayout(cats);
    emit changed();
    return true;
}

bool DevelopPresets::renameCategory(const QString &from, const QString &to)
{
    const QString n = to.trimmed();
    if (!setting || n.isEmpty()) return false;
    if (from.compare(kWinnowCategory, Qt::CaseInsensitive) == 0) return false;
    if (G::isLogger) G::log("DevelopPresets::renameCategory", from);
    QVector<PresetCategory> cats = layout();
    const int ci = indexOfCategory(cats, from);
    if (ci < 0) return false;
    /* A pure case change of its own name is allowed; any other clash is not. */
    const int clash = indexOfCategory(cats, n);
    if (clash >= 0 && clash != ci) return false;
    cats[ci].name = n;
    saveLayout(cats);
    emit changed();
    return true;
}

void DevelopPresets::removeCategory(const QString &name)
{
    if (!setting || name.compare(kWinnowCategory, Qt::CaseInsensitive) == 0) return;
    if (G::isLogger) G::log("DevelopPresets::removeCategory", name);
    QVector<PresetCategory> cats = layout();
    const int ci = indexOfCategory(cats, name);
    if (ci < 0) return;
    /* Deleting a category never deletes presets: they go to the end of Winnow. */
    const QStringList orphans = cats[ci].presets;
    cats.remove(ci);
    cats[indexOfCategory(cats, kWinnowCategory)].presets.append(orphans);
    saveLayout(cats);
    emit changed();
}

void DevelopPresets::moveCategory(const QString &name, int toIndex)
{
    QVector<PresetCategory> cats = layout();
    const int from = indexOfCategory(cats, name);
    if (from < 0) return;
    toIndex = qBound(0, toIndex, cats.size());
    if (toIndex > from) --toIndex;      // the gap shifts once the item is taken out
    if (toIndex == from) return;
    if (G::isLogger) G::log("DevelopPresets::moveCategory", name);
    cats.move(from, toIndex);
    saveLayout(cats);
    emit changed();
}

void DevelopPresets::movePreset(const QString &preset, const QString &category, int toIndex)
{
    QVector<PresetCategory> cats = layout();
    const int to = indexOfCategory(cats, category);
    if (to < 0) return;
    int from = -1, fromPos = -1;
    for (int i = 0; i < cats.size() && from < 0; ++i) {
        fromPos = cats[i].presets.indexOf(preset);
        if (fromPos >= 0) from = i;
    }
    if (from < 0) return;
    toIndex = qBound(0, toIndex, int(cats[to].presets.size()));
    if (from == to) {
        if (toIndex > fromPos) --toIndex;
        if (toIndex == fromPos) return;
    }
    if (G::isLogger) G::log("DevelopPresets::movePreset", preset);
    cats[from].presets.removeAt(fromPos);
    cats[to].presets.insert(toIndex, preset);
    /* Dropping into a folded category opens it, so the preset is seen to land. */
    cats[to].collapsed = false;
    saveLayout(cats);
    emit changed();
}

void DevelopPresets::setCategoryCollapsed(const QString &name, bool collapsed)
{
    QVector<PresetCategory> cats = layout();
    const int ci = indexOfCategory(cats, name);
    if (ci < 0 || cats[ci].collapsed == collapsed) return;
    cats[ci].collapsed = collapsed;
    saveLayout(cats);
}

void DevelopPresets::assignParam(const QString &key, const QVariant &v, EditParams &p)
{
    /* RAW values under the EditStack JSON field names (see paramsToJson), NOT the dock's
       slider keys -- DevelopProperties::applyKeyToParams is the other direction and
       div-scales (0..100 -> 0..1). Keep the two apart. */
    /* The tone curve is a STRING (ToneCurve's shared encoding), and its two preset keys
       each own only part of the curve set: "curveComposite" carries channel 0, and
       "curveChannels" carries R/G/B. Decode into a scratch and copy just those channels,
       so applying one preset leaf never clears the other. */
    auto assignCurves = [&p](const QString &enc, int from, int to) {
        EditParams t;
        ToneCurve::decode(enc, t.curveN, t.curveX, t.curveY);
        for (int c = from; c <= to; ++c) {
            p.curveN[c] = t.curveN[c];
            for (int k = 0; k < ToneCurve::kMaxPts; ++k) {
                p.curveX[c][k] = t.curveX[c][k];
                p.curveY[c][k] = t.curveY[c][k];
            }
        }
    };
    if (key == "curveComposite") { assignCurves(v.toString(), 0, 0); return; }
    if (key == "curveChannels")  { assignCurves(v.toString(), 1, 3); return; }
    /* Point Color: one string, PointColor's shared encoding; replaces every point. */
    if (key == "pointColors") {
        PointColor::decode(v.toString(), p.pointColorN, p.pointColor);
        return;
    }

    const float f = v.toFloat();
    if      (key == "temp")                p.temp                = f;
    else if (key == "tint")                p.tint                = f;
    else if (key == "wbPreset")            p.wbPreset            = v.toInt();
    else if (key == "viewTransform")       p.viewTransform       = v.toInt();
    /* By NAME, not by path -- see kBasicLeaves. A preset applied to a camera the profile
       was not made for simply does not resolve, and the panel says so. */
    else if (key == "cameraProfile")       p.cameraProfile       = v.toString();
    else if (key == "exposure")            p.exposure            = f;
    else if (key == "contrast")            p.contrast            = f;
    else if (key == "highlights")          p.highlights          = f;
    else if (key == "shadows")             p.shadows             = f;
    else if (key == "whites")              p.whites              = f;
    else if (key == "blacks")              p.blacks              = f;
    else if (key == "texture")             p.texture             = f;
    else if (key == "clarity")             p.clarity             = f;
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
    else if (key == "calShadowTint")       p.calShadowTint       = f;
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
    else if (key == "sharpenAmount")       p.sharpenAmount       = f;
    else if (key == "sharpenRadius")       p.sharpenRadius       = f;
    else if (key == "sharpenDetail")       p.sharpenDetail       = f;
    else if (key == "sharpenMasking")      p.sharpenMasking      = f;
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
    /* Tri-state, so toInt() not toFloat(): -1 unset / 0 off / 1 on. */
    else if (key == "denoiseRaw")          p.denoiseRaw          = v.toInt();
    else if (key == "removeCA")            p.removeCA            = v.toInt();   // tri-state
    else if (key == "defringePurple")      p.defringePurple      = f;
    else if (key == "defringeGreen")       p.defringeGreen       = f;
    else if (key == "lensProfile")         p.lensProfile         = v.toInt();   // tri-state
    else if (key == "lensDistortion")      p.lensDistortion      = f;
    else if (key == "lensVignetting")      p.lensVignetting      = f;
    else if (key == "lensManualDistortion") p.lensManualDistortion = f;
    else if (key == "lensManualVignetting") p.lensManualVignetting = f;
    else if (key == "lensManualMidpoint")   p.lensManualMidpoint   = f;
    /* By lens id; a lens the target's camera cannot take does not resolve, and the
       Lens row says so. */
    else if (key == "lensProfileId")       p.lensProfileId       = v.toString();
    else if (key == "toneShadowCenter")    p.toneShadowCenter    = f;
    else if (key == "toneCrossover")       p.toneCrossover       = f;
    else if (key == "toneHighlightCenter") p.toneHighlightCenter = f;
}
