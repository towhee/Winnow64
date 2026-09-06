#ifndef KEYWORDFLATTEN_H
#define KEYWORDFLATTEN_H

#include <QSet>
#include <QString>
#include <QStringList>

/*
    STATUS, 2026-09-06: THE FLAT DECISION ARGUED FOR BELOW IS BEING REVERSED. Keyword
    identity is going back to the full PATH, and this file is on its way to becoming
    Metadata/keywordpaths.h. The path algebra at the BOTTOM of this header --
    keywordEffectivePaths, keywordPrefixExpand, keywordParentPath, keywordIsDescendant --
    is the new definition and is what new code should use. flattenKeywords and the essay
    justifying it are still here because the datamodel and the index still call it, and
    they change together or not at all; both go when they do. Until then, read the
    rationale below as history rather than as the rule.

    How a keyword hierarchy is turned into the flat vocabulary Winnow filters and
    searches on. See notes/Documentation.txt "Keywords and Cataloguing".

    A KEYWORD'S IDENTITY IS ITS NAME. Two properties carry keywords: dc:subject, the flat
    list of leaf names that every application writes, and lr:hierarchicalSubject,
    Lightroom's parallel list of full paths ie "Location|Canada|BC|Vancouver". Winnow
    flattens the second into its NODE NAMES, so that path contributes four keywords --
    Location, Canada, BC and Vancouver -- each a first-class keyword in its own right.

    WHY FLAT RATHER THAN A TREE. Three reasons, and the first is the one that forced it:

      o A hierarchical file carries the SAME tag twice. Lightroom writes the leaf into
        dc:subject and the path into lr:hierarchicalSubject, so storing both forms
        separately put "Heron" in the category list twice, with the image counts split
        between the two entries. Flattening collapses them by construction, because both
        forms reduce to the same name.
      o The hierarchy is NOT UNIVERSAL. Phone images, non-Adobe DAMs and IPTC-only files
        carry dc:subject and nothing else. A tree is right for part of a library and
        overhead for the rest; a flat list is right for all of it, and a hierarchical
        library simply produces more keywords.
      o Ancestor search still works, which was the only thing the tree bought. Searching
        "Fauna" reaches an image tagged only "Fauna|Bird|Heron" because Fauna is now
        genuinely one of that image's keywords, not an ancestor to be walked to.

    WHAT IT COSTS, AND WHERE THAT IS PAID. A name that appears under two parents --
    "Location|Canada|BC|Vancouver" and "Location|USA|Washington|Vancouver" -- becomes ONE
    keyword meaning two places. The hierarchy could tell them apart and this cannot. That
    is paid for elsewhere rather than here: the catalog records which parents a name has
    been seen under (keyword_context), the Filters and Catalog docks colour an ambiguous
    keyword and name its parents in the tooltip, and filter EXCLUSION resolves it --
    include Vancouver, exclude USA.

    THE RAW PATHS ARE STILL KEPT. G::KeywordPathsColumn holds lr:hierarchicalSubject
    unchanged, and ImageMetadata::keywordPaths beside it. They are the evidence the
    ambiguity marking is built from, and they are what a future write-back must emit --
    NEVER the flattened list, which contains ancestors the file never had in dc:subject.

    A FREE FUNCTION IN ITS OWN HEADER, not a Metadata member and not a Catalog private,
    for the reason Metadata/xmpapply.h and Cache/pathkey.h give: the datamodel and the
    index must split a path IDENTICALLY or the category and the search will disagree about
    the same picture, and Metadata/metadata.h includes every parser header, so a member
    would compile only as long as the include order happened to cooperate. This depends
    on nothing but QString.
*/

/*
    Case folding for keyword comparison. toCaseFolded rather than toLower: case folding is
    the Unicode-correct operation and is locale-independent, where toLower is neither.
    The same choice Cache/pathkey.h makes, and the catalog's keyword.namefold column is
    this function's output.
*/
inline QString keywordFold(const QString &s)
{
    return s.trimmed().toCaseFolded();
}

/*
    The node names of one hierarchical keyword path, root first.
    "Location|Canada|BC" -> ["Location", "Canada", "BC"].

    Empty and whitespace-only segments are skipped rather than preserved as empty
    keywords: "A||B" is a malformed path, not a tag named "".
*/
inline QStringList keywordNodes(const QString &path)
{
    QStringList out;
    const auto parts = path.split('|', Qt::SkipEmptyParts);
    for (const QString &p : parts) {
        const QString trimmed = p.trimmed();
        if (!trimmed.isEmpty()) out << trimmed;
    }
    return out;
}

/*
    The leaf name of a hierarchical keyword path -- "A|B|C" -> "C". A path with no
    separator is its own leaf.
*/
inline QString keywordLeafOf(const QString &path)
{
    const int i = path.lastIndexOf('|');
    return (i < 0 ? path : path.mid(i + 1)).trimmed();
}

/*
    The flat keyword vocabulary for one image: the de-duplicated union of its dc:subject
    leaves and every node of every lr:hierarchicalSubject path.

    DE-DUPLICATION IS CASE-INSENSITIVE and FIRST SPELLING WINS. "Heron" and "heron" are
    one keyword, and the one that survives is whichever the file listed first -- which for
    a Lightroom file is the dc:subject leaf, the spelling the user actually typed.
    Comparing case-sensitively would defeat the whole point on a library that has been
    through more than one application.

    ORDER IS PRESERVED (flat keywords first, then hierarchy in path order) rather than
    sorted. The category lists sort for display anyway, and keeping insertion order makes
    the stored column read the way the file does, which matters when diagnosing an image.
*/
inline QStringList flattenKeywords(const QStringList &keywords, const QStringList &paths)
{
    QStringList out;
    QSet<QString> seen;

    auto add = [&out, &seen](const QString &name) {
        const QString trimmed = name.trimmed();
        if (trimmed.isEmpty()) return;
        const QString key = keywordFold(trimmed);
        if (seen.contains(key)) return;
        seen.insert(key);
        out << trimmed;
    };

    for (const QString &k : keywords) add(k);
    for (const QString &p : paths)
        for (const QString &node : keywordNodes(p)) add(node);

    return out;
}

/* ------------------------------------------------------------------------------------
    PATH IDENTITY. Everything below treats a keyword's identity as its full path, which
    is the model flattenKeywords above was written to replace and is now replacing IT.
    See notes/Documentation.txt and the plan named in the STATUS note at the top.
   --------------------------------------------------------------------------------- */

/*
    The paths one image actually carries, from its two properties.

    THE LIGHTROOM DOUBLE, AND WHY CONSUMPTION RATHER THAN COLLAPSE. A hierarchical file
    lists the same tag twice -- once as a dc:subject leaf ("Heron") and once as an
    lr:hierarchicalSubject path ("Fauna|Bird|Heron"). Taking both at face value gives the
    image a root-level "Heron" AND a "Fauna|Bird|Heron", which is two keywords where the
    user has one, and it splits the image count between them. That was the defect that
    forced flattening.

    A dc:subject entry is therefore CONSUMED by any of the same image's paths that ends
    in it: the path is the richer statement of the same fact. Only an entry that matches
    no path leaf survives, as a depth-1 path of its own -- which is exactly the phone
    image, the IPTC-only file and the non-Adobe DAM, none of which is a degraded case
    here because a flat list is simply a tree of depth one.

    MATCHING IS BY LEAF AND CASE-INSENSITIVE, deliberately loose. A file whose subject
    says "heron" and whose path says "Fauna|Bird|Heron" means one keyword, and treating
    the spellings as two would recreate the double this exists to prevent.

    HIERARCHY FIRST, then the survivors, and de-duplicated on the folded path so the
    order is deterministic. Order matters more than it looks: it is what makes the
    interned ids in the row store stable, so re-indexing an unchanged image is a no-op
    rather than a rewrite.
*/
inline QStringList keywordEffectivePaths(const QStringList &subject,
                                         const QStringList &hierarchical)
{
    QStringList out;
    QSet<QString> seen;      // folded paths already emitted
    QSet<QString> leaves;    // folded leaf of every hierarchical path

    for (const QString &p : hierarchical) {
        const QString leaf = keywordLeafOf(p);
        if (!leaf.isEmpty()) leaves.insert(keywordFold(leaf));
    }

    auto add = [&out, &seen](const QString &path) {
        if (path.isEmpty()) return;
        const QString key = keywordFold(path);
        if (seen.contains(key)) return;
        seen.insert(key);
        out << path;
    };

    /* Normalised through the node splitter and rejoined, so " A | B " and "A||B" cannot
       reach the index as two different identities for one path. */
    for (const QString &p : hierarchical) add(keywordNodes(p).join('|'));

    for (const QString &s : subject) {
        const QString trimmed = s.trimmed();
        if (trimmed.isEmpty()) continue;
        if (leaves.contains(keywordFold(trimmed))) continue;   // consumed by a path
        add(keywordNodes(trimmed).join('|'));
    }

    return out;
}

/*
    Every ancestor prefix of every path, root first:
        "Fauna|Bird|Heron" -> "Fauna", "Fauna|Bird", "Fauna|Bird|Heron"

    THIS IS WHAT KEEPS ANCESTOR SEARCH FREE. Filtering and searching run against the
    expansion, so a filter on "Fauna" reaches an image tagged only "Fauna|Bird|Heron" by
    plain EQUALITY -- no subtree walk in the predicate, no LIKE and no recursive CTE in
    the catalog, and Datamodel/filterpredicate.h's QStringList::contains is untouched.
    The cost is paid once, here, at write time.

    It also means a parent's image count needs no summing: the parent is linked directly
    to every image beneath it, so the count is already the subtree total.

    De-duplicated across the WHOLE set, not per path, because siblings share ancestors --
    which is why five assigned paths expand to roughly eleven entries rather than fifteen.
*/
inline QStringList keywordPrefixExpand(const QStringList &paths)
{
    QStringList out;
    QSet<QString> seen;

    for (const QString &p : paths) {
        QString prefix;
        for (const QString &node : keywordNodes(p)) {
            prefix = prefix.isEmpty() ? node : prefix + '|' + node;
            const QString key = keywordFold(prefix);
            if (seen.contains(key)) continue;
            seen.insert(key);
            out << prefix;
        }
    }

    return out;
}

/* The parent of a path -- "A|B|C" -> "A|B". A root has no parent and returns empty. */
inline QString keywordParentPath(const QString &path)
{
    const int i = path.lastIndexOf('|');
    return (i < 0) ? QString() : path.left(i).trimmed();
}

/*
    Is pathFold at or beneath ancestorFold? BOTH ARGUMENTS ARE ALREADY FOLDED -- the
    callers hold folded strings (the drop target, the retag range) and folding here would
    hide that from them.

    "Fauna|Bird" must not match "Fauna|Birdsong", so the test is the separator, not a bare
    startsWith. Two callers share this: rejecting a drop of a node onto its own
    descendant, and counting the images a rename or re-parent would rewrite. They must
    agree, or the dialog reports a number the operation does not go on to touch.
*/
inline bool keywordIsDescendant(const QString &pathFold, const QString &ancestorFold)
{
    if (ancestorFold.isEmpty()) return false;
    return pathFold == ancestorFold || pathFold.startsWith(ancestorFold + '|');
}

#endif // KEYWORDFLATTEN_H
