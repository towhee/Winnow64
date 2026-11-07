#include "Develop/Presets/developpresets.h"
#include "Main/global.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QStandardPaths>

/*
    See developpresets.h. Reads / writes named develop presets in QSettings. Everything
    here is deliberately key-by-key: a preset holds only the settings the user ticked,
    and read() must not invent the rest. The reserved Winnow presets are the exception:
    they live in winnowpresets.json, held in memory (winnow) and written back only for
    G::isRory.
*/

namespace {
/* Same recipe, compared as strings: a QSettings value reads back as a QString and a
   JSON one as a double, and both print the same shortest form. */
bool sameHash(const QVariantHash &a, const QVariantHash &b)
{
    if (a.size() != b.size()) return false;
    for (auto it = a.constBegin(); it != a.constEnd(); ++it)
        if (!b.contains(it.key()) || b.value(it.key()).toString() != it.value().toString())
            return false;
    return true;
}

bool sameRecipe(const DevelopPreset &a, const DevelopPreset &b)
{
    if (!sameHash(a.globals, b.globals) || !sameHash(a.params, b.params)) return false;
    if (a.spots.size() != b.spots.size()) return false;
    for (int i = 0; i < a.spots.size(); ++i)
        if (a.spots[i].paramsJson != b.spots[i].paramsJson ||
            a.spots[i].enabled != b.spots[i].enabled) return false;
    return true;
}
}

DevelopPresets::DevelopPresets(QSettings *setting, QObject *parent)
    : QObject(parent), setting(setting)
{
    loadWinnow();
    reconcileWithWinnow();
    /* The copy buffer is SESSION SCOPED: wipe whatever the previous run left behind, so
       Paste stays unavailable until this session's first Copy. It lives in QSettings only
       because that is where the preset store already reads and writes -- persistence is a
       side effect of sharing the store, not a feature of the clipboard. */
    clearClipboard();
}

QStringList DevelopPresets::userNames() const
{
    if (!setting) return QStringList();
    setting->beginGroup(kRoot);
    QStringList list = setting->childGroups();
    setting->endGroup();
    list.sort(Qt::CaseInsensitive);
    return list;
}

QStringList DevelopPresets::names() const
{
    QStringList list = userNames();
    for (const DevelopPreset &p : winnow) list.append(p.name);
    list.sort(Qt::CaseInsensitive);
    return list;
}

bool DevelopPresets::contains(const QString &name) const
{
    return names().contains(name, Qt::CaseInsensitive);
}

DevelopPreset DevelopPresets::read(const QString &name) const
{
    const int wi = indexOfWinnow(name);
    if (wi >= 0) return winnow.at(wi);
    return readGroup(kRoot, name);
}

/* ---- The reserved Winnow presets ---------------------------------------------------- */

bool DevelopPresets::canEditWinnow()
{
    return G::isRory;
}

bool DevelopPresets::isWinnowCategory(const QString &category)
{
    return category.compare(kWinnowCategory, Qt::CaseInsensitive) == 0;
}

int DevelopPresets::indexOfWinnow(const QString &name) const
{
    for (int i = 0; i < winnow.size(); ++i)
        if (winnow[i].name.compare(name, Qt::CaseInsensitive) == 0) return i;
    return -1;
}

bool DevelopPresets::isWinnowPreset(const QString &name) const
{
    return indexOfWinnow(name) >= 0;
}

void DevelopPresets::loadWinnow()
{
/*
    Read the shipped presets from the resource, in file order (= display order). The
    file holds the same groups as QSettings: "Global" (+ "spots") and "Edits". A build
    without the resource (the unit tests) simply has no Winnow presets.
*/
    winnow.clear();
    QFile f(kWinnowResource);
    if (!f.open(QIODevice::ReadOnly)) return;
    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        G::issue("Warning", "Could not parse " + QString(kWinnowResource) + ": " +
                 err.errorString(), "DevelopPresets::loadWinnow");
        return;
    }
    const QJsonArray arr = doc.object().value("presets").toArray();
    for (const QJsonValue &v : arr) {
        const QJsonObject o = v.toObject();
        DevelopPreset p;
        p.name = o.value("name").toString().trimmed();
        if (p.name.isEmpty() || indexOfWinnow(p.name) >= 0) continue;
        /* v1 by definition when absent, exactly as readGroup. */
        p.version     = o.value("version").toInt(1);
        p.sourceScope = o.value("sourceScope").toString();
        p.sourceImage = o.value("sourceImage").toString();
        p.globals     = o.value("Global").toObject().toVariantHash();
        p.params      = o.value(kEdits).toObject().toVariantHash();
        const QJsonArray spots = o.value("spots").toArray();
        for (const QJsonValue &sv : spots) {
            FillSpot spot;
            spot.paramsJson = sv.toObject().value("params").toString();
            spot.enabled    = sv.toObject().value("enabled").toBool(true);
            p.spots.append(spot);
        }
        if (p.version < DevelopSchema::current()) migrate(p);
        winnow.append(p);
    }
}

void DevelopPresets::saveWinnow()
{
/*
    Rory only. Write the Winnow presets to Develop/Presets/winnowpresets.json in the
    source tree (WINNOW_SOURCE_DIR, defined by CMake), so the file compiled into the
    resource -- and so into the release -- is the file that changes. Without a source
    tree the file goes to the desktop and the popup says where. The same arrangement as
    MW::writeWorkflowDefaultsJson.
*/
    if (!canEditWinnow()) return;
    QString dir;
    #ifdef WINNOW_SOURCE_DIR
    if (QFileInfo::exists(QString(WINNOW_SOURCE_DIR) + "/winnow.qrc"))
        dir = QString(WINNOW_SOURCE_DIR) + "/Develop/Presets";
    #endif
    const bool inSource = !dir.isEmpty();
    if (!inSource) dir = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
    QDir().mkpath(dir);
    const QString path = dir + "/" + kWinnowFile;

    QJsonArray arr;
    for (const DevelopPreset &p : winnow) {
        QJsonObject o;
        o.insert("name", p.name);
        o.insert("version", DevelopSchema::current());   // stored current-shaped
        if (!p.sourceScope.isEmpty()) o.insert("sourceScope", p.sourceScope);
        if (!p.sourceImage.isEmpty()) o.insert("sourceImage", p.sourceImage);
        if (!p.globals.isEmpty()) o.insert("Global", QJsonObject::fromVariantHash(p.globals));
        if (!p.params.isEmpty())  o.insert(kEdits, QJsonObject::fromVariantHash(p.params));
        if (!p.spots.isEmpty()) {
            QJsonArray spots;
            for (const FillSpot &spot : p.spots)
                spots.append(QJsonObject{{"params", spot.paramsJson},
                                         {"enabled", spot.enabled}});
            o.insert("spots", spots);
        }
        arr.append(o);
    }
    QJsonObject root;
    root.insert("presets", arr);

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (G::popup) G::popup->showPopup("Could not write the Winnow presets: " +
                                          f.errorString() + " (" + path + ")", 4000);
        return;
    }
    f.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    f.close();
    if (G::isLogger) G::log("DevelopPresets::saveWinnow", path);
    if (!inSource && G::popup)
        G::popup->showPopup("No source tree: the Winnow presets were written to " + path +
                            ". Copy it to Develop/Presets to ship it.", 5000);
}

void DevelopPresets::reconcileWithWinnow()
{
/*
    One-time cleanup of a store written by a build where Winnow was the catch-all
    category (and Rory's own store, where the shipped presets were first authored):

    - a user preset with a shipped preset's name is dropped if it is the same recipe,
      else renamed "<name> (user)" in its slot, so nothing the user made is lost and
      the shipped preset is never shadowed;
    - user presets the stored layout files under Winnow move to User Presets.

    Both rewrite the layout once; on a clean store this only reads.
*/
    if (!setting || winnow.isEmpty()) return;
    bool dirty = false;

    setting->beginGroup(kLayoutRoot);
    const int n = setting->beginReadArray("categories");
    for (int i = 0; i < n && !dirty; ++i) {
        setting->setArrayIndex(i);
        if (isWinnowCategory(setting->value("name").toString().trimmed()) &&
            !setting->value("presets").toStringList().isEmpty()) dirty = true;
    }
    setting->endArray();
    setting->endGroup();

    QVector<PresetCategory> cats = layout();
    const QStringList users = userNames();
    for (const QString &name : users) {
        const int wi = indexOfWinnow(name);
        if (wi < 0) continue;
        dirty = true;
        DevelopPreset u = readGroup(kRoot, name);
        removeUser(name);
        if (sameRecipe(u, winnow.at(wi))) continue;
        QString to = name + " (user)";
        for (int k = 2; contains(to); ++k) to = name + QString(" (user %1)").arg(k);
        if (G::isLogger) G::log("DevelopPresets::reconcileWithWinnow", name + " -> " + to);
        u.name = to;
        writeGroup(kRoot, u);
        for (PresetCategory &c : cats)
            for (QString &p : c.presets)
                if (p == name) p = to;
    }
    if (dirty) saveLayout(cats);
}

void DevelopPresets::removeUser(const QString &name)
{
    if (!setting) return;
    setting->beginGroup(kRoot);
    setting->remove(name);
    setting->endGroup();
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

    /* Convert anything older than the current schema BEFORE the values reach
       assignParam. */
    if (p.version < DevelopSchema::current()) migrate(p);
    return p;
}

void DevelopPresets::migrate(DevelopPreset &p)
{
    /* The steps live in Develop/developschema.h, shared with the sidecar. Both hashes
       hold EditStack JSON field names -> raw values, so they round-trip through a
       QJsonObject. A preset from a NEWER build (version > current) is left alone:
       assignParam already ignores keys it does not know, which degrades better than
       guessing backwards. */
    const int cur = DevelopSchema::current();
    if (p.version >= cur) return;
    /* Only keys a step actually changed are written back: QSettings hands values back
       as strings, and a full JSON round trip would retype every untouched one. */
    auto run = [&p](QVariantHash &h) {
        QJsonObject o = QJsonObject::fromVariantHash(h);
        const QJsonObject before = o;
        DevelopSchema::migrateParams(o, p.version, true);
        for (const QString &k : before.keys())
            if (!o.contains(k)) h.remove(k);
        for (auto it = o.constBegin(); it != o.constEnd(); ++it)
            if (!before.contains(it.key()) || before.value(it.key()) != it.value())
                h.insert(it.key(), it.value().toVariant());
    };
    run(p.params);
    run(p.globals);
    p.version = cur;
}

bool DevelopPresets::write(const DevelopPreset &p, const QString &category)
{
    if (!setting || p.name.isEmpty()) return false;
    /* Winnow: into it (named, or an existing Winnow preset re-saved in place) or out
       of it (a Winnow preset's name saved to another category). Rory only. */
    const int wi = indexOfWinnow(p.name);
    const bool toWinnow = isWinnowCategory(category) || (category.isEmpty() && wi >= 0);
    if ((toWinnow || wi >= 0) && !canEditWinnow()) return false;
    if (G::isLogger) G::log("DevelopPresets::write", p.name);
    if (toWinnow) {
        DevelopPreset q = p;
        q.version = DevelopSchema::current();
        if (wi >= 0) winnow[wi] = q;
        else {
            removeUser(p.name);         // a user preset of that name is replaced
            winnow.append(q);
        }
        saveWinnow();
        setting->sync();
        emit changed();
        return true;
    }
    if (wi >= 0) {
        winnow.removeAt(wi);
        saveWinnow();
    }
    writeGroup(kRoot, p);
    /* A named category places the preset at its end -- unless it is already in that
       category, where re-saving it must not shuffle it. Without one, layout() files an
       unplaced preset under User Presets on its own. */
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
    return true;
}

void DevelopPresets::writeGroup(const char *root, const DevelopPreset &p)
{
    if (!setting || p.name.isEmpty()) return;

    setting->beginGroup(root);
    setting->beginGroup(p.name);
    setting->remove("");                // overwrite: clear any prior contents
    /* Always stamp the CURRENT schema, whatever version p arrived as: what is about to
       be written is by definition current-shaped (read() has already migrated). */
    setting->setValue("version", DevelopSchema::current());
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
    const int wi = indexOfWinnow(from);
    if (wi >= 0) {
        if (!canEditWinnow()) return false;
        if (G::isLogger) G::log("DevelopPresets::rename", from);
        winnow[wi].name = to;
        saveWinnow();
        emit changed();
        return true;
    }
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
    const int wi = indexOfWinnow(name);
    if (wi >= 0) {
        if (!canEditWinnow()) return;
        if (G::isLogger) G::log("DevelopPresets::remove", name);
        winnow.removeAt(wi);
        saveWinnow();
        emit changed();
        return;
    }
    if (G::isLogger) G::log("DevelopPresets::remove", name);
    /* Drop its layout entry too, so a later preset of the same name starts fresh at
       the end of its category instead of reappearing in the deleted one's slot. */
    QVector<PresetCategory> cats = layout();
    for (PresetCategory &c : cats) c.presets.removeAll(name);
    removeUser(name);
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
    vanished presets and duplicates are dropped, Winnow and User Presets are guaranteed
    (first and second, if they were somehow missing), Winnow holds exactly the shipped
    presets in file order, and every user preset not placed anywhere -- including all
    presets saved before categories existed, and any the stored layout still files under
    Winnow -- is appended to User Presets in name order.
*/
    QVector<PresetCategory> cats;
    if (!setting) return cats;
    const QStringList all = userNames();
    QSet<QString> placed;

    setting->beginGroup(kLayoutRoot);
    const int n = setting->beginReadArray("categories");
    for (int i = 0; i < n; ++i) {
        setting->setArrayIndex(i);
        PresetCategory c;
        c.name = setting->value("name").toString().trimmed();
        c.collapsed = setting->value("collapsed", false).toBool();
        if (c.name.isEmpty() || indexOfCategory(cats, c.name) >= 0) continue;
        if (isWinnowCategory(c.name)) {     // its presets come from winnowpresets.json
            cats.append(c);
            continue;
        }
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
    for (const DevelopPreset &p : winnow) cats[wi].presets.append(p.name);

    int ui = indexOfCategory(cats, kUserCategory);
    if (ui < 0) {
        cats.insert(wi + 1, {kUserCategory, {}, false});
        ui = wi + 1;
    }
    cats[ui].name = kUserCategory;      // canonical spelling
    for (const QString &p : all)        // userNames() is already sorted
        if (!placed.contains(p)) cats[ui].presets.append(p);
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
        setting->setValue("presets", isWinnowCategory(cats[i].name) ? QStringList()
                                                                     : cats[i].presets);
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
    if (isWinnowCategory(from) ||
        from.compare(kUserCategory, Qt::CaseInsensitive) == 0) return false;
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
    if (!setting || isWinnowCategory(name) ||
        name.compare(kUserCategory, Qt::CaseInsensitive) == 0) return;
    if (G::isLogger) G::log("DevelopPresets::removeCategory", name);
    QVector<PresetCategory> cats = layout();
    const int ci = indexOfCategory(cats, name);
    if (ci < 0) return;
    /* Deleting a category never deletes presets: they go to the end of User Presets. */
    const QStringList orphans = cats[ci].presets;
    cats.remove(ci);
    cats[indexOfCategory(cats, kUserCategory)].presets.append(orphans);
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
    const bool fromWinnow = isWinnowCategory(cats[from].name);
    const bool toWinnow   = isWinnowCategory(cats[to].name);
    if ((fromWinnow || toWinnow) && !canEditWinnow()) return;
    toIndex = qBound(0, toIndex, int(cats[to].presets.size()));
    if (from == to) {
        if (toIndex > fromPos) --toIndex;
        if (toIndex == fromPos) return;
    }
    if (G::isLogger) G::log("DevelopPresets::movePreset", preset);
    /* A preset crossing Winnow's edge changes store: winnowpresets.json <-> QSettings.
       Its position in Winnow's list IS its index in `winnow` (layout() builds it so). */
    if (fromWinnow || toWinnow) {
        const DevelopPreset p = fromWinnow ? winnow.at(fromPos) : readGroup(kRoot, preset);
        if (fromWinnow) winnow.removeAt(fromPos);
        else            removeUser(preset);
        if (toWinnow)   winnow.insert(toIndex, p);
        else            writeGroup(kRoot, p);
        saveWinnow();
    }
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
    /* Color Mixer: "mix<Band><Kind>", raw like every other slider here. */
    if (key.startsWith(QLatin1String("mix"))) {
        for (int k = 0; k < ColorMixer::kKinds; ++k)
            for (int b = 0; b < ColorMixer::kBands; ++b)
                if (key == ColorMixer::key(k, b)) {
                    p.*EditParams::mixerMember(k, b) = f;
                    return;
                }
    }
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
    else if (key == "kaleidoSegments")     p.kaleidoSegments     = v.toInt();
    else if (key == "kaleidoX")            p.kaleidoX            = f;
    else if (key == "kaleidoY")            p.kaleidoY            = f;
    else if (key == "kaleidoAngle")        p.kaleidoAngle        = f;
    else if (key == "kaleidoSource")       p.kaleidoSource       = f;
    else if (key == "kaleidoZoom")         p.kaleidoZoom         = f;
    else if (key == "kaleidoOpacity")      p.kaleidoOpacity      = f;
    else if (key == "kaleidoType")         p.kaleidoType         = v.toInt();
    else if (key == "kaleidoVertex")       p.kaleidoVertex       = v.toInt();
    else if (key == "kaleidoRadius")       p.kaleidoRadius       = f;
    else if (key == "kaleidoInvert")       p.kaleidoInvert       = v.toInt();
    else if (key == "kaleidoWrap")         p.kaleidoWrap         = v.toInt();
    else if (key == "kaleidoNest")         p.kaleidoNest         = f;
    else if (key == "kaleidoGroup")        p.kaleidoGroup        = v.toInt();
    else if (key == "kaleidoTile")         p.kaleidoTile         = f;
    else if (key == "kaleidoFlute")        p.kaleidoFlute        = f;
    else if (key == "kaleidoBend")         p.kaleidoBend         = f;
    else if (key == "kaleidoShade")        p.kaleidoShade        = f;
    else if (key == "blurType")            p.blurType            = v.toInt();
    else if (key == "blurAmount")          p.blurAmount          = f;
    else if (key == "blurAngle")           p.blurAngle           = f;
    else if (key == "blurX")               p.blurX               = f;
    else if (key == "blurY")               p.blurY               = f;
    else if (key == "blurSize")            p.blurSize            = f;
    else if (key == "blurFeather")         p.blurFeather         = f;
    else if (key == "ortonOpacity")        p.ortonOpacity        = f;
    else if (key == "ortonSize")           p.ortonSize           = f;
    else if (key == "ortonBrightness")     p.ortonBrightness     = f;
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
    else if (key == "lensBlurAmount")      p.lensBlurAmount      = f;
    else if (key == "lensBlurFocus")       p.lensBlurFocus       = f;
    else if (key == "lensBlurRange")       p.lensBlurRange       = f;
    else if (key == "lensBlurShape")       p.lensBlurShape       = v.toInt();
    else if (key == "lensBlurBoost")       p.lensBlurBoost       = f;
    /* Per scope (a mask scope's Lens Blur refine), unlike the five above. */
    else if (key == "lensBlurRefine")      p.lensBlurRefine      = f;
    /* By lens id; a lens the target's camera cannot take does not resolve, and the
       Lens row says so. */
    else if (key == "lensProfileId")       p.lensProfileId       = v.toString();
    else if (key == "toneShadowCenter")    p.toneShadowCenter    = f;
    else if (key == "toneCrossover")       p.toneCrossover       = f;
    else if (key == "toneHighlightCenter") p.toneHighlightCenter = f;
}
