#include "Cache/catalog.h"
#include "Cache/cachedb.h"
#include "Cache/mountsnapshot.h"
#include "Cache/pathkey.h"
#include "Main/global.h"

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

/* Separator for the memo key. \x1f (unit separator) cannot occur in a keyword. */
const QChar kKeySep(QChar(0x1f));

QString defaultCacheDir()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
           + "/PreviewCache";
}

/* Case folding, matching Cache/pathkey.h's reasoning: toCaseFolded is the Unicode-correct
   locale-independent operation, where toLower is neither. */
QString fold(const QString &s)
{
    return s.trimmed().toCaseFolded();
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
    Turn what the user typed into an FTS5 MATCH expression.

    Bare words become prefix terms AND-ed together, which is what a search box is expected
    to do ("heron nan" finds Heron in Nanaimo). Anything containing FTS syntax the user
    clearly meant -- a quote, a column filter, an explicit operator -- is passed through
    untouched so the syntax remains available to those who want it.

    Every bare term is QUOTED before the '*' is appended. Unquoted, a term containing any
    of FTS5's punctuation is a syntax error, and a syntax error in MATCH makes the whole
    query fail rather than return nothing -- so a user typing an apostrophe or a hyphen
    mid-word would see the search break instead of narrow.
*/
QString ftsExpression(const QString &raw)
{
    const QString t = raw.trimmed();
    if (t.isEmpty()) return QString();

    if (t.contains('"') || t.contains(':') || t.contains('(')
        || t.contains(" OR ") || t.contains(" NOT ") || t.contains(" AND ")) {
        return t;
    }

    QStringList terms;
    const auto parts = t.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
    for (const QString &p : parts) {
        QString q = p;
        q.replace('"', "\"\"");             // FTS5 escapes a quote by doubling it
        terms << ('"' + q + "\"*");
    }
    return terms.join(" AND ");
}

qint64 nowSecs()
{
    return QDateTime::currentSecsSinceEpoch();
}

/* Every ancestor prefix of a hierarchical keyword path, longest last.
   "A|B|C" -> ["A", "A|B", "A|B|C"]. */
QStringList ancestry(const QString &path)
{
    QStringList out;
    const auto parts = path.split('|', Qt::SkipEmptyParts);
    QString acc;
    for (const QString &p : parts) {
        const QString trimmed = p.trimmed();
        if (trimmed.isEmpty()) continue;
        acc = acc.isEmpty() ? trimmed : acc + '|' + trimmed;
        out << acc;
    }
    return out;
}

QString leafOf(const QString &path)
{
    const int i = path.lastIndexOf('|');
    return (i < 0 ? path : path.mid(i + 1)).trimmed();
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

qint64 Catalog::keywordIdLocked(QSqlDatabase &db, const QString &name,
                                const QString &path)
{
/*
    The id for one keyword, inserting it if it is new.

    MEMOISED, because a folder of 2,000 images typically carries a few dozen DISTINCT
    keywords: without the memo this is two round trips per keyword per image, with it, two
    per keyword per session. The key is the same pair the unique index is on, so the memo
    and the table can never disagree about what identifies a keyword.
*/
    const QString nameFold = fold(name);
    const QString pathFold = fold(path);
    if (nameFold.isEmpty()) return 0;

    const QString memo = pathFold + kKeySep + nameFold;
    const auto it = keywordIds.constFind(memo);
    if (it != keywordIds.constEnd()) return it.value();

    QSqlQuery q(db);
    q.prepare("INSERT INTO keyword (name, namefold, path, pathfold, parent)"
              " VALUES (?, ?, ?, ?, ?)"
              " ON CONFLICT(pathfold, namefold) DO NOTHING");
    q.addBindValue(text(name.trimmed()));
    q.addBindValue(text(nameFold));
    q.addBindValue(text(path));
    q.addBindValue(text(pathFold));

    /* The parent is the keyword one level up the hierarchy, which has already been
       inserted by writeKeywordsLocked walking the ancestry shortest-first. A flat
       keyword, or the root of a hierarchy, has none. */
    QVariant parent;
    const int cut = path.lastIndexOf('|');
    if (cut > 0) {
        const QString parentPath = path.left(cut);
        const qint64 pid = keywordIdLocked(db, leafOf(parentPath), parentPath);
        if (pid) parent = pid;
    }
    q.addBindValue(parent);

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
        sel.prepare("SELECT id FROM keyword WHERE pathfold = ? AND namefold = ?");
        sel.addBindValue(pathFold);
        sel.addBindValue(nameFold);
        if (sel.exec() && sel.next()) id = sel.value(0).toLongLong();
    }
    if (id) keywordIds.insert(memo, id);
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
*/
    QSqlQuery del(db);
    del.prepare("DELETE FROM image_keyword WHERE image_id = ?");
    del.addBindValue(imageId);
    del.exec();

    QSet<qint64> ids;

    /* Hierarchical first, shortest path first, so a parent is always inserted before the
       child that needs to point at it. Every ancestor is linked to the image as well as
       the leaf, which is what makes a search for "Location" find this picture. */
    QStringList paths = r.keywordPaths;
    std::sort(paths.begin(), paths.end(),
              [](const QString &a, const QString &b) { return a.length() < b.length(); });
    for (const QString &p : paths) {
        for (const QString &node : ancestry(p)) {
            const qint64 id = keywordIdLocked(db, leafOf(node), node);
            if (id) ids.insert(id);
        }
    }

    /* Flat keywords carry no hierarchy, so they are stored with path = ''. A keyword that
       appears both ways -- which is the normal Lightroom case, since it writes the leaf
       into dc:subject AND the path into lr:hierarchicalSubject -- therefore gets two
       keyword rows. That is deliberate: they are genuinely different facts ("tagged
       Heron" vs "tagged Fauna|Bird|Heron"), and collapsing them would lose the ability to
       tell a flat library from a hierarchical one. */
    for (const QString &k : r.keywords) {
        const qint64 id = keywordIdLocked(db, k, QString());
        if (id) ids.insert(id);
    }

    if (ids.isEmpty()) return;
    QSqlQuery ins(db);
    ins.prepare("INSERT OR IGNORE INTO image_keyword (image_id, keyword_id)"
                " VALUES (?, ?)");
    for (qint64 id : ids) {
        ins.addBindValue(imageId);
        ins.addBindValue(id);
        ins.exec();
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
    the keyword facet does through the ancestor rows.
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
                " shutter = ?, focallength = ?, width = ?, height = ?, gpscoord = ?"
                " WHERE id = ?");

    QSqlQuery ins(db);
    ins.prepare("INSERT INTO image (pathkey, path, folder, vol, filename, ext,"
                " srcsize, srcmtime, sidecarmtime, indexed, live,"
                " captured, rating, label, pick, title, creator, copyright,"
                " make, model, lens, iso, aperture, shutter, focallength,"
                " width, height, gpscoord)"
                " VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, 1,"
                " ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");

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
    for (const CatalogRow &r : candidates) {
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
    return stale;
}

/* ---------------------------------------------------------------------------------
   Search
   --------------------------------------------------------------------------------- */

QStringList Catalog::search(const CatalogQuery &cq, int limit, int *total)
{
/*
    Build one SELECT from whichever parts of the query were filled in.

    EVERY VALUE IS BOUND, never interpolated -- including the FTS expression. The search
    box is user text and the catalog shares its database with the preview index, so a
    query that pasted text into SQL would put the previews one apostrophe away from a
    syntax error and worse.
*/
    QStringList out;
    if (total) *total = 0;

    QMutexLocker lk(&mutex);
    QSqlDatabase db = dbLocked();
    if (!db.isOpen()) return out;

    QStringList where;
    QVariantList binds;

    QString from = " FROM image i";

    const QString fts = ftsExpression(cq.text);
    if (!fts.isEmpty()) {
        from += " JOIN image_fts f ON f.rowid = i.id";
        where << "image_fts MATCH ?";
        binds << fts;
    }

    if (!cq.keyword.isEmpty()) {
        /* Matches the keyword itself and everything beneath it in the hierarchy. The
           ancestor rows written by writeKeywordsLocked mean the plain equality below
           already reaches descendants, but a user picking a node by PATH gets the
           prefix match too, so both spellings of the same intent behave alike. */
        from += " JOIN image_keyword ik ON ik.image_id = i.id"
                " JOIN keyword k ON k.id = ik.keyword_id";
        where << "(k.namefold = ? OR k.pathfold = ? OR k.pathfold LIKE ?)";
        const QString f = fold(cq.keyword);
        binds << f << f << (f + "|%");
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

QList<CatalogKeyword> Catalog::keywords()
{
    QList<CatalogKeyword> out;

    QMutexLocker lk(&mutex);
    QSqlDatabase db = dbLocked();
    if (!db.isOpen()) return out;

    QSqlQuery q(db);
    if (!q.exec("SELECT k.name, k.path, COUNT(ik.image_id)"
                " FROM keyword k"
                " LEFT JOIN image_keyword ik ON ik.keyword_id = k.id"
                " LEFT JOIN image i ON i.id = ik.image_id AND i.live = 1"
                " GROUP BY k.id"
                " ORDER BY k.pathfold, k.namefold")) {
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
    /* image_keyword goes by cascade, but the FTS table and the keyword vocabulary have no
       foreign key onto image, so they are cleared explicitly. */
    q.exec("DELETE FROM image_fts");
    q.exec("DELETE FROM image");
    q.exec("DELETE FROM keyword");
    keywordIds.clear();
}
