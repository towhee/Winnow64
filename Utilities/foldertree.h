#ifndef FOLDERTREE_H
#define FOLDERTREE_H

#include <QCollator>
#include <QDir>
#include <QHash>
#include <QMap>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QVector>

#include <algorithm>
#include <functional>

/*
    FOLDERS AS A HIERARCHY -- the one set of rules both folder trees are built from.
    See notes/Documentation.txt "The Library Tree (LibTree)".

    TWO TREES SHOW THE SAME FOLDERS. The Source panel's LibTree lists the Library's
    folders, and the Filters panel's Folders category lists the loaded rows' folders; in
    Library scope they are the same set, and a click in one checks an item in the other.
    If each worked out its own parents, top-level rows and labels they would disagree the
    first time one of them was edited -- so neither does. Both call hierarchy() below.

    A FOLDER'S IDENTITY IS ITS PATH, never its name. The Folders filter used to match on
    the name alone, and a library of 1,599 folders held 22 names used more than once
    ("DxO" eight times) -- ticking one ticked all of them. So a node carries its path and
    shows its name, the same split the Keywords category makes.

    ANCESTRY IS WHAT MAKES A BRANCH FILTERABLE CHEAPLY. G::FolderPathsAllColumn holds a
    row's folder AND every folder above it, so "this folder and everything under it" is an
    exact QStringList::contains in the per-row predicate -- the same move
    G::KeywordsAllColumn makes for keyword paths, for the same reason: no separator
    awareness and no prefix scan in a loop that runs on every row on every filter change.

    ONE SPELLING, as in Main/catalogscope.h: forward slashes, no trailing slash, except a
    root, which IS its slash ("/" or "C:/"). Every test here is a prefix test on text, and
    a stray trailing slash makes a prefix test silently fail.
*/
namespace FolderTree {

/* "/" or "C:/": a volume root. It has no parent and is its own slash. */
inline bool isRoot(const QString &p)
{
    return p == "/" || (p.size() == 3 && p.at(1) == ':' && p.at(2) == '/');
}

/* The one spelling. catalogScopeNormalize (Main/catalogscope.h) delegates here. */
inline QString normalize(const QString &path)
{
    QString p = QDir::fromNativeSeparators(path.trimmed());
    while (p.size() > 1 && p.endsWith('/') && !p.endsWith(":/")) p.chop(1);
    return p;
}

/* Is path this folder or somewhere beneath it? The separator is load-bearing: a bare
   startsWith would put "/Photos/2024 raw" under "/Photos/2024". */
inline bool isAtOrUnder(const QString &path, const QString &ancestor)
{
    if (ancestor.isEmpty()) return false;
    if (path == ancestor) return true;
    const QString prefix = ancestor.endsWith('/') ? ancestor : ancestor + '/';
    return path.startsWith(prefix);
}

/* The folder above, or empty for a root (and for a UNC server, which has none). */
inline QString parentOf(const QString &path)
{
    const QString p = normalize(path);
    if (p.isEmpty() || isRoot(p)) return QString();
    const int i = p.lastIndexOf('/');
    if (i < 0) return QString();
    if (p.startsWith("//") && i <= 1) return QString();     // "//server"
    if (i == 0) return QStringLiteral("/");                 // "/Users"
    QString up = p.left(i);
    if (up.endsWith(':')) up += '/';                        // "C:/Photos" -> "C:/"
    return up;
}

/* What a node shows: the folder's own name. A root shows itself ("/", "C:"). */
inline QString leafOf(const QString &path)
{
    const QString p = normalize(path);
    if (p == "/") return p;
    if (isRoot(p)) return p.left(2);
    const int i = p.lastIndexOf('/');
    return i < 0 ? p : p.mid(i + 1);
}

/*  The folder and every folder above it, ROOT FIRST: "/Users/r/Photos" gives "/",
    "/Users", "/Users/r" and "/Users/r/Photos". A drive gives its root ("C:/"), and a UNC
    path starts at its server ("//srv") because nothing sits above that. */
inline QStringList ancestry(const QString &folder)
{
    QStringList out;
    const QString p = normalize(folder);
    if (p.isEmpty()) return out;

    int from = 0;
    if (p.startsWith("//")) from = 2;
    else if (p.startsWith('/')) { out << QStringLiteral("/"); from = 1; }

    for (int i = p.indexOf('/', from); i >= 0; i = p.indexOf('/', i + 1)) {
        QString prefix = p.left(i);
        if (prefix.endsWith(':')) prefix += '/';
        if (prefix.isEmpty() || (!out.isEmpty() && out.last() == prefix)) continue;
        out << prefix;
    }
    if (out.isEmpty() || out.last() != p) out << p;
    return out;
}

/*  Per-folder counts rolled up so every ancestor carries the sum beneath it. Exact,
    because an image lives in exactly one folder -- unlike keywords, where an image can
    sit under a parent twice and a sum would count it twice. */
inline QMap<QString, int> expandCounts(const QMap<QString, int> &perFolder)
{
    QMap<QString, int> out;
    for (auto it = perFolder.constBegin(); it != perFolder.constEnd(); ++it) {
        if (it.key().isEmpty()) continue;
        for (const QString &a : ancestry(it.key())) out[a] += it.value();
    }
    return out;
}

/* The deepest folder every one of these sits at or beneath; empty when they share
   nothing (two drives). */
inline QString commonAncestor(const QStringList &folders)
{
    QStringList common;
    bool first = true;
    for (const QString &f : folders) {
        const QStringList a = ancestry(f);
        if (a.isEmpty()) continue;
        if (first) { common = a; first = false; continue; }
        int n = 0;
        while (n < common.size() && n < a.size() && common.at(n) == a.at(n)) ++n;
        common = common.mid(0, n);
        if (common.isEmpty()) break;
    }
    return common.isEmpty() ? QString() : common.last();
}

/* The folders not inside another one in the list -- a recursive selection's roots.
   Normalised, duplicates dropped, in the order first seen. */
inline QStringList outermost(const QStringList &folders)
{
    QStringList norm;
    for (const QString &f : folders) {
        const QString n = normalize(f);
        if (!n.isEmpty() && !norm.contains(n)) norm << n;
    }
    QStringList out;
    for (const QString &f : norm) {
        bool inside = false;
        for (const QString &g : norm)
            if (f != g && isAtOrUnder(f, g)) { inside = true; break; }
        if (!inside) out << f;
    }
    return out;
}

struct Node {
    QString path;
    QString parent;     // empty for a top-level node
    QString label;      // the folder name; an anchor may add its parent's to stay unique
    bool isAnchor = false;
};

/*
    The nodes of the tree for these folders, PARENTS FIRST, siblings in natural order
    ("2" before "10", case-insensitive), so a caller can create items in one pass.

    ANCHORS ARE THE TOP-LEVEL ROWS. Nothing above an anchor is shown: the Library's
    anchors are the catalogued folders the user named (Manage Catalog), so the tree starts
    at "Photos" rather than at "/" -> "Users" -> ... Folders in between an anchor and the
    folders that hold images are SYNTHESISED -- "2020-2029" holds no images of its own and
    is still the row that selects the decade.

      - An anchor nested inside another is just a folder: the OUTERMOST one is the row.
      - The Folders scope passes the folders SELECTED in the Folders tree (outermost()),
        so two folders picked side by side are two rows, not one shared parent with the
        pair beneath it -- a single row that the panel greyed as nothing to choose.
      - With no anchors at all the deepest common ancestor is the anchor, unless it is a
        volume root; a folder load that did not come from the tree (a bookmark before
        the tree has caught up) lands here.
      - A folder under no anchor becomes an anchor itself rather than disappearing.
      - An anchor with nothing beneath it is not shown.
      - Two anchors with the same name each add their parent's name to the label.
*/
inline QVector<Node> hierarchy(const QStringList &folders, const QStringList &anchorsIn)
{
    QStringList fs;
    {
        QSet<QString> seen;
        for (const QString &f : folders) {
            const QString n = normalize(f);
            if (n.isEmpty() || seen.contains(n)) continue;
            seen.insert(n);
            fs << n;
        }
    }

    QStringList anchors;
    for (const QString &a : anchorsIn) {
        const QString n = normalize(a);
        if (!n.isEmpty() && !anchors.contains(n)) anchors << n;
    }
    if (anchors.isEmpty()) {
        const QString dca = commonAncestor(fs);
        if (!dca.isEmpty() && !isRoot(dca)) anchors << dca;
    }

    auto underAny = [](const QString &f, const QStringList &as) {
        for (const QString &a : as) if (isAtOrUnder(f, a)) return true;
        return false;
    };

    /*  Leftovers become anchors, shortest first so a leftover beneath another leftover
        joins its tree instead of standing beside it. */
    QStringList leftovers;
    for (const QString &f : fs) if (!underAny(f, anchors)) leftovers << f;
    std::sort(leftovers.begin(), leftovers.end(),
              [](const QString &a, const QString &b) { return a.size() < b.size(); });
    for (const QString &f : leftovers) if (!underAny(f, anchors)) anchors << f;

    // outermost wins
    QStringList outer;
    for (const QString &a : anchors) {
        bool nested = false;
        for (const QString &b : anchors)
            if (a != b && isAtOrUnder(a, b)) { nested = true; break; }
        if (!nested) outer << a;
    }

    QHash<QString, QString> parentByPath;       // path -> parent path ("" = top level)
    for (const QString &f : fs) {
        QString anchor;
        for (const QString &a : outer) if (isAtOrUnder(f, a)) { anchor = a; break; }
        if (anchor.isEmpty()) continue;                 // cannot happen; see leftovers
        QString cur = f;
        while (!parentByPath.contains(cur)) {
            if (cur == anchor) { parentByPath.insert(cur, QString()); break; }
            const QString up = parentOf(cur);
            parentByPath.insert(cur, up);
            if (up.isEmpty()) break;
            cur = up;
        }
    }

    // labels: the name, and for colliding anchors the parent's name too
    QHash<QString, int> anchorLeafUses;
    for (auto it = parentByPath.constBegin(); it != parentByPath.constEnd(); ++it)
        if (it.value().isEmpty()) anchorLeafUses[leafOf(it.key())]++;
    auto labelFor = [&](const QString &path, bool isAnchor) {
        const QString leaf = leafOf(path);
        if (!isAnchor || anchorLeafUses.value(leaf) < 2) return leaf;
        const QString up = parentOf(path);
        return up.isEmpty() ? path : QString("%1 (%2)").arg(leaf, leafOf(up));
    };

    QHash<QString, QStringList> children;
    QStringList tops;
    for (auto it = parentByPath.constBegin(); it != parentByPath.constEnd(); ++it) {
        if (it.value().isEmpty()) tops << it.key();
        else children[it.value()] << it.key();
    }

    QCollator collator;
    collator.setNumericMode(true);
    collator.setCaseSensitivity(Qt::CaseInsensitive);
    QHash<QString, QString> labelByPath;
    for (auto it = parentByPath.constBegin(); it != parentByPath.constEnd(); ++it)
        labelByPath.insert(it.key(), labelFor(it.key(), it.value().isEmpty()));
    auto bySortKey = [&](const QString &a, const QString &b) {
        const int c = collator.compare(labelByPath.value(a), labelByPath.value(b));
        return c != 0 ? c < 0 : a < b;
    };

    QVector<Node> out;
    out.reserve(parentByPath.size());
    std::function<void(const QString &)> visit = [&](const QString &path) {
        const QString parent = parentByPath.value(path);
        out.append({path, parent, labelByPath.value(path), parent.isEmpty()});
        QStringList kids = children.value(path);
        std::sort(kids.begin(), kids.end(), bySortKey);
        for (const QString &k : kids) visit(k);
    };
    std::sort(tops.begin(), tops.end(), bySortKey);
    for (const QString &t : tops) visit(t);
    return out;
}

} // namespace FolderTree

#endif // FOLDERTREE_H
