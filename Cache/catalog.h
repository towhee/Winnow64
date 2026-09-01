#ifndef CATALOG_H
#define CATALOG_H

#include <QDateTime>
#include <QHash>
#include <QMap>
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
    image row, because the category list the UI wants ("show me every keyword, with
    counts") is then an index scan instead of a quarter of a million string splits.

    KEYWORDS ARE FLAT, keyed on the NAME alone. The hierarchy is flattened before it gets
    here -- Metadata/keywordflatten.h turns "Location|Canada|BC" into three keywords -- so
    an ancestor is an ordinary keyword and a tag Lightroom wrote both ways (leaf in
    dc:subject, path in lr:hierarchicalSubject) is ONE row rather than two. Schema 3 kept
    both forms and so listed the same keyword twice with its image count split; schema 4
    merges them, in place, without re-reading a single file.

    WHAT THE HIERARCHY LEAVES BEHIND is keyword_context: which parents a name has been
    seen under. A name with more than one parent is AMBIGUOUS -- "Vancouver" under both
    Canada and USA -- which is the one thing flattening genuinely loses. The docks colour
    those and name their parents, and the user resolves them with an exclude filter
    (include Vancouver, exclude USA). Nothing else reads the old hierarchy.

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
    /* The FLAT vocabulary, already de-duplicated by flattenKeywords: dc:subject's leaves
       and every node of every hierarchical path, as one list of names. This is what gets
       indexed and searched. */
    QStringList keywords;
    /* lr:hierarchicalSubject as the file spelled it, "A|B|C". NOT indexed as structure --
       it is read only to record which parent each name was seen under (keyword_context),
       which is what makes ambiguity detectable. */
    QStringList keywordPaths;
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
    /* Exact keyword names, from the category list. Multiple keywords are OR-ed, matching
       what checking several items in one Filters category does. Because the vocabulary is
       flat, picking an ancestor name already reaches everything that was beneath it --
       there is no subtree walk to ask for. */
    QStringList keywords;
    /* Keyword names to reject. AND-NOT, applied after everything else, and the way an
       ambiguous name is resolved: keywords = {Vancouver}, excludeKeywords = {USA}. */
    QStringList excludeKeywords;
    int minRating = 0;
    QString label;
    QString model;
    QString lens;
    QDateTime from;
    QDateTime to;
    /* Restrict to one folder subtree. Empty means the whole catalog. */
    QString folder;
    /* The generic CATEGORY restriction, keyed by G::dataModelColumns: values within one
       column are OR-ed, columns are AND-ed, and exclude is AND-NOT over everything. This
       is what lets the Find dock hand the same checked-item structure to either scope
       instead of the query growing a named field per category. Keywords are NOT in here
       -- they need a join rather than a column compare, so they keep the two lists
       above. */
    QMap<int, QStringList> include;
    QMap<int, QStringList> exclude;
    /* Rows whose source file was missing at the last sweep are excluded by default: they
       are usually an ejected card, and offering to load them would fail. */
    bool includeMissing = false;
};

/* A keyword and how many catalogued images carry it -- what the category list renders. */
struct CatalogKeyword
{
    QString name;
    int count = 0;
    /* The parent names this keyword has been seen under, from keyword_context. Empty for
       a keyword that has only ever been flat. More than ONE means the name is ambiguous:
       the docks colour it and list these in its tooltip, so the user can see that
       "Vancouver" is two places before deciding what to exclude. */
    QStringList contexts;
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

    /* Every keyword in the catalog with its image count and its parent names, for the
       category list. */
    QList<CatalogKeyword> keywords();

    /* Every distinct value of one CATEGORY, with how many live images carry it -- the
       catalog's half of the shared category vocabulary the Find dock renders in
       Catalog scope. dmColumn is a G::dataModelColumns value, so the panel asks the
       index and the datamodel the same question in the same terms; a column the catalog
       cannot answer (duplicates, the search flag) returns empty and the panel hides that
       category. Strings are formatted to match EXACTLY what DataModel writes into the
       same column, because the user checks one category item and both scopes must agree
       what it means. */
    QMap<QString, int> categoryItems(int dmColumn);

    /* The names recorded under more than one parent -- the keywords whose meaning
       flattening made ambiguous. Case-folded, so callers compare with keywordFold().
       Cheap enough (one indexed GROUP BY over the vocabulary, not the images) to call
       once per filter build. Empty when there is no catalog, which callers must treat as
       "unknown", not as "nothing is ambiguous". */
    QSet<QString> ambiguousKeywords();

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

    /* Id for a keyword name, inserting it if new. Memoised, so a folder commit costs one
       round trip per DISTINCT keyword rather than one per image. */
    qint64 keywordIdLocked(QSqlDatabase &db, const QString &name);
    /* Write one row's keyword links, replacing whatever it had. */
    void writeKeywordsLocked(QSqlDatabase &db, qint64 imageId, const CatalogRow &r);
    void writeFtsLocked(QSqlDatabase &db, qint64 imageId, const CatalogRow &r);

    mutable QMutex mutex;
    /* namefold -> keyword.id. Cleared by clear(); otherwise it only grows, and a keyword
       id is stable for the life of the database. */
    QHash<QString, qint64> keywordIds;
    /* Which database the memo above describes, so pointing CacheDb at a different file
       invalidates it rather than mixing two files' primary keys. */
    QString loadedPath;
};

#endif // CATALOG_H
