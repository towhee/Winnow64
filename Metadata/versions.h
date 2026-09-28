#ifndef VERSIONS_H
#define VERSIONS_H

#include <QByteArray>
#include <QDateTime>
#include <QJsonObject>
#include <QList>
#include <QString>
#include <functional>

/*
    VERSIONS (VIRTUAL COPIES) -- STORAGE.

    A version is a second Develop recipe for the same source file, shown as its own
    row in the grid (see "Versions (Virtual Copies)" in notes/Documentation.txt and
    Utilities/versionkey.h for the row key). This file is where versions LIVE: the
    image's XMP sidecar, beside the master's recipe.

    ONE ATTRIBUTE, winnow:Versions, whose value is base64 of compact JSON:

      { "schema": 1, "nextId": 3,
        "versions": [ { "id": 1, "name": "B&W", "created": "2026-09-27T10:00:00Z",
                        "rating": 4, "label": "Red", "pick": "Picked",
                        "develop": "<EditStack::toBase64()>",
                        "previewKey": "<Metadata::devPreviewKey(develop)>",
                        "preview": "<256px JPEG, base64>" }, ... ] }

    THE MASTER IS NOT IN IT. winnow:Develop / DevelopPreview / DevelopPreviewKey stay
    the master's, exactly as before, so a sidecar with versions is still read
    correctly by an older Winnow and by anything else that reads winnow:Develop. An
    older Winnow rewriting the sidecar keeps winnow:Versions: Xmp edits the parsed
    document in place, so attributes it does not know survive.

    IDS ARE NEVER REUSED. nextId only goes up, so a deleted version's id cannot come
    back as a different version -- the id is part of the row key, and every
    key-keyed cache (devPreview, history, icon) would otherwise hand the new version
    the old one's state. So deleting the LAST version does not remove the attribute:
    {"schema":1,"nextId":N,"versions":[]} stays behind as the id high-water mark.

    UNKNOWN FIELDS SURVIVE. A newer build may add fields to a version or to the
    document; they are carried through a read-modify-write untouched (extra), so an
    older build editing a rating does not strip them. A document whose schema is
    NEWER than kSchema is read but never written.

    RATING, LABEL AND PICK are per version and Winnow-private: they always live here,
    whatever "Permit image file modification" says. The master's rating/label stay
    in xmp:Rating / xmp:Label as before.

    THREADING. read/readVersion are safe on any thread. update() holds
    FileOps::sidecarLock for the whole read-modify-write, the same lock every other
    sidecar writer holds.
*/

struct ImageVersion
{
    int id = 0;                 // > 0; 0 means "not a version" (the master)
    QString name;               // user-visible; "" shows as "v<id>"
    QDateTime created;          // UTC
    int rating = 0;             // 0 = unrated, 1..5
    QString label;              // "" or a colour name, as G::LabelColumn holds it
    QString pick;               // "", "Picked" or "Rejected", as G::PickColumn holds it
    QString develop;            // EditStack::toBase64(); "" = identity
    QString previewKey;         // devPreviewKey of the recipe the preview shows
    QByteArray preview;         // 256px JPEG as base64 text; empty unless asked for
    QJsonObject extra;          // fields this build does not know, carried through

    QString displayName() const
    {
        return name.isEmpty() ? QStringLiteral("v%1").arg(id) : name;
    }
    bool isDeveloped() const { return !develop.isEmpty(); }
};

/* What the folder-load pass keeps per version (ImageMetadata::versions): enough to
   make the row and its badge, none of the recipe or preview bytes. */
struct VersionSummary
{
    int id = 0;
    QString name;
    int rating = 0;
    QString label;
    QString pick;
    bool developed = false;
    QString devPreviewKey;      // key of the RECIPE, like ImageMetadata::devPreviewKey
    /*  The version's develop geometry as output/source per axis, like
        ImageMetadata::cropFx/cropFy. Filled by Metadata::parseSidecar, which has the
        image's dimensions; summaries() leaves 1,1. */
    float cropFx = 1.0f, cropFy = 1.0f;
};

struct VersionSet
{
    static constexpr int kSchema = 1;

    enum class Detail { Recipe, Full };     // Full also keeps the preview bytes
    enum class Status { Ok, Absent, Corrupt, Newer };

    int schema = kSchema;
    int nextId = 1;
    QList<ImageVersion> versions;
    QJsonObject extra;          // document-level fields this build does not know

    bool isEmpty() const { return versions.isEmpty(); }
    int indexOf(int id) const;
    const ImageVersion *find(int id) const;
    ImageVersion *find(int id);
    /* Appends v with a fresh id (nextId++) and a creation time if it has none.
       Returns the stored copy. */
    ImageVersion &add(ImageVersion v);
    bool remove(int id);

    QString toBase64() const;
    /* "" -> Absent (an empty set). Corrupt leaves an empty set; a caller that would
       WRITE must refuse on anything but Ok/Absent, or it destroys the user's
       versions. */
    static VersionSet fromBase64(const QString &b64, Detail detail = Detail::Full,
                                 Status *status = nullptr);
};

namespace Versions {

/* The versions in src's sidecar ("" / absent sidecar -> empty set). src must be a
   SOURCE path, never a version key. */
VersionSet read(const QString &src,
                VersionSet::Detail detail = VersionSet::Detail::Recipe,
                VersionSet::Status *status = nullptr);

/* One version, with its preview. Returns a version with id 0 when absent. */
ImageVersion readVersion(const QString &src, int id);

/* The version's recipe (EditStack base64), "" when absent or identity. */
QString readRecipe(const QString &src, int id);

/* Read-modify-write of src's winnow:Versions under FileOps::sidecarLock. edit gets
   the full set (previews included) and may change it freely; ids are preserved and
   nextId must not go down (enforced). A set that never issued an id removes the
   attribute; one that did keeps its nextId even when empty. Returns
   false -- having written nothing -- when the stored set is corrupt or from a newer
   schema, the sidecar cannot be written (symlink, read-only volume, preview cache
   folder, a version key passed as src), or the write fails. Each is issued. */
bool update(const QString &src, const std::function<void(VersionSet &)> &edit);

/* The folder-load view of a set. keyOf maps a
   recipe to its devPreviewKey -- Metadata::devPreviewKey, passed in so this module
   does not link the Metadata class. */
QList<VersionSummary> summaries(const VersionSet &set,
                                const std::function<QString(const QString &)> &keyOf);

} // namespace Versions

#endif // VERSIONS_H
