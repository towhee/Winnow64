#ifndef KEYWORDFLATTEN_H
#define KEYWORDFLATTEN_H

#include <QSet>
#include <QString>
#include <QStringList>

/*
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

#endif // KEYWORDFLATTEN_H
