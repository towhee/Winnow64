#ifndef CATALOGSCOPE_H
#define CATALOGSCOPE_H

#include <QDir>
#include <QMetaType>
#include <QString>
#include <QStringList>
#include <QVector>

/*
    WHAT THE BACKGROUND SCANNER IS ALLOWED TO WALK, as one ordered table of rules rather
    than a root list plus a separate exclusion list. See notes/Documentation.txt
    "Cataloguing Designated Folders (the Scope Table and Background Scanner)".

    ONE TABLE, BECAUSE INCLUDE AND EXCLUDE ARE THE SAME KIND OF STATEMENT. Both name a
    folder and say how far down it reaches; presenting them as two lists made the reader
    hold the relationship in their head, and made "include subfolders" look like a
    property of the whole catalog when it is really a property of each folder named. A
    library with one branch carved out is two rows here, and reads as two rows.

    RECURSE IS PER ROW, and it means the same thing on both sides: an include row with it
    off catalogues that folder only, and an exclude row with it off skips that folder
    while still cataloguing everything beneath it. That is the only reading under which
    the column means one thing in both columns' company.

    ORDER IS NOT PRECEDENCE. An exclude always wins over an include, wherever it sits in
    the table -- the alternative is a rule list whose meaning changes when a user sorts
    it, and nobody can see precedence in a table that does not show it.
*/
/*  ONE SPELLING OF A FOLDER PATH, and it is load-bearing rather than tidy. Every rule
    here is a PREFIX TEST, and a prefix test compares text: "/Pictures/" never prefixes
    "/Pictures/Zenfolio" once the separator is appended, so a single trailing slash makes
    an exclusion silently do nothing and the dialog announce that a folder plainly inside
    a library is not inside it. Paths reach this table from a file dialog, from migrated
    settings and from the folder tree, and those three do not agree about the slash. The
    catalog's own `folder` column has none, so no-trailing-slash is the form that also
    matches the database. A volume root ("/" or "C:/") keeps its slash: it IS the slash. */
inline QString catalogScopeNormalize(const QString &path)
{
    QString p = QDir::fromNativeSeparators(path.trimmed());
    while (p.size() > 1 && p.endsWith('/') && !p.endsWith(":/")) p.chop(1);
    return p;
}

struct CatalogScopeEntry
{
    QString path;
    bool include = true;      // false = exclude this folder from the scan
    bool recurse = true;      // does this row reach into subfolders

    bool operator==(const CatalogScopeEntry &o) const {
        return path == o.path && include == o.include && recurse == o.recurse;
    }
};

using CatalogScope = QVector<CatalogScopeEntry>;

/*  WHAT THE FOLDERS THEMSELVES HOLD, counted on disk rather than read out of the index.
    rows is per scope row, in row order; inScope is the arithmetic the table implies --
    the top-level includes summed, minus the exclusions that actually bite. The point of
    counting it this way is that inScope and the catalog's own count are then two answers
    to the same question, and the difference between them is exactly the work a Scan has
    left to do. */
struct CatalogScopeCounts
{
    QVector<int> rows;
    int inScope = 0;
};

/* True when folder is named by an include row, or lies under a recursive one -- what
   makes an exclusion mean something. Same normalisation, same reason. */
inline bool catalogScopeIncludes(const CatalogScope &scope, const QString &folder)
{
    const QString f = catalogScopeNormalize(folder);
    for (const CatalogScopeEntry &e : scope) {
        if (!e.include || e.path.isEmpty()) continue;
        const QString in = catalogScopeNormalize(e.path);
        if (f == in) return true;
        if (e.recurse && f.startsWith(in + "/")) return true;
    }
    return false;
}

/* True when folder is named by an exclude row, or lies under a recursive one. The
   trailing separator is load-bearing: a plain startsWith would make "/Photos/2024"
   exclude "/Photos/2024 raw" as well, which is a silent way to lose half a library. */
inline bool catalogScopeExcludes(const CatalogScope &scope, const QString &folder)
{
    const QString f = catalogScopeNormalize(folder);
    for (const CatalogScopeEntry &e : scope) {
        if (e.include || e.path.isEmpty()) continue;
        const QString ex = catalogScopeNormalize(e.path);
        if (f == ex) return true;
        if (e.recurse && f.startsWith(ex + "/")) return true;
    }
    return false;
}

/* The exclude rows that prune a walk -- only the recursive ones, since a non-recursive
   exclude does not stop the descent past it. */
inline QStringList catalogScopePrunePaths(const CatalogScope &scope)
{
    QStringList out;
    for (const CatalogScopeEntry &e : scope)
        if (!e.include && e.recurse && !e.path.isEmpty())
            out << catalogScopeNormalize(e.path);
    return out;
}

Q_DECLARE_METATYPE(CatalogScopeEntry)
Q_DECLARE_METATYPE(CatalogScope)

#endif // CATALOGSCOPE_H
