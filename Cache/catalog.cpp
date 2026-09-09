#include "Cache/catalog.h"
#include "Cache/cachedb.h"
#include "Cache/mountsnapshot.h"
#include "Cache/pathkey.h"
#include "Main/global.h"
#include "Metadata/keywordpaths.h"
#include "Utilities/searchterms.h"

#include <QDir>
#include <algorithm>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QVariant>

namespace {

const char *kDbName = "index.db";

/* How many rows a long pass handles between takes of the mutex. Big enough that the
   locking is not the cost, small enough that a Search on the GUI thread never waits on
   more than a few hundred stats. Matches DevPreviewCache. */
constexpr int kPageRows = 512;

QString defaultCacheDir()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
           + "/PreviewCache";
}

/* Case folding, matching Cache/pathkey.h's reasoning: toCaseFolded is the Unicode-correct
   locale-independent operation, where toLower is neither. Delegates to the shared
   keyword helper so the index folds a name exactly as the datamodel does -- if the two
   disagreed, the category and the search would disagree about the same picture. */
QString fold(const QString &s)
{
    return keywordFold(s);
}

/*
    Bind a text value, never a NULL.

    A default-constructed QString is NULL to QSqlQuery, not '', and every text column in
    the image table is NOT NULL. An image with no title, no lens or no GPS is the ORDINARY
    case, so without this the very first such row aborts the whole folder's transaction --
    which is how this arrived: every insert failed with "NOT NULL constraint failed".

    The columns stay NOT NULL rather than being relaxed, so that searching never has to
    reason about the difference between "no title" and "unknown title".
*/
QVariant text(const QString &s)
{
    return QVariant(s.isNull() ? QString("") : s);
}

/*
    THE COLUMNS THAT MAKE A ROW, in one place because two queries return them.

    fetchFresh asks for one image by primary key; searchRows asks for every image the
    query matched. They must produce the IDENTICAL CatalogRow or a row would mean
    something different depending on which path fetched it -- which is the drift the
    shared IndexMetadata mapping exists to prevent one layer up. Qualified with the i.
    alias so both statements can use the same string.
*/
const char *kRowColumns =
    " i.id, i.srcsize, i.srcmtime, i.sidecarmtime,"
    " i.path, i.folder, i.filename, i.ext,"
    " i.captured, i.rating, i.label, i.pick, i.title, i.creator, i.copyright,"
    " i.make, i.model, i.lens, i.iso, i.aperture, i.shutter, i.focallength,"
    " i.width, i.height, i.gpscoord,"
    " i.orientation, i.exposurecomp, i.focusx, i.focusy, i.email, i.url,"
    " i.orig_rating, i.orig_label, i.orig_creator, i.orig_title,"
    " i.orig_copyright, i.orig_email, i.orig_url, i.developed, i.devpreviewkey,"
    " i.keywordpaths, i.shootinginfo, i.keywords_literal";

/*
    One row of kRowColumns into a CatalogRow, keywords excluded -- they are a join and
    each caller fetches them the way that suits its shape. Returns the image id, which
    is what a keyword lookup needs and CatalogRow does not carry.
*/
qint64 readRow(const QSqlQuery &q, CatalogRow &r)
{
    r.path = q.value(4).toString();
    r.srcSize = q.value(1).toLongLong();
    r.srcMtime = q.value(2).toLongLong();
    r.sidecarMtime = q.value(3).toLongLong();
    r.folder = q.value(5).toString();
    r.filename = q.value(6).toString();
    r.ext = q.value(7).toString();
    /*  captured is stored as SECONDS SINCE EPOCH -- commit() binds
        r.captured.toSecsSinceEpoch(), and the category SQL reads it with
        strftime(..., 'unixepoch'). Reading it back with QVariant::toDateTime gave a
        QDateTime parsed from the DIGITS of the integer, which is a plausible-looking date
        that is simply wrong: an A7R2 shot in September 2016 came back as April 2017.
        Found by fingerprinting a row served from the catalog against the same row read
        from its file. */
    r.captured = q.value(8).isNull()
                     ? QDateTime()
                     : QDateTime::fromSecsSinceEpoch(q.value(8).toLongLong());
    r.rating = q.value(9).toInt();
    r.label = q.value(10).toString();
    r.pick = q.value(11).toBool();
    r.title = q.value(12).toString();
    r.creator = q.value(13).toString();
    r.copyright = q.value(14).toString();
    r.make = q.value(15).toString();
    r.model = q.value(16).toString();
    r.lens = q.value(17).toString();
    r.iso = q.value(18).toInt();
    r.aperture = q.value(19).toDouble();
    r.shutter = q.value(20).toDouble();
    r.focalLength = q.value(21).toDouble();
    r.width = q.value(22).toInt();
    r.height = q.value(23).toInt();
    r.gpsCoord = q.value(24).toString();
    r.orientation = q.value(25).toInt();
    r.exposureComp = q.value(26).toString();
    r.focusX = q.value(27).toDouble();
    r.focusY = q.value(28).toDouble();
    r.email = q.value(29).toString();
    r.url = q.value(30).toString();
    r._rating = q.value(31).toString();
    r._label = q.value(32).toString();
    r._creator = q.value(33).toString();
    r._title = q.value(34).toString();
    r._copyright = q.value(35).toString();
    r._email = q.value(36).toString();
    r._url = q.value(37).toString();
    r.developed = q.value(38).toBool();
    r.devPreviewKey = q.value(39).toString();
    const QString kp = q.value(40).toString();
    if (!kp.isEmpty()) r.keywordPaths = kp.split('\n', Qt::SkipEmptyParts);
    r.shootingInfo = q.value(41).toString();
    const QString kl = q.value(42).toString();
    if (!kl.isEmpty()) r.keywordsLiteral = kl.split('\n', Qt::SkipEmptyParts);
    return q.value(0).toLongLong();
}

/*
    The two flat-keyword scans share this: the depth-1 paths ONE image carries.

    A path is flat when it has no separator after keywordEffectivePaths has run -- that
    is, a dc:subject entry that names no node of any of the same image's hierarchical
    paths. Everything else about a flat keyword follows from that one test, so both
    scans ask it here rather than each spelling out the rule.
*/
QStringList flatOf(const CatalogRow &r)
{
    QStringList out;
    for (const QString &p : keywordEffectivePaths(r.keywordsLiteral, r.keywordPaths))
        if (!p.contains('|')) out << p;
    return out;
}


/*
    The SQL that produces one category item's value, keyed by the datamodel column the
    Filters panel maps that category to.

    ONE MAP, USED BOTH WAYS -- categoryItems() lists the distinct values and search()
    compares against them -- so a category item the user checks cannot mean something
    different from the item that was offered. Two expressions would drift the first time
    one was edited.

    THE STRINGS MUST MATCH WHAT DataModel WRITES into the same column, because the Filter
    dock shows one list and the user does not know which scope produced it: TypeColumn is
    the suffix UPPER-cased, YearColumn is "yyyy", MonthColumn is the English abbreviation
    ("Jan".."Dec"), ISOColumn is the number right-justified to six, DayColumn is
    "yyyy-MM-dd", FolderName is
    the folder's NAME and not its path, Pick is the words "Picked"/"Unpicked", and Rating
    is the digit as text with "" for unrated.

    A COLUMN NOT LISTED HERE CANNOT BE ANSWERED by the index -- duplicates (CompareColumn)
    is a comparison of what is loaded, and SearchColumn is the search box's own flag -- so
    categoryItems() returns nothing and the panel hides that category rather than showing
    an empty one that looks broken.
*/
QString categorySql(int dmColumn)
{
    /* IFNULL, applied once below, is what makes "no value" a value. A NULL title or an
       image with no capture date has to come back as the empty string so it groups into
       the blank category item and so a checked blank item matches it with IN (''), rather
       than vanishing from both the list and the query. */
    QString expr;
    switch (dmColumn) {
    case G::RatingColumn:     expr = "CASE WHEN i.rating > 0 THEN CAST(i.rating AS TEXT)"
                                     " ELSE '' END"; break;
    case G::LabelColumn:      expr = "i.label"; break;
    case G::PickColumn:       expr = "CASE WHEN i.pick THEN 'Picked' ELSE 'Unpicked' END";
                              break;
    case G::TypeColumn:       expr = "UPPER(i.ext)"; break;
    case G::CameraModelColumn: expr = "i.model"; break;
    case G::LensColumn:       expr = "i.lens"; break;
    case G::TitleColumn:      expr = "i.title"; break;
    case G::CreatorColumn:    expr = "i.creator"; break;
    /* Stored as a REAL; the datamodel shows the number, so drop a trailing ".0" that
       would otherwise make "400" and "400.0" look like two focal lengths. */
    case G::FocalLengthColumn: expr = "CAST(CAST(i.focallength AS INTEGER) AS TEXT)";
                              break;
    /* captured is seconds since epoch; 'unixepoch' is what makes these local-agnostic and
       stable, which a category list has to be. */
    case G::YearColumn:       expr = "strftime('%Y', i.captured, 'unixepoch')"; break;
    case G::DayColumn:        expr = "strftime('%Y-%m-%d', i.captured, 'unixepoch')"; break;
    /* The month NAME, spelled from Catalog::monthLabels so the CASE cannot drift from
       what DataModel writes into G::MonthColumn. A row with no capture date gives NULL
       here and IFNULL folds it into the blank item, exactly as Year and Day do. */
    case G::MonthColumn: {
        expr = "CASE strftime('%m', i.captured, 'unixepoch')";
        const QStringList names = Catalog::monthLabels();
        for (int m = 1; m <= 12; ++m)
            expr += QString(" WHEN '%1' THEN '%2'")
                        .arg(m, 2, 10, QChar('0')).arg(names.at(m - 1));
        expr += " END";
        break;
    }
    /* ISO right-justified to six, which is what BuildFilters does to the datamodel's
       int before counting it -- the padding is what makes "800" sort after "1600"
       rather than between "100" and "8000". printf() is SQLite's own, so the two sides
       pad identically. Six covers every ISO a camera reports (409600). */
    case G::ISOColumn:        expr = "printf('%6d', i.iso)"; break;
    /* The folder NAME, not the path: rtrim everything up to the last separator. */
    case G::FolderNameColumn: expr = "replace(i.folder, rtrim(i.folder,"
                                     " replace(i.folder, '/', '')), '')"; break;
    default:                  return QString();
    }
    return "IFNULL(" + expr + ", '')";
}

qint64 nowSecs()
{
    return QDateTime::currentSecsSinceEpoch();
}

}  // namespace

Catalog &Catalog::instance()
{
    static Catalog c;
    return c;
}

void Catalog::ensureLoadedLocked()
{
/*
    Make sure the shared database has a location, WITHOUT overriding one already chosen.

    Both tenants open lazily and either may be first, so this deliberately only supplies
    the DEFAULT when nothing has been set. An unconditional setPath here would be a bug
    with teeth: setPath closes every open connection and bumps the generation, so a
    Catalog call arriving after DevPreviewCache::setCacheDir had pointed the database
    somewhere else -- a test at a temp dir, or a future "move the cache" preference --
    would silently drag it back to AppDataLocation and strand the previews.
*/
    if (CacheDb::instance().path().isEmpty()) {
        const QString dir = defaultCacheDir();
        QDir().mkpath(dir);
        CacheDb::instance().setPath(dir + "/" + kDbName);
    }

    /* Keyword ids are per-database: they are that file's primary keys. If the file has
       changed under us the memo describes rows in a database nobody has open any more,
       and reusing it would attach this session's images to another file's keyword ids. */
    const QString current = CacheDb::instance().path();
    if (current != loadedPath) {
        keywordIds.clear();
        loadedPath = current;
    }
}

QSqlDatabase Catalog::dbLocked()
{
    ensureLoadedLocked();
    return CacheDb::instance().db();
}

bool Catalog::isAvailable()
{
    QMutexLocker lk(&mutex);
    return dbLocked().isOpen();
}

/* ---------------------------------------------------------------------------------
   Keywords
   --------------------------------------------------------------------------------- */

qint64 Catalog::keywordIdLocked(QSqlDatabase &db, const QString &path)
{
/*
    The id for one keyword PATH, inserting it if it is new.

    KEYED ON THE WHOLE PATH (schema 10). Schema 4 keyed on the leaf name alone, which made
    "Location|Canada|BC|Vancouver" and "Location|USA|WA|Vancouver" one keyword meaning two
    places, with their image counts merged. The tag Lightroom writes twice -- "Heron" in
    dc:subject and "Fauna|Bird|Heron" in lr:hierarchicalSubject -- is still ONE row,
    because keywordEffectivePaths consumes the leaf into the path before anything reaches
    here, so there is no second form left to collapse.

    BOTH COLUMNS ARE WRITTEN. path/pathfold are the identity; name/namefold are the LEAF,
    which is what a tree draws and what the type-ahead completer searches. Deriving the
    leaf on read instead would mean every caller splitting the path the same way, which is
    the kind of duplication Metadata/keywordpaths.h exists to prevent.

    MEMOISED, because a folder of 2,000 images typically carries a few dozen DISTINCT
    keywords: without the memo this is two round trips per keyword per image, with it, two
    per keyword per session. The key is what the unique index is on, so the memo and the
    table can never disagree about what identifies a keyword.
*/
    const QString pathFold = fold(path);
    if (pathFold.isEmpty()) return 0;

    const auto it = keywordIds.constFind(pathFold);
    if (it != keywordIds.constEnd()) return it.value();

    const QString leaf = keywordLeafOf(path);

    QSqlQuery q(db);
    q.prepare("INSERT INTO keyword (name, namefold, path, pathfold)"
              " VALUES (?, ?, ?, ?)"
              " ON CONFLICT(pathfold) DO NOTHING");
    q.addBindValue(text(leaf));
    q.addBindValue(text(fold(leaf)));
    q.addBindValue(text(path.trimmed()));
    q.addBindValue(text(pathFold));

    if (!q.exec()) {
        G::issueDedup("Warning", "Catalog keyword insert failed: " + q.lastError().text(),
                      "Catalog::keywordIdLocked", -1, path);
        return 0;
    }

    qint64 id = q.lastInsertId().toLongLong();
    if (!id) {
        /* DO NOTHING fired: the row already existed (another folder, or a previous
           session), so look it up rather than treating a conflict as a failure. */
        QSqlQuery sel(db);
        sel.prepare("SELECT id FROM keyword WHERE pathfold = ?");
        sel.addBindValue(pathFold);
        if (sel.exec() && sel.next()) id = sel.value(0).toLongLong();
    }
    if (id) keywordIds.insert(pathFold, id);
    return id;
}

void Catalog::writeKeywordsLocked(QSqlDatabase &db, qint64 imageId, const CatalogRow &r)
{
/*
    Replace this image's keyword links.

    DELETE-THEN-INSERT rather than a diff: an image's keyword list is a handful of rows,
    the delete is one indexed statement, and a diff would have to be right about removals
    to be worth anything. Removing a keyword in Lightroom must remove it here too, and
    this is what makes that fall out for free.

    THE LINKS ARE DERIVED FROM THE TEXT THIS SAME COMMIT IS STORING, not from r.keywords.
    That is the whole point of the two lines below, and it was not always so.

    r.keywords is ALSO the prefix expansion -- DataModel::catalogRowFor reads it out of
    G::KeywordsAllColumn, and CatalogScanner computes it -- so using it looks equivalent
    and saves a little work. It is not equivalent: it is a SECOND source for the same
    fact, and two sources drift. The image row's keywords_literal and keywordpaths come
    from r.keywordsLiteral and r.keywordPaths; the links came from r.keywords; and when
    the datamodel column was stale or half-written the commit stored text that said one
    thing and links that said another. On the author's library that left an image whose
    text carried "Location|New Zealand|Castlepoint" with no link to it -- the Filters
    panel, which counts the text, said four images and the Keywords dock, which counts the
    links, said one. Schema 12 repaired a library-wide instance of exactly this, and the
    drift came back, because a repair cannot outrun a writer that keeps producing it.

    DERIVING THEM HERE MAKES THE DISAGREEMENT IMPOSSIBLE rather than rare. Whatever state
    any datamodel column was in, the links this row gets are the prefix expansion of the
    text this row gets, computed together from the same two strings by the same functions
    the migration uses. The cost is one keywordEffectivePaths and one keywordPrefixExpand
    per committed row, against a commit that is already writing several columns and a
    full-text row.

    AN ANCESTOR IS AN ORDINARY ROW in the resulting list, which is what keeps a filter on
    "Fauna" reaching an image tagged only "Fauna|Bird|Heron" with plain equality, and what
    makes a parent's count already its subtree total.
*/
    QSqlQuery del(db);
    del.prepare("DELETE FROM image_keyword WHERE image_id = ?");
    del.addBindValue(imageId);
    del.exec();

    const QStringList expanded =
        keywordPrefixExpand(keywordEffectivePaths(r.keywordsLiteral, r.keywordPaths));

    QSet<qint64> ids;
    for (const QString &k : expanded) {
        const qint64 id = keywordIdLocked(db, k);
        if (id) ids.insert(id);
    }

    if (!ids.isEmpty()) {
        QSqlQuery ins(db);
        ins.prepare("INSERT OR IGNORE INTO image_keyword (image_id, keyword_id)"
                    " VALUES (?, ?)");
        for (qint64 id : ids) {
            ins.addBindValue(imageId);
            ins.addBindValue(id);
            ins.exec();
        }
    }
}

void Catalog::writeFtsLocked(QSqlDatabase &db, qint64 imageId, const CatalogRow &r)
{
/*
    The full-text row for this image. rowid == image.id, so the delete below is what keeps
    a re-index from leaving the old text behind and matching on words the image no longer
    carries.

    Both keyword forms go into the one column with their '|' replaced by spaces, so every
    ancestor becomes its own token: that is what lets a free text search for "wildlife"
    hit an image tagged "Wildlife|Birds|Heron", matching what the Keywords category does
    through the ancestor rows.

    r.keywords HOLDS PATHS NOW (schema 10), so it needs the same '|' treatment the
    hierarchical list has always had -- and because it is the prefix expansion of those
    same paths, the two lists overlap almost completely. De-duplicated rather than
    concatenated: fts5 does not care about a repeated token, but the stored text would
    otherwise carry every ancestor two or three times for nothing.
*/
    QSqlQuery del(db);
    del.prepare("DELETE FROM image_fts WHERE rowid = ?");
    del.addBindValue(imageId);
    del.exec();

    QStringList kw;
    QSet<QString> kwSeen;
    for (const QStringList &list : {r.keywords, r.keywordPaths}) {
        for (const QString &p : list) {
            const QString tokens = QString(p).replace('|', ' ');
            if (tokens.isEmpty() || kwSeen.contains(tokens)) continue;
            kwSeen.insert(tokens);
            kw << tokens;
        }
    }

    QStringList gear;
    if (!r.make.isEmpty())  gear << r.make;
    if (!r.model.isEmpty()) gear << r.model;
    if (!r.lens.isEmpty())  gear << r.lens;

    QSqlQuery ins(db);
    ins.prepare("INSERT INTO image_fts"
                " (rowid, keywords, title, creator, copyright, gear, filename)"
                " VALUES (?, ?, ?, ?, ?, ?, ?)");
    ins.addBindValue(imageId);
    ins.addBindValue(text(kw.join(' ')));
    ins.addBindValue(text(r.title));
    ins.addBindValue(text(r.creator));
    ins.addBindValue(text(r.copyright));
    ins.addBindValue(text(gear.join(' ')));
    ins.addBindValue(text(r.filename));
    ins.exec();
}

/* ---------------------------------------------------------------------------------
   Commit
   --------------------------------------------------------------------------------- */

int Catalog::commit(const QVector<CatalogRow> &rows)
{
/*
    Upsert every row in ONE transaction.

    ONE TRANSACTION FOR THE WHOLE FOLDER, not one per image: SQLite commits by fsync, and
    2,000 separate commits over a folder load is seconds of disk work for an index nobody
    is waiting on. A crash mid-commit loses the whole batch, which costs exactly one
    rescan of one folder.

    ROWS THAT HAVE NOT CHANGED ARE SKIPPED before any write, so revisiting a folder is a
    read of one row per image and nothing else. This matters more than it looks: the
    opportunistic capture in MW::folderChangeCompleted runs on EVERY folder change, and
    without the skip a user pacing back and forth between two folders would rewrite both
    indefinitely.
*/
    if (rows.isEmpty()) return 0;

    QMutexLocker lk(&mutex);
    QSqlDatabase db = dbLocked();
    if (!db.isOpen()) return 0;

    const MountSnapshot mounts = MountSnapshot::take();
    const qint64 now = nowSecs();

    if (!db.transaction()) return 0;

    QSqlQuery sel(db);
    sel.prepare("SELECT id, srcsize, srcmtime, sidecarmtime, unreadable FROM image"
                " WHERE pathkey = ?");

    QSqlQuery upd(db);
    upd.prepare("UPDATE image SET path = ?, folder = ?, vol = ?, filename = ?, ext = ?,"
                " srcsize = ?, srcmtime = ?, sidecarmtime = ?, indexed = ?, live = 1,"
                " captured = ?, rating = ?, label = ?, pick = ?, title = ?, creator = ?,"
                " copyright = ?, make = ?, model = ?, lens = ?, iso = ?, aperture = ?,"
                " shutter = ?, focallength = ?, width = ?, height = ?, gpscoord = ?,"
                /* schema 6: what a row DISPLAYS, beyond what a search needs */
                " orientation = ?, exposurecomp = ?, focusx = ?, focusy = ?,"
                " email = ?, url = ?, orig_rating = ?, orig_label = ?,"
                " orig_creator = ?, orig_title = ?, orig_copyright = ?,"
                " orig_email = ?, orig_url = ?, developed = ?, devpreviewkey = ?,"
                /* schema 7 */
                " keywordpaths = ?, shootinginfo = ?, keywords_literal = ?"
                " , unreadable = 0"
              " WHERE id = ?");

    QSqlQuery ins(db);
    ins.prepare("INSERT INTO image (pathkey, path, folder, vol, filename, ext,"
                " srcsize, srcmtime, sidecarmtime, indexed, live,"
                " captured, rating, label, pick, title, creator, copyright,"
                " make, model, lens, iso, aperture, shutter, focallength,"
                " width, height, gpscoord,"
                /* schema 6: what a row DISPLAYS, beyond what a search needs */
                " orientation, exposurecomp, focusx, focusy, email, url,"
                " orig_rating, orig_label, orig_creator, orig_title,"
                " orig_copyright, orig_email, orig_url, developed, devpreviewkey,"
                " keywordpaths, shootinginfo, keywords_literal)"
                " VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, 1,"
                " ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?,"
                " ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");

    int written = 0;
    for (const CatalogRow &r : rows) {
        if (r.path.isEmpty()) continue;
        const QString key = cachePathKey(r.path);

        qint64 id = 0;
        bool fresh = false;
        sel.addBindValue(key);
        if (sel.exec() && sel.next()) {
            id = sel.value(0).toLongLong();
            fresh = sel.value(1).toLongLong() == r.srcSize
                    && sel.value(2).toLongLong() == r.srcMtime
                    && sel.value(3).toLongLong() == r.sidecarMtime
                    /*  AN UNREADABLE STUB IS NEVER FRESH -- the same rule staleOf and
                        fetchFresh apply, and THIS is the copy that clears the flag. The
                        stub carries the file's stamps, so a row that has just been parsed
                        successfully looked unchanged here and the write was skipped: the
                        row stayed a stub, `unreadable` stayed 1, and the editor went on
                        reporting files it could now read perfectly well. Three functions
                        compare these stamps and all three had to learn that this row is
                        about Winnow's ability to read the file, not about the file. */
                    && !sel.value(4).toBool();
        }
        sel.finish();
        if (id && fresh) continue;

        const QVariant captured = r.captured.isValid()
                                      ? QVariant(r.captured.toSecsSinceEpoch())
                                      : QVariant();
        const QString vol = mounts.rootOf(r.path);

        QSqlQuery &w = id ? upd : ins;
        if (!id) w.addBindValue(key);
        w.addBindValue(text(r.path));
        w.addBindValue(text(r.folder));
        w.addBindValue(text(vol));
        w.addBindValue(text(r.filename));
        w.addBindValue(text(r.ext));
        w.addBindValue(r.srcSize);
        w.addBindValue(r.srcMtime);
        w.addBindValue(r.sidecarMtime);
        w.addBindValue(now);
        w.addBindValue(captured);
        w.addBindValue(r.rating);
        w.addBindValue(text(r.label));
        w.addBindValue(r.pick ? 1 : 0);
        w.addBindValue(text(r.title));
        w.addBindValue(text(r.creator));
        w.addBindValue(text(r.copyright));
        w.addBindValue(text(r.make));
        w.addBindValue(text(r.model));
        w.addBindValue(text(r.lens));
        w.addBindValue(r.iso);
        w.addBindValue(r.aperture);
        w.addBindValue(r.shutter);
        w.addBindValue(r.focalLength);
        w.addBindValue(r.width);
        w.addBindValue(r.height);
        w.addBindValue(text(r.gpsCoord));
        /*  schema 6. Bound in the SAME ORDER for both statements, which is why
            the UPDATE puts them before its WHERE and the INSERT after gpscoord:
            one bind sequence serves both. */
        w.addBindValue(r.orientation);
        w.addBindValue(text(r.exposureComp));
        w.addBindValue(r.focusX);
        w.addBindValue(r.focusY);
        w.addBindValue(text(r.email));
        w.addBindValue(text(r.url));
        w.addBindValue(text(r._rating));
        w.addBindValue(text(r._label));
        w.addBindValue(text(r._creator));
        w.addBindValue(text(r._title));
        w.addBindValue(text(r._copyright));
        w.addBindValue(text(r._email));
        w.addBindValue(text(r._url));
        w.addBindValue(r.developed ? 1 : 0);
        w.addBindValue(text(r.devPreviewKey));
        /*  Verbatim, newline separated -- see the schema 7 note. */
        w.addBindValue(text(r.keywordPaths.join('\n')));
        w.addBindValue(text(r.shootingInfo));
        w.addBindValue(text(r.keywordsLiteral.join('\n')));
        if (id) w.addBindValue(id);

        if (!w.exec()) {
            G::issueDedup("Warning", "Catalog write failed: " + w.lastError().text(),
                          "Catalog::commit", -1, r.path);
            w.finish();
            continue;
        }
        if (!id) id = w.lastInsertId().toLongLong();
        w.finish();
        if (!id) continue;

        writeKeywordsLocked(db, id, r);
        writeFtsLocked(db, id, r);
        ++written;
    }

    if (!db.commit()) {
        db.rollback();
        return 0;
    }
    return written;
}

QSet<QString> Catalog::staleOf(const QList<CatalogRow> &candidates)
{
    QSet<QString> stale;
    if (candidates.isEmpty()) return stale;

    /*  PAGED, TAKING THE LOCK PER PAGE, for the reason spelled out in availabilityOf --
        which cited this function as following the convention while it did not. A scan
        can ask about a whole library, and one query per row under a single lock is
        exactly the shape that froze the GUI for 30 seconds there. */
    for (int from = 0; from < candidates.size(); from += kPageRows) {
        const int to = qMin(candidates.size(), from + kPageRows);

        QMutexLocker lk(&mutex);
        QSqlDatabase db = dbLocked();
        /* No catalog means everything is stale: the caller should read it all rather than
           silently index nothing. */
        if (!db.isOpen()) {
            for (const CatalogRow &r : candidates) stale.insert(r.path);
            return stale;
        }

        QSqlQuery q(db);
        q.prepare("SELECT srcsize, srcmtime, sidecarmtime, unreadable FROM image"
                  " WHERE pathkey = ?");
        for (int i = from; i < to; ++i) {
            const CatalogRow &r = candidates.at(i);
            q.addBindValue(cachePathKey(r.path));
            bool fresh = false;
            if (q.exec() && q.next()) {
                fresh = q.value(0).toLongLong() == r.srcSize
                        && q.value(1).toLongLong() == r.srcMtime
                        && q.value(2).toLongLong() == r.sidecarMtime
                        /*  AN UNREADABLE STUB IS NEVER FRESH. The stamp says the FILE has
                            not changed, which is not the question -- what changes for
                            these rows is WINNOW: a parser gains a format, a fallback is
                            added, and the file that could not be read last month can be
                            read now. Stamped fresh, such a row is skipped by every
                            future scan, so the fix never reaches it and the user rescans
                            to no effect. That is exactly what happened when HEIC files
                            with no Exif were first recorded and then taught to parse.

                            The cost is one re-attempt per unreadable file per scan, and
                            the population is by definition tiny -- a library where it is
                            not is a library whose owner needs to know. */
                        && !q.value(3).toBool();
            }
            q.finish();
            if (!fresh) stale.insert(r.path);
        }
    }
    return stale;
}

QSet<QString> Catalog::outOfDate(const QList<CatalogRow> &candidates)
{
/*
    See the declaration for why this is not staleOf(): an unindexed path is NOT
    reported here.
*/
    QSet<QString> stale;
    if (candidates.isEmpty()) return stale;

    for (int from = 0; from < candidates.size(); from += kPageRows) {
        const int to = qMin(candidates.size(), from + kPageRows);

        QMutexLocker lk(&mutex);
        QSqlDatabase db = dbLocked();
        /*  No catalog means nothing was served from it, so nothing can be out of date.
            This is the other half of the difference from staleOf, which calls
            everything stale in the same situation. */
        if (!db.isOpen()) return stale;

        QSqlQuery q(db);
        q.prepare("SELECT srcsize, srcmtime, sidecarmtime FROM image WHERE pathkey = ?");
        for (int i = from; i < to; ++i) {
            const CatalogRow &r = candidates.at(i);
            if (r.path.isEmpty()) continue;
            q.addBindValue(cachePathKey(r.path));
            if (!q.exec() || !q.next()) { q.finish(); continue; }   // not indexed
            const bool fresh = q.value(0).toLongLong() == r.srcSize
                               && q.value(1).toLongLong() == r.srcMtime
                               && q.value(2).toLongLong() == r.sidecarMtime;
            q.finish();
            if (!fresh) stale.insert(r.path);
        }
    }
    return stale;
}

QString Catalog::availabilityLabel(int code)
{
    switch (code) {
    case int(Availability::Offline):    return "Offline";
    case int(Availability::Missing):    return "Missing";
    case int(Availability::Unreadable): return "Unreadable";
    default:                            return "Present";
    }
}

QStringList Catalog::monthLabels()
{
    static const QStringList names = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                      "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    return names;
}

QString Catalog::monthLabel(int month)
{
    if (month < 1 || month > 12) return QString();
    return monthLabels().at(month - 1);
}

int Catalog::availabilityCode(const QString &label)
{
    if (label == "Offline") return int(Availability::Offline);
    if (label == "Missing") return int(Availability::Missing);
    if (label == "Unreadable") return int(Availability::Unreadable);
    return int(Availability::Present);
}

QHash<QString, CatalogRow> Catalog::fetchFresh(const QList<CatalogRow> &candidates)
{
/*
    staleOf() read the other way round -- see the declaration for why both exist.

    ONE PREPARED STATEMENT REUSED, not one query per path built from scratch, and no
    "WHERE pathkey IN (...)": a folder of 5,000 images would put 5,000 bound values in
    one statement, and SQLite's parameter limit is smaller than that on some builds. The
    lookup is on the primary key, so the loop is 5,000 index seeks, which is what an IN
    would have compiled to anyway.

    THE KEYWORDS COME BACK TOO, and they are the reason this is worth doing at all: they
    are the expensive part of a metadata read, because they live in the sidecar rather
    than in the file's own header, and reconstructing them here is two more indexed
    joins rather than opening and parsing an XML document per image.
*/
    QHash<QString, CatalogRow> out;
    if (candidates.isEmpty()) return out;

    QMutexLocker lk(&mutex);
    QSqlDatabase db = dbLocked();
    /*  No catalog means nothing is fresh: the caller reads every file, which is exactly
        what it did before this existed. */
    if (!db.isOpen()) return out;

    QSqlQuery q(db);
    /*  AND NOT AN UNREADABLE STUB. Such a row holds a filename and stamps and no
        metadata at all, so serving it as fresh would hand the loader an empty row and
        stop it reading the file -- and it is the file read that would discover the
        format is now supported. staleOf makes the same exclusion, and the comment below
        says why the two must agree. */
    q.prepare(QString("SELECT") + kRowColumns
              + " FROM image i WHERE i.pathkey = ? AND i.live = 1"
                " AND i.unreadable = 0");

    /*  k.path, not k.name. This list becomes CatalogRow::keywords and from there the
        datamodel's own keyword column, so a leaf here would make a row served from the
        index filter differently from the same row read from its file. */
    QSqlQuery kw(db);
    kw.prepare("SELECT k.path FROM keyword k"
               " JOIN image_keyword ik ON ik.keyword_id = k.id"
               " WHERE ik.image_id = ?");

    for (const CatalogRow &cand : candidates) {
        if (cand.path.isEmpty()) continue;
        q.addBindValue(cachePathKey(cand.path));
        if (!q.exec() || !q.next()) { q.finish(); continue; }

        /*  THE SAME COMPARISON staleOf MAKES, and it has to stay the same: a row this
            says is fresh is a row the loader will not read, and a row staleOf says is
            fresh is a row the scanner will not index. If they ever disagree, an image
            can be both skipped and unindexed -- invisible, and permanently so, because
            nothing would revisit it until its file changed. */
        const bool fresh = q.value(1).toLongLong() == cand.srcSize
                           && q.value(2).toLongLong() == cand.srcMtime
                           && q.value(3).toLongLong() == cand.sidecarMtime;
        if (!fresh) { q.finish(); continue; }

        CatalogRow r;
        const qint64 id = readRow(q, r);
        /*  The path AND ITS STAMPS AS THE CALLER SPELLED THEM, overwriting what readRow
            took from the database. The two are the same file but not always the same
            string, and the caller looks the result up by what it passed in. */
        r.path = cand.path;
        r.srcSize = cand.srcSize;
        r.srcMtime = cand.srcMtime;
        r.sidecarMtime = cand.sidecarMtime;
        q.finish();

        kw.addBindValue(id);
        if (kw.exec()) while (kw.next()) r.keywords << kw.value(0).toString();
        kw.finish();

        out.insert(cand.path, r);
    }
    return out;
}

QHash<QString, Catalog::Availability> Catalog::availabilityOf(const QStringList &paths)
{
/*
    See the declaration for what the three states mean and why Offline is
    computed rather than stored.
*/
    QHash<QString, Availability> out;
    if (paths.isEmpty()) return out;

    /*  ONE mount-table walk, taken before the lock so the syscalls are not made
        with the catalog held. */
    const MountSnapshot mounts = MountSnapshot::take();

    /*  PAGED, TAKING THE LOCK PER PAGE -- kPageRows, the same convention staleOf and the
        commit passes follow, and for the reason stated where it is defined: "small enough
        that a Search on the GUI thread never waits on more than a few hundred".

        THIS FUNCTION HELD THE LOCK FOR THE WHOLE LIST, which was harmless while a caller
        asked about a folder's worth of paths and became a 30-second freeze the moment a
        catalog scope asked about 42,979 of them. The queries run off the GUI thread, so
        the pass itself was never the problem -- what blocked was every GUI-thread call
        INTO the catalog (FilterPanel::refresh, updateCatalogScopeTrees, the dock becoming
        visible) waiting on a mutex held for one query per row. Measured from a person's
        click: GUI STALL 30,689 ms, beginning the instant the load completed, with every
        stage of the load itself under 30 ms.

        A page boundary is a fine place to be interrupted: each path's answer is
        independent, and the caller applies them as a set afterwards. */
    for (int from = 0; from < paths.size(); from += kPageRows) {
        const int to = qMin(paths.size(), from + kPageRows);

        QMutexLocker lk(&mutex);
        QSqlDatabase db = dbLocked();
        if (!db.isOpen()) return out;

        QSqlQuery q(db);
        q.prepare("SELECT live, vol, unreadable FROM image WHERE pathkey = ?");
        for (int i = from; i < to; ++i) {
            const QString &p = paths.at(i);
            if (p.isEmpty()) continue;
            q.addBindValue(cachePathKey(p));
            if (!q.exec() || !q.next()) { q.finish(); continue; }   // not indexed
            const bool live = q.value(0).toBool();
            const QString vol = q.value(1).toString();
            const bool unreadable = q.value(2).toBool();
            q.finish();

            /*  THE VOLUME IS ASKED FIRST, and it has to be. A row can be marked not
                live from a sweep taken while the drive WAS mounted, and then the
                drive is unplugged: the file is missing AND the volume is absent. The
                useful thing to say then is "that disk isn't plugged in", because
                that is the one the user can act on -- and because until it is back
                there is no way to know whether the file is still gone. */
            /*  ORDER IS BY WHAT THE USER CAN ACT ON. The volume first, for the reason
                above; then missing, which is a fact about the file; then unreadable,
                which is a fact about the file's FORMAT and only meaningful once the file
                is known to be there. */
            if (!mounts.isMounted(vol)) out.insert(p, Availability::Offline);
            else if (!live)             out.insert(p, Availability::Missing);
            else if (unreadable)        out.insert(p, Availability::Unreadable);
            else                        out.insert(p, Availability::Present);
        }
    }
    return out;
}

/* ---------------------------------------------------------------------------------
   Search
   --------------------------------------------------------------------------------- */

void Catalog::buildQueryLocked(const CatalogQuery &cq, QString &from,
                               QStringList &where, QVariantList &binds)
{
/*
    THE PREDICATE, shared by every query that answers a CatalogQuery.

    search() returns paths and searchRows() returns whole rows, but "which images does
    this query match" must mean exactly one thing or the two would answer differently
    from the same search box -- and the count beside the result would then describe a
    different set from the rows on screen.

    EVERY VALUE IS BOUND, never interpolated -- including the FTS expression. The search
    box is user text and the catalog shares its database with the preview index, so a
    query that pasted text into SQL would put the previews one apostrophe away from a
    syntax error and worse.

    Caller holds the mutex; this touches no connection of its own.
*/
    from = " FROM image i";

    /* The SAME grammar the Filters search box uses (Utilities/searchterms.h), so "heron
       OR eagle" narrows here exactly as it narrows there. Parsing is what the two search
       boxes now share; only the compilation differs. */
    const SearchTerms terms = SearchTerms::parse(cq.text);

    const QString fts = terms.positiveFts();
    if (!fts.isEmpty()) {
        from += " JOIN image_fts f ON f.rowid = i.id";
        where << "image_fts MATCH ?";
        binds << fts;
    }

    const QString notFts = terms.negativeFts();
    if (!notFts.isEmpty()) {
        /* A NOT EXISTS over a SEPARATE fts5 lookup rather than FTS5's own NOT operator,
           which is binary: "-heron" on its own has no left-hand side to subtract from,
           and MATCH offers no "everything" token to supply one. As a subquery it works
           whether or not there is anything positive to go with it. */
        where << "NOT EXISTS (SELECT 1 FROM image_fts nf"
                 " WHERE nf.rowid = i.id AND nf.image_fts MATCH ?)";
        binds << notFts;
    }

    if (!cq.keywords.isEmpty()) {
        /* OR-ed, matching what checking several items in one Filters category does. No
           subtree walk and no LIKE: every image is linked to every ANCESTOR PREFIX of
           every path it carries, so checking "Fauna" reaches everything beneath it by
           plain indexed equality on pathfold. The expansion is done once at write time
           (keywordPrefixExpand) precisely so this stays an equality test. */
        from += " JOIN image_keyword ik ON ik.image_id = i.id"
                " JOIN keyword k ON k.id = ik.keyword_id";
        QStringList marks;
        for (const QString &k : cq.keywords) {
            if (k.trimmed().isEmpty()) continue;
            marks << "?";
            binds << fold(k);
        }
        if (!marks.isEmpty())
            where << "k.pathfold IN (" + marks.join(",") + ")";
    }

    if (!cq.excludeKeywords.isEmpty()) {
        /* AND-NOT, as a NOT EXISTS rather than a join: joining would multiply the rows
           and then need DISTINCT to undo it, and "this image has no such keyword" is a
           question about the image, not about a row to return.

           Path identity removed the ambiguity this used to resolve, but not the need for
           it: excluding a branch is how "everything under Fauna except Fauna|Bird" is
           expressed, and that is a question a tree raises rather than answers. */
        QStringList marks;
        QVariantList xbinds;
        for (const QString &k : cq.excludeKeywords) {
            if (k.trimmed().isEmpty()) continue;
            marks << "?";
            xbinds << fold(k);
        }
        if (!marks.isEmpty()) {
            where << "NOT EXISTS (SELECT 1 FROM image_keyword xik"
                     " JOIN keyword xk ON xk.id = xik.keyword_id"
                     " WHERE xik.image_id = i.id"
                     " AND xk.pathfold IN (" + marks.join(",") + "))";
            binds += xbinds;
        }
    }

    /* The generic CATEGORY restriction. Values within a column are OR-ed and columns
       AND-ed, which is exactly what checking several items in one Filters category and
       then checking a second category means -- the two scopes must narrow the same way
       from the same checkboxes. */
    /* text() on every bound value, because the BLANK category item is a value the user
       can check and a null QString binds as SQL NULL -- "NULL IN (NULL)" is NULL, so the
       blank row would select nothing at all. categorySql's IFNULL puts the column side at
       '', and this puts the bound side there too. */
    for (auto it = cq.include.constBegin(); it != cq.include.constEnd(); ++it) {
        const QString expr = categorySql(it.key());
        if (expr.isEmpty() || it.value().isEmpty()) continue;
        QStringList marks;
        for (const QString &v : it.value()) { marks << "?"; binds << text(v); }
        where << "(" + expr + ") IN (" + marks.join(",") + ")";
    }
    for (auto it = cq.exclude.constBegin(); it != cq.exclude.constEnd(); ++it) {
        const QString expr = categorySql(it.key());
        if (expr.isEmpty() || it.value().isEmpty()) continue;
        QStringList marks;
        for (const QString &v : it.value()) { marks << "?"; binds << text(v); }
        /* NOT IN, not "<> each": an exclusion subtracts the listed values and must leave
           everything else -- including rows whose value is empty. */
        where << "(" + expr + ") NOT IN (" + marks.join(",") + ")";
    }

    if (cq.minRating > 0)      { where << "i.rating >= ?";  binds << cq.minRating; }
    if (!cq.label.isEmpty())   { where << "i.label = ?";    binds << cq.label; }
    if (!cq.model.isEmpty())   { where << "i.model = ?";    binds << cq.model; }
    if (!cq.lens.isEmpty())    { where << "i.lens = ?";     binds << cq.lens; }
    if (cq.from.isValid()) {
        where << "i.captured >= ?";
        binds << cq.from.toSecsSinceEpoch();
    }
    if (cq.to.isValid()) {
        where << "i.captured <= ?";
        binds << cq.to.toSecsSinceEpoch();
    }
    if (!cq.folder.isEmpty()) {
        where << "(i.folder = ? OR i.folder LIKE ?)";
        binds << cq.folder << (cq.folder + "/%");
    }
    if (!cq.includeMissing) where << "i.live = 1";

}

QStringList Catalog::search(const CatalogQuery &cq, int limit, int *total)
{
/*
    The matching paths, newest first. See buildQueryLocked for the predicate.
*/
    QStringList out;
    if (total) *total = 0;

    QMutexLocker lk(&mutex);
    QSqlDatabase db = dbLocked();
    if (!db.isOpen()) return out;

    QString from;
    QStringList where;
    QVariantList binds;
    buildQueryLocked(cq, from, where, binds);

    QString sql = "SELECT DISTINCT i.path, i.captured" + from;
    if (!where.isEmpty()) sql += " WHERE " + where.join(" AND ");
    /* Newest first, and id as the tie-break so the order is stable between runs -- an
       unstable order would reshuffle the grid every time the same search is repeated. */
    sql += " ORDER BY i.captured DESC, i.id DESC";
    if (limit > 0) sql += " LIMIT ?";

    QSqlQuery q(db);
    q.prepare(sql);
    for (const QVariant &b : binds) q.addBindValue(b);
    if (limit > 0) q.addBindValue(limit);

    if (!q.exec()) {
        /* A malformed MATCH is the expected failure here -- the user is mid-way through
           typing FTS syntax -- so this is a quiet miss, not a warning popped in their
           face on a keystroke. */
        return out;
    }
    while (q.next()) out << q.value(0).toString();

    if (total) {
        QString csql = "SELECT COUNT(DISTINCT i.id)" + from;
        if (!where.isEmpty()) csql += " WHERE " + where.join(" AND ");
        QSqlQuery c(db);
        c.prepare(csql);
        for (const QVariant &b : binds) c.addBindValue(b);
        if (c.exec() && c.next()) *total = c.value(0).toInt();
    }
    return out;
}

QVector<CatalogRow> Catalog::searchRows(const CatalogQuery &cq, int limit, int *total)
{
/*
    The matching images as WHOLE ROWS rather than paths -- everything a datamodel row
    displays, from the same predicate and in the same order as search().

    WHY IT EXISTS. Loading a catalog result used to mean search() for the paths and then
    a metadata read per file to fill each row; with the index able to answer for a row
    outright (see IndexMetadata), the read became fetchFresh path-by-path instead. That is
    still one indexed seek per image -- measured at 42.6 us/row against 1.6 us/row for the
    search itself, so on a 43,000-image catalog the lookups cost 1.8 s and the query that
    found them cost 68 ms. This asks for the rows in the query that already knows which
    rows they are.

    IT DOES NOT CHECK FRESHNESS, and that is the difference from fetchFresh rather than an
    oversight. Freshness needs the file's size and mtime, so every candidate must be
    stat'd before the question can even be asked; browsing does not need it, because a
    row's stamps are checked when it is actually looked at. Callers that must know a row
    is current still go through fetchFresh -- this one trusts the index and says so.

    THE KEYWORDS COME BACK IN ONE QUERY, not one per image. The same predicate is reused
    as a subquery, so the join runs over exactly the images being returned. Per-image
    keyword lookups were the other half of fetchFresh's cost, and at 43,000 rows they are
    43,000 round trips to save a single join.
*/
    QVector<CatalogRow> out;
    if (total) *total = 0;

    QMutexLocker lk(&mutex);
    QSqlDatabase db = dbLocked();
    if (!db.isOpen()) return out;

    QString from;
    QStringList where;
    QVariantList binds;
    buildQueryLocked(cq, from, where, binds);

    const QString whereSql = where.isEmpty() ? QString()
                                             : " WHERE " + where.join(" AND ");
    /* The SAME order as search(): newest first, id as the tie-break so repeating a
       search does not reshuffle the grid. */
    const QString orderSql = " ORDER BY i.captured DESC, i.id DESC";
    const QString limitSql = limit > 0 ? QString(" LIMIT ?") : QString();

    QSqlQuery q(db);
    q.prepare("SELECT DISTINCT" + QString(kRowColumns) + from + whereSql + orderSql
              + limitSql);
    for (const QVariant &b : binds) q.addBindValue(b);
    if (limit > 0) q.addBindValue(limit);

    if (!q.exec()) {
        /* A malformed MATCH is the expected failure -- the user is mid-way through typing
           FTS syntax -- so this is a quiet miss, as in search(). */
        return out;
    }

    /* image id -> its position in out, so the keyword pass can attach names without
       searching the vector once per row. */
    QHash<qint64, int> byId;
    while (q.next()) {
        CatalogRow r;
        const qint64 id = readRow(q, r);
        byId.insert(id, out.size());
        out.append(r);
    }
    q.finish();

    if (out.isEmpty()) return out;

    QSqlQuery kw(db);
    kw.prepare("SELECT ik.image_id, k.path"
               " FROM image_keyword ik"
               " JOIN keyword k ON k.id = ik.keyword_id"
               " WHERE ik.image_id IN (SELECT DISTINCT i.id" + from + whereSql
               + orderSql + limitSql + ")");
    /* The predicate is bound a SECOND time, for the subquery. Rebuilding it would risk
       the two drifting; re-binding the same list cannot. */
    for (const QVariant &b : binds) kw.addBindValue(b);
    if (limit > 0) kw.addBindValue(limit);
    if (kw.exec()) {
        while (kw.next()) {
            const auto it = byId.constFind(kw.value(0).toLongLong());
            if (it != byId.constEnd()) out[it.value()].keywords << kw.value(1).toString();
        }
    }
    kw.finish();

    if (total) {
        QString csql = "SELECT COUNT(DISTINCT i.id)" + from + whereSql;
        QSqlQuery c(db);
        c.prepare(csql);
        for (const QVariant &b : binds) c.addBindValue(b);
        if (c.exec() && c.next()) *total = c.value(0).toInt();
    }
    return out;
}

QList<CatalogKeyword> Catalog::keywords()
{
/*
    The whole keyword vocabulary with image counts -- what the category lists render.

    ONE ROW PER PATH, and STILL NO SUMMING OF CHILDREN. That is not because the vocabulary
    is flat -- it is not, any more -- but because every image is linked to every ancestor
    prefix it carries, so an ancestor's count is already the total of its subtree. The
    hierarchy costs nothing to count, which is the whole point of expanding at write time.
*/
    QList<CatalogKeyword> out;

    QMutexLocker lk(&mutex);
    QSqlDatabase db = dbLocked();
    if (!db.isOpen()) return out;

    /*  ORDERED BY PATHFOLD, which is also depth-first order for a tree: a parent sorts
        immediately before its children because its path is their prefix. A caller
        building a tree can therefore consume this in one pass, and a caller showing a
        list gets the branches grouped rather than the leaves interleaved. */
    QSqlQuery q(db);
    if (!q.exec("SELECT k.name, k.path, COUNT(ik.image_id)"
                " FROM keyword k"
                " LEFT JOIN image_keyword ik ON ik.keyword_id = k.id"
                " LEFT JOIN image i ON i.id = ik.image_id AND i.live = 1"
                " GROUP BY k.id"
                " ORDER BY k.pathfold")) {
        return out;
    }
    while (q.next()) {
        CatalogKeyword k;
        k.name = q.value(0).toString();
        k.path = q.value(1).toString();
        k.count = q.value(2).toInt();
        out << k;
    }

    return out;
}

QMap<QString, int> Catalog::categoryItems(int dmColumn)
{
/*
    Every distinct value of one category, with how many live images carry it.

    THE KEYWORDS CATEGORY IS A JOIN, everything else is a GROUP BY on the image row. That
    is the only structural difference between them, and it is why the switch below has two
    arms rather than one generic query.

    THE BLANK VALUE IS A CATEGORY ITEM. A single-valued category has to add up to the
    catalog: if 43,064 images are indexed and 3,000 carry a rating, the ratings list says
    3,000 rated and 40,064 blank, not 3,000 and an unexplained shortfall. categorySql's
    IFNULL folds NULL into '' so the GROUP BY produces that row for free, and checking it
    means "the ones with nothing here" -- which is exactly what the datamodel side of the
    Filters panel has always offered, since its per-row QMap counts the empty string like
    any other key. Keywords are the exception and get no blank row: an image carries many,
    so the counts overlap and cannot sum to anything, and the datamodel side does not
    offer one either.

    COUNTS ARE UNFILTERED -- the whole catalog, not the current query. Per-item counts
    under the live query would be one GROUP BY per category on every keystroke over a
    quarter of a million rows, which is exactly the shape the debounce exists to avoid.
    The Filter dock therefore leaves the filtered column blank in Catalog scope and says
    so, rather than showing a number that is quietly the wrong one.
*/
    QMap<QString, int> out;

    QMutexLocker lk(&mutex);
    QSqlDatabase db = dbLocked();
    if (!db.isOpen()) return out;

    QSqlQuery q(db);
    if (dmColumn == G::KeywordsAllColumn) {
        /*  THE PATH, not the leaf name: it is the identity, it is what the filter binds
            and what Filters stores as the item's value, and two keywords can share a
            leaf. */
        if (!q.exec("SELECT k.path, COUNT(ik.image_id)"
                    " FROM keyword k"
                    " LEFT JOIN image_keyword ik ON ik.keyword_id = k.id"
                    " LEFT JOIN image i ON i.id = ik.image_id AND i.live = 1"
                    " GROUP BY k.id"
                    " HAVING COUNT(ik.image_id) > 0"
                    " ORDER BY k.pathfold")) {
            return out;
        }
    }
    else {
        const QString expr = categorySql(dmColumn);
        if (expr.isEmpty()) return out;         // the index cannot answer this one
        if (!q.exec("SELECT " + expr + " AS v, COUNT(*)"
                    " FROM image i WHERE i.live = 1"
                    " GROUP BY v ORDER BY v")) {
            return out;
        }
    }

    const bool isKeywords = dmColumn == G::KeywordsAllColumn;
    while (q.next()) {
        const QString v = q.value(0).toString();
        /* A blank keyword name is not a value, it is a bad row. */
        if (isKeywords && v.isEmpty()) continue;
        out.insert(v, q.value(1).toInt());
    }
    return out;
}

int Catalog::imagesUnderKeyword(const QString &path, const QString &folder)
{
/*
    A HALF-OPEN RANGE ON pathfold, not a LIKE and not a recursive CTE.

        pathfold = :p  OR  (pathfold >= :p||'|'  AND  pathfold < :p||'}')

    '}' is 0x7D, one past '|' (0x7C), so the second clause is exactly "every path that
    begins with this one followed by a separator". A range on the UNIQUE keyword_pathkey
    index is guaranteed index-driven; LIKE 'x%' is only index-driven when SQLite
    decides it can be, which depends on collation and on the pattern being a literal.

    THE SEPARATOR IS WHY THE RANGE HAS TWO CLAUSES. Without it, a range from "fauna" would
    also cover "faunal" and "fauna zoo" -- neither of which is beneath Fauna. Same trap
    keywordIsDescendant guards in memory, and the two must agree or the dialog reports a
    number the operation does not go on to touch.
*/
    if (path.trimmed().isEmpty()) return 0;

    QMutexLocker lk(&mutex);
    QSqlDatabase db = dbLocked();
    if (!db.isOpen()) return 0;

    const QString p = fold(path);

    QString sql = "SELECT COUNT(DISTINCT ik.image_id)"
                  " FROM image_keyword ik"
                  " JOIN keyword k ON k.id = ik.keyword_id"
                  " JOIN image i   ON i.id = ik.image_id AND i.live = 1"
                  " WHERE (k.pathfold = ?"
                  "     OR (k.pathfold >= ? AND k.pathfold < ?))";
    if (!folder.isEmpty()) sql += " AND i.folder = ?";

    QSqlQuery q(db);
    q.prepare(sql);
    q.addBindValue(p);
    q.addBindValue(p + "|");
    q.addBindValue(p + "}");
    if (!folder.isEmpty()) q.addBindValue(folder);

    if (q.exec() && q.next()) return q.value(0).toInt();
    return 0;
}

QList<CatalogKeyword> Catalog::flatKeywords(const QString &folder)
{
/*
    See the header: a flat keyword is one an image carries as a ROOT, not a keyword row
    without a separator. The distinction is the whole reason this is a scan.

    TWO COLUMNS, NO JOIN. The verbatim lists are on the image row, so the query reads
    keywords_literal and keywordpaths and nothing else -- the fastest shape available for
    a question that has to look at every image anyway.
*/
    QList<CatalogKeyword> out;

    QMutexLocker lk(&mutex);
    QSqlDatabase db = dbLocked();
    if (!db.isOpen()) return out;

    QString sql = "SELECT keywords_literal, keywordpaths FROM image"
                  " WHERE live = 1 AND keywords_literal <> ''";
    if (!folder.isEmpty()) sql += " AND folder = ?";

    QSqlQuery q(db);
    q.prepare(sql);
    if (!folder.isEmpty()) q.addBindValue(folder);
    if (!q.exec()) return out;

    /*  Folded name -> (spelling as first seen, count). Two files spelling one keyword
        "heron" and "Heron" are ONE flat keyword, because everything downstream -- the
        merge, the vocabulary lookup, keywordEffectivePaths itself -- matches folded. */
    QHash<QString, QPair<QString, int>> tally;
    while (q.next()) {
        CatalogRow r;
        const QString kl = q.value(0).toString();
        const QString kp = q.value(1).toString();
        if (!kl.isEmpty()) r.keywordsLiteral = kl.split('\n', Qt::SkipEmptyParts);
        if (!kp.isEmpty()) r.keywordPaths = kp.split('\n', Qt::SkipEmptyParts);
        for (const QString &f : flatOf(r)) {
            auto &e = tally[keywordFold(f)];
            if (e.second == 0) e.first = f;
            e.second++;
        }
    }

    for (auto it = tally.constBegin(); it != tally.constEnd(); ++it) {
        CatalogKeyword k;
        k.name = it.value().first;
        k.path = it.value().first;      // a flat keyword IS its own path
        k.count = it.value().second;
        out << k;
    }
    /* Biggest first: the legacy tags worth dealing with are the ones on thousands of
       images, and a 1,000-row list sorted alphabetically buries them. */
    std::sort(out.begin(), out.end(),
              [](const CatalogKeyword &a, const CatalogKeyword &b) {
                  if (a.count != b.count) return a.count > b.count;
                  return a.path.compare(b.path, Qt::CaseInsensitive) < 0;
              });
    return out;
}

QVector<CatalogRow> Catalog::flatKeywordRows(const QString &folder)
{
/*
    Every image with at least one flat keyword, as WHOLE rows.

    WHOLE ROWS BECAUSE THE CALLER MUST COMMIT THEM BACK. An image the tidy rewrites is
    usually not loaded -- a merge near the root of the vocabulary reaches folders the
    datamodel has never seen -- so its index entry is updated by committing the row it
    came from with new keywords. commit() replaces the row, so a partial row would erase
    everything it did not carry.

    ONE PASS, FILTERED HERE. Asking instead for images matching each flat keyword would
    be one query per keyword over an OR-ed join, and would return every image linked to
    that name as an ANCESTOR -- a superset that then has to be filtered by this same test
    anyway. This walks the images once and keeps the ones that qualify.
*/
    QVector<CatalogRow> out;

    QMutexLocker lk(&mutex);
    QSqlDatabase db = dbLocked();
    if (!db.isOpen()) return out;

    QString sql = QString("SELECT") + kRowColumns + " FROM image i"
                  " WHERE i.live = 1 AND i.keywords_literal <> ''";
    if (!folder.isEmpty()) sql += " AND i.folder = ?";

    QSqlQuery q(db);
    q.prepare(sql);
    if (!folder.isEmpty()) q.addBindValue(folder);
    if (!q.exec()) return out;

    while (q.next()) {
        CatalogRow r;
        readRow(q, r);
        if (flatOf(r).isEmpty()) continue;
        /*  r.keywords -- the prefix expansion -- is deliberately NOT fetched. The caller
            recomputes it from the keywords it is about to write, exactly as
            MW::retagKeywordPath does, so a joined copy of the OLD expansion would only
            be a chance to commit it back by mistake. */
        out << r;
    }
    return out;
}

int Catalog::pruneUnusedKeywords()
{
/*
    See the header. One statement, and the memo has to go with it: keywordIds maps
    pathfold to a row id, so leaving it populated after deleting the rows would have the
    next commit link an image to an id that no longer exists -- silently, because the
    insert is skipped when the memo answers.
*/
    QMutexLocker lk(&mutex);
    QSqlDatabase db = dbLocked();
    if (!db.isOpen()) return 0;

    QSqlQuery q(db);
    if (!q.exec("DELETE FROM keyword"
                " WHERE id NOT IN (SELECT DISTINCT keyword_id FROM image_keyword)"))
        return 0;

    const int gone = q.numRowsAffected();
    if (gone > 0) keywordIds.clear();
    return gone;
}


int Catalog::count()
{
    QMutexLocker lk(&mutex);
    QSqlDatabase db = dbLocked();
    if (!db.isOpen()) return 0;
    QSqlQuery q(db);
    if (q.exec("SELECT COUNT(*) FROM image") && q.next()) return q.value(0).toInt();
    return 0;
}

int Catalog::folderCount()
{
    QMutexLocker lk(&mutex);
    QSqlDatabase db = dbLocked();
    if (!db.isOpen()) return 0;
    QSqlQuery q(db);
    if (q.exec("SELECT COUNT(DISTINCT folder) FROM image") && q.next())
        return q.value(0).toInt();
    return 0;
}

/* ---------------------------------------------------------------------------------
   Maintenance
   --------------------------------------------------------------------------------- */

int Catalog::reconcileFolder(const QString &folder, const QSet<QString> &present)
{
/*
    Demote every live row in this folder that the enumeration did not find. See the
    declaration for the precondition -- present must be a COMPLETE listing of the folder.

    NO STAT, deliberately, which is the whole difference from sweep(): the caller has just
    read the directory, so asking the filesystem again would be asking a question we were
    handed the answer to. That is what makes this cheap enough to run on every folder load
    rather than once a session.

    DEMOTE, NEVER DELETE, exactly as sweep does, and for the same reason: a row that comes
    back is promoted again by the next commit that sees the file, so a folder that was
    briefly unreadable costs a rescan rather than its catalogued keywords.

    A folder Winnow has never catalogued selects nothing and this is one indexed query.
*/
    if (folder.isEmpty()) return 0;

    struct Row { qint64 id; QString path; };
    QList<Row> live;
    {
        QMutexLocker lk(&mutex);
        QSqlDatabase db = dbLocked();
        if (!db.isOpen()) return 0;
        QSqlQuery q(db);
        q.prepare("SELECT id, path FROM image WHERE folder = ? AND live = 1");
        q.addBindValue(folder);
        if (!q.exec()) return 0;
        while (q.next()) live.append({q.value(0).toLongLong(), q.value(1).toString()});
    }
    if (live.isEmpty()) return 0;

    QList<qint64> gone;
    for (const Row &r : live) {
        if (!present.contains(r.path)) gone.append(r.id);
    }
    if (gone.isEmpty()) return 0;

    int demoted = 0;
    QMutexLocker lk(&mutex);
    QSqlDatabase db = dbLocked();
    if (!db.isOpen()) return 0;
    if (!db.transaction()) return 0;
    QSqlQuery u(db);
    u.prepare("UPDATE image SET live = 0 WHERE id = ?");
    for (qint64 id : gone) {
        u.addBindValue(id);
        if (u.exec()) ++demoted;
    }
    db.commit();
    return demoted;
}

int Catalog::commitUnreadable(const QVector<CatalogRow> &rows)
{
/*
    A STUB ROW PER FILE THE PARSERS COULD NOT READ. See the declaration for why they are
    in the index rather than left out of it.

    ONLY THE FACTS THAT SURVIVED. path, folder, filename, extension and the freshness
    stamp -- everything else in a catalog row comes from the parse that failed, and
    writing zeros into those columns would be inventing a photograph's metadata rather
    than recording that there is none.

    THE STAMP IS THE POINT OF STORING THEM. staleOf compares it like any other row, so the
    next scan does not re-attempt a file that has not changed; a file that IS changed --
    replaced, repaired, or a format Winnow later learns -- goes stale and is tried again,
    and the ordinary commit clears the flag when it succeeds.
*/
    if (rows.isEmpty()) return 0;

    /* Taken before the lock, like the ordinary commit does: the mount walk is syscalls
       and must not be made with the catalog held. */
    const MountSnapshot mounts = MountSnapshot::take();

    QMutexLocker lk(&mutex);
    QSqlDatabase db = dbLocked();
    if (!db.isOpen()) return 0;
    if (!db.transaction()) return 0;

    QSqlQuery sel(db);
    sel.prepare("SELECT id FROM image WHERE pathkey = ?");
    QSqlQuery upd(db);
    upd.prepare("UPDATE image SET path = ?, folder = ?, vol = ?, filename = ?, ext = ?,"
                " srcsize = ?, srcmtime = ?, sidecarmtime = ?, live = 1, unreadable = 1"
                " WHERE id = ?");
    QSqlQuery ins(db);
    ins.prepare("INSERT INTO image (pathkey, path, folder, vol, filename, ext,"
                " srcsize, srcmtime, sidecarmtime, live, unreadable)"
                " VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, 1, 1)");

    int written = 0;
    for (const CatalogRow &r : rows) {
        if (r.path.isEmpty()) continue;
        const QString key = cachePathKey(r.path);
        const QString vol = mounts.rootOf(r.path);

        qint64 id = 0;
        sel.addBindValue(key);
        if (sel.exec() && sel.next()) id = sel.value(0).toLongLong();
        sel.finish();

        QSqlQuery &q = id ? upd : ins;
        if (!id) q.addBindValue(key);
        q.addBindValue(r.path);
        q.addBindValue(r.folder);
        q.addBindValue(vol);
        q.addBindValue(r.filename);
        q.addBindValue(r.ext);
        q.addBindValue(r.srcSize);
        q.addBindValue(r.srcMtime);
        q.addBindValue(r.sidecarMtime);
        if (id) q.addBindValue(id);
        if (q.exec()) ++written;
        q.finish();
    }

    if (!db.commit()) { db.rollback(); return 0; }
    return written;
}

int Catalog::unreadableCount()
{
    QMutexLocker lk(&mutex);
    QSqlDatabase db = dbLocked();
    if (!db.isOpen()) return 0;
    QSqlQuery q(db);
    if (q.exec("SELECT COUNT(*) FROM image WHERE unreadable = 1") && q.next())
        return q.value(0).toInt();
    return 0;
}

namespace {

/* The WHERE clause for "this folder, or this folder and everything under it", and the
   values to bind to it. LIKE is given an explicit ESCAPE and the prefix is escaped into
   it, because a folder name may legally contain % or _ and an unescaped one would match
   folders it has nothing to do with. */
QString folderScopeClause(bool recurse)
{
    return recurse ? "(folder = ? OR folder LIKE ? ESCAPE '\\')" : "folder = ?";
}

void bindFolderScope(QSqlQuery &q, const QString &folder, bool recurse)
{
    q.addBindValue(folder);
    if (!recurse) return;
    QString prefix = folder;
    prefix.replace("\\", "\\\\").replace("%", "\\%").replace("_", "\\_");
    q.addBindValue(prefix + "/%");
}

}  // namespace

int Catalog::countUnder(const QString &folder, bool recurse)
{
    if (folder.isEmpty()) return 0;
    QMutexLocker lk(&mutex);
    QSqlDatabase db = dbLocked();
    if (!db.isOpen()) return 0;
    QSqlQuery q(db);
    q.prepare("SELECT COUNT(*) FROM image WHERE " + folderScopeClause(recurse));
    bindFolderScope(q, folder, recurse);
    if (q.exec() && q.next()) return q.value(0).toInt();
    return 0;
}

int Catalog::forgetUnder(const QString &folder, bool recurse)
{
/*
    Delete every catalogued row under folder. See the declaration for why this deletes
    where the sweep demotes.

    THE FTS ROWS GO FIRST, and by subselect rather than by a second pass over the ids:
    image_fts has no foreign key onto image, so once the image rows are gone there is
    nothing left to say which FTS rows were theirs. image_keyword follows by cascade.
*/
    if (folder.isEmpty()) return 0;
    QMutexLocker lk(&mutex);
    QSqlDatabase db = dbLocked();
    if (!db.isOpen()) return 0;

    const QString where = folderScopeClause(recurse);
    db.transaction();
    QSqlQuery f(db);
    f.prepare("DELETE FROM image_fts WHERE rowid IN"
              " (SELECT id FROM image WHERE " + where + ")");
    bindFolderScope(f, folder, recurse);
    if (!f.exec()) { db.rollback(); return 0; }

    QSqlQuery d(db);
    d.prepare("DELETE FROM image WHERE " + where);
    bindFolderScope(d, folder, recurse);
    if (!d.exec()) { db.rollback(); return 0; }
    const int gone = d.numRowsAffected();
    db.commit();
    return gone > 0 ? gone : 0;
}

QMap<QString, int> Catalog::folderCounts()
{
/*
    Every catalogued folder with its image count -- see the declaration for why both
    facts come back from one query.
*/
    QMap<QString, int> out;
    QMutexLocker lk(&mutex);
    QSqlDatabase db = dbLocked();
    if (!db.isOpen()) return out;
    QSqlQuery q(db);
    if (!q.exec("SELECT folder, COUNT(*) FROM image GROUP BY folder")) return out;
    while (q.next()) out.insert(q.value(0).toString(), q.value(1).toInt());
    return out;
}

int Catalog::forgetFolders(const QStringList &folders)
{
/*
    Delete every catalogued row in these folders. See the declaration for why this takes
    a list rather than a prefix, and for what it deliberately leaves behind.

    ONE TRANSACTION, CHUNKED BINDS. SQLite's variable limit is finite and a library can
    hold thousands of folders, so the IN list is filled in batches -- but the batches sit
    inside a single transaction, because a reconcile that half succeeded would leave the
    catalog disagreeing with the scope table it was just made to match, which is the one
    state this whole path exists to abolish.

    THE FTS ROWS GO FIRST, and by subselect, for the same reason as forgetUnder:
    image_fts has no foreign key onto image, so once the image rows are gone there is
    nothing left to say which FTS rows were theirs.
*/
    if (folders.isEmpty()) return 0;
    QMutexLocker lk(&mutex);
    QSqlDatabase db = dbLocked();
    if (!db.isOpen()) return 0;

    constexpr int kChunk = 500;
    int gone = 0;
    db.transaction();
    for (int start = 0; start < folders.size(); start += kChunk) {
        const QStringList chunk = folders.mid(start, kChunk);
        QString marks;
        for (int i = 0; i < chunk.size(); ++i) marks += (i ? ",?" : "?");
        const QString where = " WHERE folder IN (" + marks + ")";

        QSqlQuery f(db);
        f.prepare("DELETE FROM image_fts WHERE rowid IN"
                  " (SELECT id FROM image" + where + ")");
        for (const QString &folder : chunk) f.addBindValue(folder);
        if (!f.exec()) { db.rollback(); return 0; }

        QSqlQuery d(db);
        d.prepare("DELETE FROM image" + where);
        for (const QString &folder : chunk) d.addBindValue(folder);
        if (!d.exec()) { db.rollback(); return 0; }
        if (d.numRowsAffected() > 0) gone += d.numRowsAffected();
    }
    db.commit();
    return gone;
}

int Catalog::sweep()
{
/*
    Mark rows whose source image is gone as not live, so a search stops offering images it
    cannot load.

    DEMOTE, NEVER DELETE, and skip anything on an unmounted volume -- the same rule as the
    devPreview sweep, for the same reason: Winnow browses memory cards and external drives
    constantly, and an ejected drive must not read as a mass deletion. A row that comes
    back is promoted again by the next Commit that sees the file.

    PAGED, with the mutex released between pages. This stats one file per row; at the
    250,000 rows the database is sized for, holding the lock throughout would block every
    Search for the duration, which is a frozen search box.
*/
    const MountSnapshot mounts = MountSnapshot::take();

    struct Row { qint64 id; QString path; QString vol; };
    int demoted = 0;
    qint64 after = 0;

    for (;;) {
        QList<Row> page;
        {
            QMutexLocker lk(&mutex);
            QSqlDatabase db = dbLocked();
            if (!db.isOpen()) return demoted;
            QSqlQuery q(db);
            q.prepare("SELECT id, path, vol FROM image"
                      " WHERE live = 1 AND id > ? ORDER BY id LIMIT ?");
            q.addBindValue(after);
            q.addBindValue(kPageRows);
            if (!q.exec()) return demoted;
            while (q.next()) {
                page.append({q.value(0).toLongLong(), q.value(1).toString(),
                             q.value(2).toString()});
            }
        }
        if (page.isEmpty()) break;
        after = page.last().id;

        /* The stats happen with the lock DROPPED. */
        QList<qint64> gone;
        for (const Row &r : page) {
            if (!mounts.isMounted(r.vol)) continue;   // ejected, not deleted
            if (!QFileInfo::exists(r.path)) gone.append(r.id);
        }
        if (gone.isEmpty()) continue;

        QMutexLocker lk(&mutex);
        QSqlDatabase db = dbLocked();
        if (!db.isOpen()) return demoted;
        if (!db.transaction()) continue;
        QSqlQuery u(db);
        u.prepare("UPDATE image SET live = 0 WHERE id = ?");
        for (qint64 id : gone) {
            u.addBindValue(id);
            if (u.exec()) ++demoted;
        }
        db.commit();
    }
    return demoted;
}

void Catalog::onMoved(const QString &srcPath, const QString &dstPath)
{
    if (srcPath.isEmpty() || dstPath.isEmpty()) return;

    QMutexLocker lk(&mutex);
    QSqlDatabase db = dbLocked();
    if (!db.isOpen()) return;

    const QFileInfo fi(dstPath);
    QSqlQuery q(db);
    q.prepare("UPDATE image SET pathkey = ?, path = ?, folder = ?, filename = ?,"
              " live = 1"
              " WHERE pathkey = ?");
    q.addBindValue(cachePathKey(dstPath));
    q.addBindValue(text(dstPath));
    q.addBindValue(text(fi.absoluteDir().path()));
    q.addBindValue(text(fi.fileName()));
    q.addBindValue(cachePathKey(srcPath));
    if (!q.exec()) {
        /* The destination may already be catalogued -- moving a file onto a path the
           catalog knows. The unique index refuses; drop the stale source row instead of
           leaving two rows claiming the same image. */
        QSqlQuery del(db);
        del.prepare("DELETE FROM image WHERE pathkey = ?");
        del.addBindValue(cachePathKey(srcPath));
        del.exec();
    }
}

void Catalog::onDeleted(const QString &fPath)
{
    if (fPath.isEmpty()) return;

    QMutexLocker lk(&mutex);
    QSqlDatabase db = dbLocked();
    if (!db.isOpen()) return;

    /* A delete Winnow performed itself is certain, unlike the sweep's inference from a
       missing file, so the row goes rather than being demoted. ON DELETE CASCADE takes
       the keyword links with it; the FTS row has no foreign key, so it goes by hand. */
    QSqlQuery id(db);
    id.prepare("SELECT id FROM image WHERE pathkey = ?");
    id.addBindValue(cachePathKey(fPath));
    if (id.exec() && id.next()) {
        const qint64 imageId = id.value(0).toLongLong();
        QSqlQuery f(db);
        f.prepare("DELETE FROM image_fts WHERE rowid = ?");
        f.addBindValue(imageId);
        f.exec();
        QSqlQuery d(db);
        d.prepare("DELETE FROM image WHERE id = ?");
        d.addBindValue(imageId);
        d.exec();
    }
}

void Catalog::clear()
{
    QMutexLocker lk(&mutex);
    QSqlDatabase db = dbLocked();
    if (!db.isOpen()) return;

    QSqlQuery q(db);
    /* image_keyword goes by cascade off image. The FTS table and the keyword vocabulary
       have no foreign key onto image, so they are cleared explicitly.

       vocab is NOT cleared: it is the user's AUTHORED tree, not derived data, and this
       clears the INDEX. Emptying the catalog must not throw away a vocabulary the user
       curated and imported -- the branches simply report zero images until something is
       indexed under them again. */
    q.exec("DELETE FROM image_fts");
    q.exec("DELETE FROM image");
    q.exec("DELETE FROM keyword");
    keywordIds.clear();
}
