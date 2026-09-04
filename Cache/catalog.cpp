#include "Cache/catalog.h"
#include "Cache/cachedb.h"
#include "Cache/mountsnapshot.h"
#include "Cache/pathkey.h"
#include "Main/global.h"
#include "Metadata/keywordflatten.h"
#include "Utilities/searchterms.h"

#include <QDir>
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
    The SQL that produces one category item's value, keyed by the datamodel column the
    Filters panel maps that category to.

    ONE MAP, USED BOTH WAYS -- categoryItems() lists the distinct values and search()
    compares against them -- so a category item the user checks cannot mean something
    different from the item that was offered. Two expressions would drift the first time
    one was edited.

    THE STRINGS MUST MATCH WHAT DataModel WRITES into the same column, because the Find
    dock shows one list and the user does not know which scope produced it: TypeColumn is
    the suffix UPPER-cased, YearColumn is "yyyy", DayColumn is "yyyy-MM-dd", FolderName is
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

qint64 Catalog::keywordIdLocked(QSqlDatabase &db, const QString &name)
{
/*
    The id for one keyword name, inserting it if it is new.

    KEYED ON THE NAME ALONE. Schema 3 keyed on (path, name), which meant a tag Lightroom
    wrote both ways -- "Heron" in dc:subject and "Fauna|Bird|Heron" in
    lr:hierarchicalSubject -- became two keyword rows for one tag, and so appeared twice
    in the category list with its image count split between the entries. The hierarchy is
    flattened before it reaches here (Metadata/keywordflatten.h), so both forms arrive as
    the same name and collapse onto one row.

    MEMOISED, because a folder of 2,000 images typically carries a few dozen DISTINCT
    keywords: without the memo this is two round trips per keyword per image, with it, two
    per keyword per session. The key is what the unique index is on, so the memo and the
    table can never disagree about what identifies a keyword.
*/
    const QString nameFold = fold(name);
    if (nameFold.isEmpty()) return 0;

    const auto it = keywordIds.constFind(nameFold);
    if (it != keywordIds.constEnd()) return it.value();

    QSqlQuery q(db);
    q.prepare("INSERT INTO keyword (name, namefold)"
              " VALUES (?, ?)"
              " ON CONFLICT(namefold) DO NOTHING");
    q.addBindValue(text(name.trimmed()));
    q.addBindValue(text(nameFold));

    if (!q.exec()) {
        G::issueDedup("Warning", "Catalog keyword insert failed: " + q.lastError().text(),
                      "Catalog::keywordIdLocked", -1, name);
        return 0;
    }

    qint64 id = q.lastInsertId().toLongLong();
    if (!id) {
        /* DO NOTHING fired: the row already existed (another folder, or a previous
           session), so look it up rather than treating a conflict as a failure. */
        QSqlQuery sel(db);
        sel.prepare("SELECT id FROM keyword WHERE namefold = ?");
        sel.addBindValue(nameFold);
        if (sel.exec() && sel.next()) id = sel.value(0).toLongLong();
    }
    if (id) keywordIds.insert(nameFold, id);
    return id;
}

void Catalog::writeKeywordsLocked(QSqlDatabase &db, qint64 imageId, const CatalogRow &r)
{
/*
    Replace this image's keyword links, and record which parents its hierarchical tags
    were seen under.

    DELETE-THEN-INSERT rather than a diff: an image's keyword list is a handful of rows,
    the delete is one indexed statement, and a diff would have to be right about removals
    to be worth anything. Removing a keyword in Lightroom must remove it here too, and
    this is what makes that fall out for free.

    r.keywords IS ALREADY FLAT. DataModel::catalogRows and CatalogScanner both hand over
    flattenKeywords()'s output -- dc:subject's leaves and every node of every hierarchical
    path, de-duplicated -- so there is nothing to walk here and no second form of the same
    tag to reconcile. An ancestor is an ordinary keyword in that list, which is what keeps
    a search for "Fauna" reaching an image tagged only "Fauna|Bird|Heron".

    CONTEXTS ARE NOT DELETED WITH THE LINKS. They describe the VOCABULARY -- that
    "Vancouver" has been seen under both Canada and USA -- not this image, and the fact
    stays true after this image is re-indexed or removed. Deleting them per image would
    make ambiguity flicker as folders are browsed.
*/
    QSqlQuery del(db);
    del.prepare("DELETE FROM image_keyword WHERE image_id = ?");
    del.addBindValue(imageId);
    del.exec();

    QSet<qint64> ids;
    for (const QString &k : r.keywords) {
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

    /* What the hierarchy leaves behind. Each adjacent pair in a path is one (child,
       parent) fact; a name with more than one distinct parent is ambiguous. */
    QSqlQuery ctx(db);
    ctx.prepare("INSERT OR IGNORE INTO keyword_context (keyword_id, parent_id)"
                " VALUES (?, ?)");
    for (const QString &path : r.keywordPaths) {
        const QStringList nodes = keywordNodes(path);
        for (int i = 1; i < nodes.size(); ++i) {
            const qint64 childId  = keywordIdLocked(db, nodes.at(i));
            const qint64 parentId = keywordIdLocked(db, nodes.at(i - 1));
            /* childId == parentId is a path like "A|A": a name is not its own parent,
               and recording it would read as an ambiguity that does not exist. */
            if (!childId || !parentId || childId == parentId) continue;
            ctx.addBindValue(childId);
            ctx.addBindValue(parentId);
            ctx.exec();
        }
    }
}

void Catalog::writeFtsLocked(QSqlDatabase &db, qint64 imageId, const CatalogRow &r)
{
/*
    The full-text row for this image. rowid == image.id, so the delete below is what keeps
    a re-index from leaving the old text behind and matching on words the image no longer
    carries.

    Both keyword forms go into the one column, and the hierarchical paths have their '|'
    replaced by spaces so every ancestor becomes its own token: that is what lets a free
    text search for "wildlife" hit an image tagged "Wildlife|Birds|Heron", matching what
    the Keywords category does through the ancestor rows.
*/
    QSqlQuery del(db);
    del.prepare("DELETE FROM image_fts WHERE rowid = ?");
    del.addBindValue(imageId);
    del.exec();

    QStringList kw = r.keywords;
    for (const QString &p : r.keywordPaths) kw << QString(p).replace('|', ' ');

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
    sel.prepare("SELECT id, srcsize, srcmtime, sidecarmtime FROM image"
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
                    && sel.value(3).toLongLong() == r.sidecarMtime;
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
        q.prepare("SELECT srcsize, srcmtime, sidecarmtime FROM image WHERE pathkey = ?");
        for (int i = from; i < to; ++i) {
            const CatalogRow &r = candidates.at(i);
            q.addBindValue(cachePathKey(r.path));
            bool fresh = false;
            if (q.exec() && q.next()) {
                fresh = q.value(0).toLongLong() == r.srcSize
                        && q.value(1).toLongLong() == r.srcMtime
                        && q.value(2).toLongLong() == r.sidecarMtime;
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
    case int(Availability::Offline): return "Offline";
    case int(Availability::Missing): return "Missing";
    default:                         return "Present";
    }
}

int Catalog::availabilityCode(const QString &label)
{
    if (label == "Offline") return int(Availability::Offline);
    if (label == "Missing") return int(Availability::Missing);
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
    q.prepare(QString("SELECT") + kRowColumns
              + " FROM image i WHERE i.pathkey = ? AND i.live = 1");

    QSqlQuery kw(db);
    kw.prepare("SELECT k.name FROM keyword k"
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
        INTO the catalog (FindPanel::refresh, updateCatalogScopeTrees, the dock becoming
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
        q.prepare("SELECT live, vol FROM image WHERE pathkey = ?");
        for (int i = from; i < to; ++i) {
            const QString &p = paths.at(i);
            if (p.isEmpty()) continue;
            q.addBindValue(cachePathKey(p));
            if (!q.exec() || !q.next()) { q.finish(); continue; }   // not indexed
            const bool live = q.value(0).toBool();
            const QString vol = q.value(1).toString();
            q.finish();

            /*  THE VOLUME IS ASKED FIRST, and it has to be. A row can be marked not
                live from a sweep taken while the drive WAS mounted, and then the
                drive is unplugged: the file is missing AND the volume is absent. The
                useful thing to say then is "that disk isn't plugged in", because
                that is the one the user can act on -- and because until it is back
                there is no way to know whether the file is still gone. */
            if (!mounts.isMounted(vol)) out.insert(p, Availability::Offline);
            else if (!live)             out.insert(p, Availability::Missing);
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
           subtree walk: the vocabulary is flat, so an ancestor name is linked to every
           image beneath it directly and plain equality already reaches them all. */
        from += " JOIN image_keyword ik ON ik.image_id = i.id"
                " JOIN keyword k ON k.id = ik.keyword_id";
        QStringList marks;
        for (const QString &k : cq.keywords) {
            if (k.trimmed().isEmpty()) continue;
            marks << "?";
            binds << fold(k);
        }
        if (!marks.isEmpty())
            where << "k.namefold IN (" + marks.join(",") + ")";
    }

    if (!cq.excludeKeywords.isEmpty()) {
        /* AND-NOT, as a NOT EXISTS rather than a join: joining would multiply the rows
           and then need DISTINCT to undo it, and "this image has no such keyword" is a
           question about the image, not about a row to return. This is how an ambiguous
           name is resolved -- keywords = {Vancouver}, excludeKeywords = {USA}. */
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
                     " AND xk.namefold IN (" + marks.join(",") + "))";
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
    kw.prepare("SELECT ik.image_id, k.name"
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
    The whole keyword vocabulary with image counts and parent names -- what the category
    lists render.

    ONE ROW PER NAME, because the vocabulary is flat. Counts come from image_keyword
    directly; there is no summing of children to do, since an ancestor is linked to every
    image beneath it in its own right.

    CONTEXTS ARE FETCHED IN A SECOND PASS rather than joined in. Joining keyword_context
    into the counting query would multiply each keyword row by its number of parents and
    inflate COUNT(), and getting that right needs a DISTINCT that costs more than the
    second query -- which reads a table the size of the vocabulary, not of the library.
*/
    QList<CatalogKeyword> out;

    QMutexLocker lk(&mutex);
    QSqlDatabase db = dbLocked();
    if (!db.isOpen()) return out;

    /* keyword id -> its position in out, so the second pass can attach contexts without
       searching the list once per row. */
    QHash<qint64, int> byId;

    QSqlQuery q(db);
    if (!q.exec("SELECT k.id, k.name, COUNT(ik.image_id)"
                " FROM keyword k"
                " LEFT JOIN image_keyword ik ON ik.keyword_id = k.id"
                " LEFT JOIN image i ON i.id = ik.image_id AND i.live = 1"
                " GROUP BY k.id"
                " ORDER BY k.namefold")) {
        return out;
    }
    while (q.next()) {
        CatalogKeyword k;
        k.name = q.value(1).toString();
        k.count = q.value(2).toInt();
        byId.insert(q.value(0).toLongLong(), out.size());
        out << k;
    }

    QSqlQuery c(db);
    if (c.exec("SELECT c.keyword_id, p.name"
               " FROM keyword_context c"
               " JOIN keyword p ON p.id = c.parent_id"
               " ORDER BY p.namefold")) {
        while (c.next()) {
            const auto it = byId.constFind(c.value(0).toLongLong());
            if (it != byId.constEnd()) out[it.value()].contexts << c.value(1).toString();
        }
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
    The Find dock therefore leaves the filtered column blank in Catalog scope and says
    so, rather than showing a number that is quietly the wrong one.
*/
    QMap<QString, int> out;

    QMutexLocker lk(&mutex);
    QSqlDatabase db = dbLocked();
    if (!db.isOpen()) return out;

    QSqlQuery q(db);
    if (dmColumn == G::KeywordsAllColumn) {
        if (!q.exec("SELECT k.name, COUNT(ik.image_id)"
                    " FROM keyword k"
                    " LEFT JOIN image_keyword ik ON ik.keyword_id = k.id"
                    " LEFT JOIN image i ON i.id = ik.image_id AND i.live = 1"
                    " GROUP BY k.id"
                    " HAVING COUNT(ik.image_id) > 0"
                    " ORDER BY k.namefold")) {
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

QSet<QString> Catalog::ambiguousKeywords()
{
/*
    The names recorded under more than one parent -- what flattening the hierarchy
    genuinely lost, and the only thing the docks colour differently.

    Returned FOLDED, because every caller is comparing against a keyword it got from
    somewhere else (a datamodel column, a category item) and folding at the point of
    comparison is the only way the two can agree about "Heron" and "heron".

    An empty result means EITHER nothing is ambiguous OR there is no catalog. Callers must
    not present the second as the first: with no index we do not know, and colouring
    nothing while implying we checked would be a quiet lie.
*/
    QSet<QString> out;

    QMutexLocker lk(&mutex);
    QSqlDatabase db = dbLocked();
    if (!db.isOpen()) return out;

    QSqlQuery q(db);
    if (!q.exec("SELECT k.namefold"
                " FROM keyword_context c"
                " JOIN keyword k ON k.id = c.keyword_id"
                " GROUP BY c.keyword_id"
                " HAVING COUNT(DISTINCT c.parent_id) > 1")) {
        return out;
    }
    while (q.next()) out.insert(q.value(0).toString());
    return out;
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
    /* image_keyword goes by cascade off image, and keyword_context by cascade off
       keyword. The FTS table and the keyword vocabulary have no foreign key onto image,
       so they are cleared explicitly. */
    q.exec("DELETE FROM image_fts");
    q.exec("DELETE FROM image");
    q.exec("DELETE FROM keyword");
    keywordIds.clear();
}
