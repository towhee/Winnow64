#include "Metadata/versions.h"
#include "Metadata/xmp.h"
#include "Cache/devpreviewcache.h"
#include "Utilities/fileops.h"
#include "Utilities/utilities.h"
#include "Main/global.h"

#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QMutexLocker>

/* ---------------------------------------------------------------------------------
   VersionSet
   --------------------------------------------------------------------------------- */

namespace {

// the keys this build owns; everything else in an object is carried in extra
const QStringList &versionKeys()
{
    static const QStringList k = {"id", "name", "created", "rating", "label", "pick",
                                  "develop", "previewKey", "preview"};
    return k;
}

const QStringList &documentKeys()
{
    static const QStringList k = {"schema", "nextId", "versions"};
    return k;
}

QJsonObject versionToJson(const ImageVersion &v)
{
    QJsonObject o = v.extra;
    o["id"] = v.id;
    if (!v.name.isEmpty()) o["name"] = v.name;
    if (v.created.isValid())
        o["created"] = v.created.toUTC().toString(Qt::ISODate);
    if (v.rating) o["rating"] = v.rating;
    if (!v.label.isEmpty()) o["label"] = v.label;
    if (!v.pick.isEmpty()) o["pick"] = v.pick;
    if (!v.develop.isEmpty()) o["develop"] = v.develop;
    /* A preview without its recipe cannot be trusted, and a key without a preview
       names nothing -- the same rule writeDevelopSidecar keeps for the master. */
    if (!v.develop.isEmpty() && !v.preview.isEmpty()) {
        o["previewKey"] = v.previewKey;
        o["preview"] = QString::fromLatin1(v.preview);
    }
    return o;
}

ImageVersion versionFromJson(const QJsonObject &o, VersionSet::Detail detail)
{
    ImageVersion v;
    v.id = o.value("id").toInt();
    v.name = o.value("name").toString();
    v.created = QDateTime::fromString(o.value("created").toString(), Qt::ISODate);
    v.rating = qBound(0, o.value("rating").toInt(), 5);
    v.label = o.value("label").toString();
    v.pick = o.value("pick").toString();
    v.previewKey = o.value("previewKey").toString();
    v.develop = o.value("develop").toString();
    if (detail == VersionSet::Detail::Full)
        v.preview = o.value("preview").toString().toLatin1();
    for (auto it = o.begin(); it != o.end(); ++it)
        if (!versionKeys().contains(it.key())) v.extra.insert(it.key(), it.value());
    return v;
}

} // namespace

int VersionSet::indexOf(int id) const
{
    for (int i = 0; i < versions.size(); ++i)
        if (versions.at(i).id == id) return i;
    return -1;
}

const ImageVersion *VersionSet::find(int id) const
{
    const int i = indexOf(id);
    return i < 0 ? nullptr : &versions.at(i);
}

ImageVersion *VersionSet::find(int id)
{
    const int i = indexOf(id);
    return i < 0 ? nullptr : &versions[i];
}

ImageVersion &VersionSet::add(ImageVersion v)
{
    v.id = nextId++;
    if (!v.created.isValid()) v.created = QDateTime::currentDateTimeUtc();
    versions.append(v);
    return versions.last();
}

bool VersionSet::remove(int id)
{
    const int i = indexOf(id);
    if (i < 0) return false;
    versions.removeAt(i);
    return true;                // nextId is left alone: ids are never reused
}

QString VersionSet::toBase64() const
{
    /* A set that never issued an id is nothing and removes the attribute. One that
       did is kept even when its last version is deleted -- {"nextId":N,"versions":[]}
       -- or the next version created would take id 1 again and inherit whatever the
       key-keyed caches (devPreview, history, icon) still hold for the deleted one. */
    if (versions.isEmpty() && nextId <= 1 && extra.isEmpty()) return QString();
    QJsonObject doc = extra;
    doc["schema"] = schema;
    doc["nextId"] = nextId;
    QJsonArray arr;
    for (const ImageVersion &v : versions) arr.append(versionToJson(v));
    doc["versions"] = arr;
    return QString::fromLatin1(
        QJsonDocument(doc).toJson(QJsonDocument::Compact).toBase64());
}

VersionSet VersionSet::fromBase64(const QString &b64, Detail detail, Status *status)
{
    VersionSet set;
    auto result = [&](Status s) { if (status) *status = s; };
    if (b64.trimmed().isEmpty()) { result(Status::Absent); return set; }

    const QByteArray raw = QByteArray::fromBase64(b64.toLatin1(),
                                                  QByteArray::AbortOnBase64DecodingErrors);
    QJsonParseError err;
    const QJsonDocument jd = QJsonDocument::fromJson(raw, &err);
    if (raw.isEmpty() || err.error != QJsonParseError::NoError || !jd.isObject()) {
        result(Status::Corrupt);
        return set;
    }
    const QJsonObject doc = jd.object();
    set.schema = doc.value("schema").toInt(kSchema);
    for (const QJsonValue &jv : doc.value("versions").toArray()) {
        const ImageVersion v = versionFromJson(jv.toObject(), detail);
        if (v.id <= 0 || set.indexOf(v.id) >= 0) continue;      // unusable or duplicate
        set.versions.append(v);
    }
    // nextId never falls to or below an id in use, whatever the document says
    int maxId = 0;
    for (const ImageVersion &v : std::as_const(set.versions)) maxId = qMax(maxId, v.id);
    set.nextId = qMax(doc.value("nextId").toInt(1), maxId + 1);
    for (auto it = doc.begin(); it != doc.end(); ++it)
        if (!documentKeys().contains(it.key())) set.extra.insert(it.key(), it.value());

    result(set.schema > kSchema ? Status::Newer : Status::Ok);
    return set;
}

/* ---------------------------------------------------------------------------------
   Versions -- the sidecar
   --------------------------------------------------------------------------------- */

namespace {

QString readAttribute(const QString &src)
{
    const QString sidecarPath = FileOps::existingSidecar(src);
    if (sidecarPath.isEmpty()) return QString();
    QFile f(sidecarPath);
    if (!f.open(QIODevice::ReadOnly)) return QString();
    Xmp xmp(f, G::dmInstance);
    const QString b64 = xmp.isValid ? xmp.getItem("versions") : QString();
    f.close();
    return b64;
}

} // namespace

VersionSet Versions::read(const QString &src, VersionSet::Detail detail,
                          VersionSet::Status *status)
{
    if (FileOps::refuseVersionKey(src, "Versions::read")) {
        if (status) *status = VersionSet::Status::Absent;
        return VersionSet();
    }
    return VersionSet::fromBase64(readAttribute(src), detail, status);
}

ImageVersion Versions::readVersion(const QString &src, int id)
{
    const VersionSet set = read(src, VersionSet::Detail::Full);
    const ImageVersion *v = set.find(id);
    return v ? *v : ImageVersion();
}

QString Versions::readRecipe(const QString &src, int id)
{
    const VersionSet set = read(src, VersionSet::Detail::Recipe);
    const ImageVersion *v = set.find(id);
    return v ? v->develop : QString();
}

bool Versions::update(const QString &src, const std::function<void(VersionSet &)> &edit)
{
    const QString fun = "Versions::update";
    if (G::isLogger) G::log(fun, src);
    if (src.isEmpty() || FileOps::refuseVersionKey(src, fun)) return false;
    if (DevPreviewCache::instance().isCachePath(src)) {
        G::issue("Warning", "Refusing to write sidecar: "
                 + DevPreviewCache::readOnlyReason() + ".", fun, -1, src);
        return false;
    }
    QMutexLocker sidecarLocker(&FileOps::sidecarLock(src));

    const QString sidecarPath = FileOps::sidecarPath(src);
    if (QFileInfo(sidecarPath).isSymLink()) {
        G::issue("Warning", "Refusing to write sidecar: path is a symlink.",
                 fun, -1, sidecarPath);
        return false;
    }
    if (!Utilities::folderIsWritable(QFileInfo(src).absolutePath())) {
        G::issue("Warning", "Cannot save versions: the folder is read-only.",
                 fun, -1, src);
        return false;
    }

    const QString writePath = FileOps::prepareSidecarForWrite(src);
    QFile f(writePath);
    if (!f.open(QIODevice::ReadWrite)) {
        G::issue("Warning", "Failed to open sidecar to write versions.", fun, -1, writePath);
        return false;
    }
    Xmp xmp(f, G::dmInstance);
    if (!xmp.isValid) xmp.fix();

    VersionSet::Status status;
    const QString before = xmp.isValid ? xmp.getItem("versions") : QString();
    VersionSet set = VersionSet::fromBase64(before, VersionSet::Detail::Full, &status);
    if (status == VersionSet::Status::Corrupt) {
        G::issue("Warning", "The versions stored in this sidecar are unreadable; "
                 "not overwriting them.", fun, -1, writePath);
        return false;
    }
    if (status == VersionSet::Status::Newer) {
        G::issue("Warning", "The versions in this sidecar were saved by a newer "
                 "Winnow; not overwriting them.", fun, -1, writePath);
        return false;
    }

    const int nextIdBefore = set.nextId;
    edit(set);
    set.nextId = qMax(set.nextId, nextIdBefore);        // ids are never reused
    for (const ImageVersion &v : std::as_const(set.versions))
        set.nextId = qMax(set.nextId, v.id + 1);

    const QString after = set.toBase64();
    if (after == before) return true;                   // nothing changed
    xmp.setItem("versions", after.toLatin1());          // "" removes the attribute

    const QString modifyDate = QDateTime::currentDateTime().toOffsetFromUtc
        (QDateTime::currentDateTime().offsetFromUtc()).toString(Qt::ISODate);
    xmp.setItem("modifydate", modifyDate.toLatin1());
    const bool ok = xmp.writeSidecar(f);
    f.close();
    if (!ok) {
        G::issue("Warning", "Failed to write versions to sidecar.", fun, -1, writePath);
    }
    return ok;
}

QList<VersionSummary> Versions::summaries(
    const VersionSet &set, const std::function<QString(const QString &)> &keyOf)
{
    QList<VersionSummary> out;
    out.reserve(set.versions.size());
    for (const ImageVersion &v : set.versions) {
        VersionSummary s;
        s.id = v.id;
        s.name = v.name;
        s.rating = v.rating;
        s.label = v.label;
        s.pick = v.pick;
        s.developed = v.isDeveloped();
        /* Keyed on the RECIPE, like the master's devPreviewKey, so it means "what this
           version should look like" rather than "what was last rendered". */
        if (s.developed && keyOf) s.devPreviewKey = keyOf(v.develop);
        out.append(s);
    }
    return out;
}
