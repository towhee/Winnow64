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
#include <QVariant>
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
    /*  --- schema 6: the fields a datamodel ROW displays that a search index
        never needed. Added when serving a row's metadata from the catalog was
        fingerprinted against reading it from the file and these were exactly
        what diverged -- see "Metadata From the Index" in Documentation.txt.

        orientation is the one that mattered: it drives rotation, so a row
        served without it shows the picture on its side. */
    int orientation = 0;
    QString exposureComp;
    double focusX = -1;
    double focusY = -1;
    QString email;
    QString url;
    /*  The ORIGINAL values, as the file had them before the user edited
        anything. They are how an edit is reverted and how a sidecar write
        decides whether anything actually changed, so a row that came back
        without them would make Winnow think every image had been edited. */
    QString _rating;
    QString _label;
    QString _creator;
    QString _title;
    QString _copyright;
    QString _email;
    QString _url;
    /*  Whether the sidecar holds a develop recipe, and its hash. The badge in
        the icon delegate reads the first; the devPreview cache is keyed on the
        second. */
    bool developed = false;
    QString devPreviewKey;
    /*  Composed by the metadata read, and only when it succeeded -- see the
        schema 7 note in cachedb.cpp for why it is stored rather than rebuilt. */
    QString shootingInfo;

    QStringList keywords;
    /*  dc:subject AS THE FILE SPELLED IT -- not the flattened vocabulary
        above. Only this one may be written back to a file; see the schema 7
        note in cachedb.cpp. */
    QStringList keywordsLiteral;
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
       is what lets the Filter dock hand the same checked-item structure to either scope
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

    /*  THE SAME QUESTION ASKED BY A BROWSER RATHER THAN BY AN INDEXER, and the
        difference is what happens to a path the catalog has never seen.

        staleOf() calls it STALE, because its caller is about to read every file it
        names and an unindexed image is exactly one of the files that must be read.
        This caller is the opposite: it holds rows that were FILLED FROM the index and
        is asking which of them the index can no longer vouch for. A path the catalog
        does not know was never filled from it, so there is nothing to be stale
        against -- reporting it would make the verification pass re-read every
        uncatalogued row in the window, on every scroll, forever.

        So: INDEXED AND NO LONGER CURRENT. Same three stamps, same comparison, and
        the two must agree about what "current" means or a row would be served from
        the index by one and re-read by the other.

        PAGED, taking the lock per page, for the reason availabilityOf states: the
        queries run off the GUI thread, but a lock held for one query per row is what
        every GUI-thread call into the catalog then waits on.

        Safe to call off the GUI thread. */
    QSet<QString> outOfDate(const QList<CatalogRow> &candidates);

    /* THE INDEX AS A METADATA SOURCE, not just a search index.

       Hands back the catalogued row for each candidate whose freshness stamp still
       matches -- so the loader can populate those rows from the database instead of
       opening the file and parsing its metadata. Candidates whose stamp does not match,
       or that were never catalogued, are simply absent from the result: they are the gap
       the file reader still has to fill.

       This is staleOf() read the other way round, and it is deliberately ONE query per
       call rather than one per path. staleOf answers "what must I read?" for the
       scanner, which then reads it; this answers "what need I not read?" for the loader,
       which then skips it. Same stamp, same comparison, opposite consumer -- and the two
       must agree, or a row would be both skipped and unindexed.

       The caller supplies srcSize, srcMtime and sidecarMtime on each candidate (the file
       scan already stats for them) and gets back everything else. Keyed by the
       candidate's path AS SUPPLIED, so the caller can look results up with the spelling
       it passed in rather than a folded key.

       Safe to call off the GUI thread. */
    QHash<QString, CatalogRow> fetchFresh(const QList<CatalogRow> &candidates);

    /* Matching image paths, most recently captured first, at most limit of them. total
       (optional) receives the full match count, so the UI can say "showing 500 of
       3,214". */
    QStringList search(const CatalogQuery &q, int limit = 5000, int *total = nullptr);

    /* The same matches as search(), in the same order, as WHOLE ROWS -- everything a
       datamodel row displays, without opening a single file. This is what lets a catalog
       result be loaded like a folder rather than read like one: the rows come back in the
       query that found them, instead of one indexed lookup per path afterwards.

       It TRUSTS THE INDEX. No file is stat'd, so nothing here can tell whether a row is
       still current; that question needs the file's stamps and belongs to fetchFresh,
       which the caller uses for the rows it is about to show. limit <= 0 means every
       match. total (optional) receives the full count as in search(). */
    QVector<CatalogRow> searchRows(const CatalogQuery &q, int limit = 0,
                                   int *total = nullptr);

    /*  WHY IS THIS IMAGE NOT OPENABLE RIGHT NOW? The catalog can outlive the
        files it indexes, and the two ways that happens are not the same thing:

          Present  the file is where the catalog says it is.
          Offline  its VOLUME is not mounted -- an ejected card, an unplugged
                   drive, a share that is not up. The image is fine; the disk is
                   absent, and plugging it back in restores it.
          Missing  the volume IS mounted and the file is not there. It was
                   deleted or moved by something other than Winnow.

        The sweep already relies on this distinction -- it demotes a row only
        when the volume is mounted and the file is still gone, precisely so that
        ejecting a card is not read as a mass deletion -- but nothing could ASK
        for it, so the catalog's only answer to "can I open this" was to leave
        the row out of the results entirely. That is the wrong answer for
        browsing: with the whole catalog visible, "that disk isn't plugged in" is
        information the user wants, not a row to hide.

        Offline is computed against the mount table AT CALL TIME, not stored: a
        volume's presence changes without anything telling the catalog, so a
        stored answer would be wrong the moment a drive is plugged in. Missing is
        the stored `live` flag, which is what the sweep maintains.

        A FOURTH STATE, added later:

          Unreadable  the file is there and Winnow's parsers cannot read it. It is in
                      the index as a STUB -- path, folder, filename, stamps -- and
                      nothing else, because there was nothing else to record. Without
                      the stub such a file is invisible: absent from the index, present
                      on disk, and the only evidence is a count that never reaches zero.

        Unreadable is STORED (the `unreadable` column, schema 9) rather than inferred:
        only the scanner knows a parse was attempted and failed, and re-deriving it would
        mean re-attempting the parse. */
    enum class Availability { Present, Offline, Missing, Unreadable };

    /*  THE THREE STATES SPELLED ONCE. Three places name them -- the badge tooltip on
        the thumbnail, the Availability column in the table, and the Filters category
        the user checks -- and the filter is the one that makes a shared spelling
        load-bearing rather than tidy: a checked filter item is compared against what
        the MODEL holds, which is the int, so the item has to carry the code and show
        the label. Converting in one place is what keeps "Offline" in the category and
        "Offline" on the badge the same word.

        availabilityCode returns Present for anything it does not recognise, which is
        the same answer an unset cell gives. */
    static QString availabilityLabel(int code);
    static int availabilityCode(const QString &label);

    /*  THE MONTH NAMES SPELLED ONCE, for the same reason the availability labels are:
        the datamodel writes G::MonthColumn from this and categorySql builds its CASE
        from it, so a folder and the library cannot disagree about what June is called.
        DELIBERATELY NOT LOCALISED -- QDate::toString("MMM") follows the system locale,
        which would make the value a user's language rather than a fact about the image,
        and would not match the English CASE the SQL side has to spell out. month is
        1-12; anything else returns an empty string, which is what a row with no capture
        date holds. */
    static QString monthLabel(int month);
    /*  The 12 names in calendar order -- what Filters uses to insert the items in a
        sequence rather than alphabetically. */
    static QStringList monthLabels();

    /*  The availability of each of these paths, in one pass. Paths the catalog
        does not know are absent from the result rather than reported Missing --
        "not indexed" is a different statement from "indexed and gone".

        ONE MOUNT-TABLE WALK for the whole call (see Cache/mountsnapshot.h): it
        is a syscall per volume, and a caller asking about a folder's worth of
        rows should not pay it per row. */
    QHash<QString, Availability> availabilityOf(const QStringList &paths);

    /* Every keyword in the catalog with its image count and its parent names, for the
       category list. */
    QList<CatalogKeyword> keywords();

    /* Every distinct value of one CATEGORY, with how many live images carry it -- the
       catalog's half of the shared category vocabulary the Filter dock renders in
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

    /*  HOW MANY CATALOGUED IMAGES LIE UNDER THIS FOLDER, and forget them. recurse tells
        the two apart: the folder alone, or the folder and everything below it.

        FORGET IS A DELETE, NOT A DEMOTE, and it is the one place that is right. sweep and
        reconcileFolder demote because they INFER that a file is gone and can be wrong (an
        ejected card, a folder briefly unreadable), so the row must be able to come back.
        This is not an inference: the user has said they do not want these folders indexed,
        the files are still there, and a demoted row would sit in the database forever
        being neither findable nor reclaimable. Nothing is lost that a scan cannot rebuild
        -- the images themselves are untouched.

        countUnder EXISTS SO THE DELETE CAN BE SHOWN BEFORE IT HAPPENS. Forgetting tens of
        thousands of rows is not something to discover afterwards from a changed number.
        Returns the number of rows counted / deleted. */
    int countUnder(const QString &folder, bool recurse);
    int forgetUnder(const QString &folder, bool recurse);

    /*  EVERY CATALOGUED FOLDER AND HOW MANY IMAGES IT HOLDS, in one GROUP BY over the
        image_folder index. The caller that reconciles the catalog to the scope table
        needs both facts about every folder at once -- which folders exist, so it can ask
        the scope about each, and what forgetting one would cost, so the total can be put
        in front of the user before anything is deleted. Asking countUnder per folder
        would be the same answer for one query per folder. The vocabulary is folders, not
        images, so this stays cheap at the 250,000 rows the database is sized for. */
    QMap<QString, int> folderCounts();

    /*  FORGET THESE FOLDERS EXACTLY -- no prefix test, because the caller has already
        decided folder by folder (see MW::reconcileCatalogToScope). forgetUnder answers
        "this folder and what is under it"; this answers "these folders", which is what
        a scope reconcile has to say and what a prefix cannot express: an included branch
        may sit inside an excluded one. Returns rows deleted.

        THE KEYWORD VOCABULARY IS NOT TOUCHED, and that is the whole reason this is not
        clear(). image_keyword goes by cascade with the images, but keyword and
        keyword_context have no foreign key onto image and survive: the vocabulary is
        USER INTENT, like the scope table itself, where the images are a rebuildable
        index. Emptying the catalog by removing every scope row therefore leaves the
        keywords the user has accumulated intact. */
    int forgetFolders(const QStringList &folders);

    /*  Record files the scanner could not parse, as stub rows flagged unreadable. They
        carry the freshness stamp like any other row, so a rescan does not re-attempt them
        until the file itself changes -- and a file that later parses normally has the
        flag cleared by the ordinary commit. Returns rows written.

        WHY THEY ARE IN THE INDEX AT ALL. Left out, they are the difference between what
        the folders hold and what the catalog holds that nothing can explain and no scan
        can close. In, they are one Availability value the user can filter on and see. */
    int commitUnreadable(const QVector<CatalogRow> &rows);
    /* How many rows are flagged unreadable. */
    int unreadableCount();

    /* Demote rows whose source file is gone, skipping unmounted volumes so an ejected
       card never reads as a mass deletion. Returns the number demoted. */
    int sweep();

    /*  RECONCILE ONE FOLDER AGAINST WHAT WAS ACTUALLY THERE. Given the paths a folder
        enumeration just found, demote every live row this folder holds that is not among
        them: those files are gone.

        WHY THIS IS NOT sweep(). sweep stats one file per row over the WHOLE catalog, so
        it can only run as a rare one-shot; this answers the same question for one folder
        with NO stat at all, because the caller has just listed the directory and that
        listing IS the truth. It is the write-back half of a folder scope -- the load
        reconciles the index against the filesystem, and this is where the index learns
        what the reconcile found.

        THE CALLER MUST HAVE ENUMERATED THE WHOLE FOLDER. A partial listing -- a load the
        user aborted, or one the memory cap cut short -- would demote everything it did
        not reach, so present must be complete or this must not be called. Demoting is
        recoverable (the next commit that sees the file promotes it again), but a search
        that silently stops finding half a folder is not a good way to find that out.

        Returns the number demoted. */
    int reconcileFolder(const QString &folder, const QSet<QString> &present);

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
    /* The WHERE/FROM/binds a CatalogQuery compiles to, shared by search() and
       searchRows() so the two cannot disagree about what a query matches. Caller holds
       the mutex. */
    void buildQueryLocked(const CatalogQuery &cq, QString &from,
                          QStringList &where, QVariantList &binds);
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
