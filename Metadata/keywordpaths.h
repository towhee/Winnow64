#ifndef KEYWORDPATHS_H
#define KEYWORDPATHS_H

#include <QSet>
#include <QString>
#include <QStringList>

/*
    HOW A KEYWORD IS IDENTIFIED, and the one definition of it, shared by the datamodel and
    the catalog index. See notes/Documentation.txt "Keywords and Cataloguing".

    A KEYWORD'S IDENTITY IS ITS FULL PATH. "Location|Canada|BC|Vancouver" and
    "Location|USA|WA|Vancouver" are two keywords, not one name meaning two places. Two
    properties carry keywords in a file: dc:subject, the flat list every application
    writes, and lr:hierarchicalSubject, Lightroom's parallel list of full paths.

    THIS FILE USED TO ARGUE THE OPPOSITE, and the history is worth keeping because the
    problem that drove it is real and is solved here rather than avoided. Winnow flattened
    a path into its NODE NAMES, so a keyword was its bare leaf. That was done because a
    hierarchical file carries the SAME tag twice -- Lightroom writes the leaf into
    dc:subject and the path into lr:hierarchicalSubject -- and storing both forms put
    "Heron" in the category list twice with its image count split between the entries.

    WHAT REPLACED IT is NODE CONSUMPTION rather than flattening: keywordEffectivePaths
    drops a dc:subject entry whose name matches ANY node of one of the SAME image's paths,
    because the path is the richer statement of the same fact. One tag, one keyword, and
    the hierarchy survives. It consumed only the LEAF at first, which left every ANCESTOR
    name Lightroom's "export containing keywords" writes into dc:subject standing as a
    phantom depth-1 keyword -- see the note on the nodes set in the function itself.

    THE OTHER TWO ARGUMENTS FOR FLAT ALSO HAVE ANSWERS. "The hierarchy is not universal"
    -- phone images, non-Adobe DAMs and IPTC-only files carry dc:subject and nothing else
    -- is true and costs nothing, because a flat list is a tree of depth one and those
    keywords simply become roots. "Ancestor search is the only thing the tree bought" is
    answered by keywordPrefixExpand: every ancestor is linked to the image in its own
    right, so searching a parent is plain equality with no tree to walk.

    WHAT FLAT IDENTITY COST, and why it was reversed in the end: a name used under two
    parents became ONE keyword with the counts merged and nothing able to tell them apart.
    In a real user vocabulary of 3,975 Lightroom keywords, 59 names appeared under more
    than one parent -- including a Bear Lake in BC and another in Colorado.

    FREE FUNCTIONS IN THEIR OWN HEADER, not Metadata members and not Catalog privates, for
    the reason Metadata/xmpapply.h and Cache/pathkey.h give: the datamodel and the index
    must split a path IDENTICALLY or the category and the search will disagree about the
    same picture, and Metadata/metadata.h includes every parser header, so a member would
    compile only as long as the include order happened to cooperate. This depends on
    nothing but QString.
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
    QSet<QString> nodes;     // folded name of EVERY node of every hierarchical path

    /*  EVERY NODE, NOT JUST THE LEAF.

        This collected leaves only, and a dc:subject entry naming an ANCESTOR of a path
        the image already carries therefore survived as a separate depth-1 keyword. That
        is not a rare shape: it is what Lightroom writes with "export containing
        keywords" on. One real row --

            dc:subject             Animal, BC, Canada, Chipmunk, Fauna,
                                   Kettle River Recreation Area, Location, Southern BC,
                                   Squirrel
            lr:hierarchicalSubject Fauna|Animal|Squirrel|Chipmunk
                                   Location|Canada|BC|Southern BC|Kettle River Recreation Area

        -- consumed Chipmunk and Kettle River Recreation Area and kept the other SEVEN,
        every one of them an ancestor name from those same two paths. The library then
        carries a phantom top-level "Squirrel" beside the real "Fauna|Animal|Squirrel",
        with its own image count, and the Filters panel offers two identically labelled
        nodes with nothing to choose between them. Checking the wrong one filters to
        nothing and says nothing about why.

        A NODE NAME IS CONSUMED WHEREVER IT SITS in a path the image carries, because
        keywordPrefixExpand emits every ancestor as a keyword in its own right -- so
        "Fauna|Animal" is already searchable and a bare "Animal" beside it is not a second
        fact, it is the same one spelled without its parents. A subject naming something
        NO path mentions is still kept: that is a genuinely unfiled keyword and the panel
        shows it as one. */
    for (const QString &p : hierarchical)
        for (const QString &node : keywordNodes(p))
            if (!node.isEmpty()) nodes.insert(keywordFold(node));

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
        if (nodes.contains(keywordFold(trimmed))) continue;    // named by a path already
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

/*
    Drop every path that is a proper ANCESTOR of another path in the same list.

        {"Location", "Location|Canada", "Location|Canada|BC", "Fauna|Bird"}
            -> {"Location|Canada|BC", "Fauna|Bird"}

    WHY IT IS SAFE TO DROP THEM. keywordPrefixExpand emits every ancestor of every path,
    so "Location" stays searchable, filterable and countable for an image tagged only
    "Location|Canada|BC". A separate depth-1 "Location" is therefore not a second fact
    about the picture -- it is the same fact spelled without its parents, which is exactly
    what keywordEffectivePaths already consumes when the file says it twice.

    IT IS NOT USED ON THE READ PATH. Reading keeps what the file says; only an operation
    that deliberately merges keywords (Tidy flat keywords) prunes, because that is the
    operation that CREATES the redundancy -- moving a flat "Canada" onto an image that
    already carries "Location|Canada|BC" would otherwise leave both.

    ORDER IS PRESERVED for the survivors, so a pruned list stays as deterministic as the
    list it came from.
*/
inline QStringList keywordPruneAncestors(const QStringList &paths)
{
    QStringList out;
    QList<QString> folded;
    folded.reserve(paths.size());
    for (const QString &p : paths) folded << keywordFold(p);

    for (int i = 0; i < paths.size(); ++i) {
        bool covered = false;
        for (int j = 0; j < paths.size(); ++j) {
            if (i == j) continue;
            /*  A PROPER ancestor: the separator test, not startsWith, or "Fauna" would
                be swallowed by "Faunal|Study". Equality is not pruning -- two identical
                paths are a duplicate, which the callers de-duplicate on their own. */
            if (folded.at(j) != folded.at(i)
                && folded.at(j).startsWith(folded.at(i) + '|')) {
                covered = true;
                break;
            }
        }
        if (!covered) out << paths.at(i);
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

#endif // KEYWORDPATHS_H
