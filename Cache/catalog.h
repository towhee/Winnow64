#ifndef CATALOG_H
#define CATALOG_H

#include <QDateTime>
#include <QHash>
#include <QList>
#include <QMutex>
#include <QSet>
#include <QSqlDatabase>
#include <QString>
#include <QStringList>
#include <QVector>

/*
    Winnow's catalog: what the app has learned about images it has seen, kept so that a
    keyword or title search can answer ACROSS FOLDERS instead of only inside the one
    currently loaded. See notes/Documentation.txt "Keywords and Cataloguing".

    WHY IT EXISTS

    Winnow deliberately kept no catalogue -- "only the images in the datamodel exist as
    far as the app is concerned". That is the right call for EDITS, and it stays: the XMP
    sidecar remains the per-image database and the only source of truth. But it meant
    everything read during a folder load -- keywords, title, creator, camera, date --
    evaporated the moment the user navigated away, so "where are my heron photos?" had no
    answer short of reopening every folder by hand.

    THIS IS AN INDEX, NOT A LIBRARY. Every column is re-readable from the image and its
    sidecar. Losing this file costs a rescan, never data, which is what lets it live in
    the same move-it-aside-and-rebuild database as the preview cache (Cache/cachedb.h).
    Nothing is written back to anyone's photographs from here.

    The one piece of state that is NOT derived -- the folders the user nominated for
    background scanning -- deliberately lives in QSettings instead, because
    CacheDb::moveAside discards this file without asking and user intent is not
    rebuildable.

    HOW ROWS GET HERE

    Two ways, and both go through commit():

      1. OPPORTUNISTICALLY, from MW::folderChangeCompleted: every folder the user opens is
         committed once its metadata has finished loading. This is deferred to there, and
         run off the GUI thread, for the same reasons the devPreview sweep is -- the load
         the user is waiting on must not carry it.
      2. From the background scanner over the user's designated roots.

    Commit is NOT called per image from addMetadataForItem. That fires once per image on a
    Reader thread whose backpressure is already tuned, and adding a database write there
    is the per-row signal fan-out that has twice been the cause of folder-load lag.

    FRESHNESS is (srcsize, srcmtime, sidecarmtime). The sidecar stamp is the load-bearing
    one for a raw library: Lightroom edits keywords by rewriting the .xmp and never
    touches the NEF, so a check that looked only at the image would never notice.
    staleOf() answers "which of these do I actually need to re-read", so revisiting an
    unchanged folder costs one stat per file and no parsing at all.

    KEYWORDS ARE NORMALISED into keyword + image_keyword rather than stored as text on the
    image row, because the facet list the UI wants ("show me every keyword, with counts")
    is then an index scan instead of a quarter of a million string splits. Hierarchical
    keywords (lr:hierarchicalSubject, ie "Location|Canada|BC") insert one row per ANCESTOR
    as well, wired by parent, which is what lets a search for a parent keyword reach an
    image tagged only with the leaf.

    THREADING. Commit and Sweep run on pool threads; Search runs on the GUI thread while
    the user types. Each thread has its own connection (CacheDb) and WAL means a search
    never blocks a commit. The mutex here guards only this class's own in-memory state --
    the keyword id cache and the loaded flag -- and is never held across a long pass:
    Sweep works in pages and takes it per page, exactly as DevPreviewCache::sweep does.

    FAILURE. Every method degrades to "no catalog" when the database will not open. A
    search returns nothing, a commit does nothing, and browsing is unaffected.
*/

/*
    One image's worth of catalog data, read off the DataModel on the GUI thread and then
    handed to a pool thread. A plain value with no Qt model types in it, deliberately: the
    model is not thread-safe and must not be touched from the thread that does the insert.
*/
struct CatalogRow
{
    QString path;               // as the filesystem spelled it
    QString folder;
    QString filename;
    QString ext;
    qint64 srcSize = 0;
    qint64 srcMtime = 0;        // seconds since epoch
    qint64 sidecarMtime = 0;    // 0 when there is no sidecar
    QDateTime captured;
    int rating = 0;
    QString label;
    bool pick = false;
    QString title;
    QString creator;
    QString copyright;
    QString make;
    QString model;
    QString lens;
    int iso = 0;
    double aperture = 0;
    double shutter = 0;
    double focalLength = 0;
    int width = 0;
    int height = 0;
    QString gpsCoord;
    QStringList keywords;       // dc:subject, leaf names
    QStringList keywordPaths;   // lr:hierarchicalSubject, "A|B|C"
};

/*
    What to search for. Empty fields are "don't care", so a default-constructed query
    matches everything -- which is what the dock shows before the user types.
*/
struct CatalogQuery
{
    /* Free text, passed to FTS5. Bare words are AND-ed and prefix-matched; the user can
       also write FTS syntax directly (quoted phrases, OR, NOT, keywords:heron). */
    QString text;
    /* Exact keyword, from the facet tree. Matches the keyword itself and, when it names a
       hierarchy node, everything beneath it. */
    QString keyword;
    int minRating = 0;
    QString label;
    QString model;
    QString lens;
    QDateTime from;
    QDateTime to;
    /* Restrict to one folder subtree. Empty means the whole catalog. */
    QString folder;
    /* Rows whose source file was missing at the last sweep are excluded by default: they
       are usually an ejected card, and offering to load them would fail. */
    bool includeMissing = false;
};

/* A keyword and how many catalogued images carry it -- what the facet tree renders. */
struct CatalogKeyword
{
    QString name;       // leaf name
    QString path;       // full hierarchical path, or "" for a flat keyword
    int count = 0;
};

class Catalog
{
public:
    static Catalog &instance();

    /* Whether the catalog is usable at all. False means the database would not open;
       callers grey their controls and say so in the panel rather than failing later. */
    bool isAvailable();

    /* Upsert these rows in ONE transaction. Rows whose freshness stamp already matches
       are skipped without touching the database. Returns how many were actually written.
       Safe to call off the GUI thread. */
    int commit(const QVector<CatalogRow> &rows);

    /* Of these paths, which are absent from the catalog or have changed since they were
       indexed. The scanner asks this before parsing anything. */
    QSet<QString> staleOf(const QList<CatalogRow> &candidates);

    /* Matching image paths, most recently captured first, at most limit of them. total
       (optional) receives the full match count, so the UI can say "showing 500 of
       3,214". */
    QStringList search(const CatalogQuery &q, int limit = 5000, int *total = nullptr);

    /* Every keyword in the catalog with its image count, for the facet tree. */
    QList<CatalogKeyword> keywords();

    /* How many images the catalog holds, and how many folders they came from. */
    int count();
    int folderCount();

    /* Demote rows whose source file is gone, skipping unmounted volumes so an ejected
       card never reads as a mass deletion. Returns the number demoted. */
    int sweep();

    /* File-operation sync. Call via Utilities/fileops.h, not directly. */
    void onMoved(const QString &srcPath, const QString &dstPath);
    void onDeleted(const QString &fPath);

    /* Drop every catalogued row. Does NOT touch the devPreview tables in the same
       file. */
    void clear();

private:
    Catalog() = default;
    Q_DISABLE_COPY(Catalog)

    /* The calling thread's connection, with the catalog's own lazy init done. May be
       closed -- every caller checks isOpen() and degrades to "no catalog". */
    QSqlDatabase dbLocked();
    void ensureLoadedLocked();

    /* Id for a keyword, inserting it (and its ancestors) if new. Memoised, so a folder
       commit costs one round trip per DISTINCT keyword rather than one per image. */
    qint64 keywordIdLocked(QSqlDatabase &db, const QString &name, const QString &path);
    /* Write one row's keyword links, replacing whatever it had. */
    void writeKeywordsLocked(QSqlDatabase &db, qint64 imageId, const CatalogRow &r);
    void writeFtsLocked(QSqlDatabase &db, qint64 imageId, const CatalogRow &r);

    mutable QMutex mutex;
    /* pathfold + '\\x1f' + namefold -> keyword.id. Cleared by clear(); otherwise it only
       grows, and a keyword id is stable for the life of the database. */
    QHash<QString, qint64> keywordIds;
    /* Which database the memo above describes, so pointing CacheDb at a different file
       invalidates it rather than mixing two files' primary keys. */
    QString loadedPath;
};

#endif // CATALOG_H
