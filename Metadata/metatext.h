#ifndef METATEXT_H
#define METATEXT_H

#include <QString>
#include <QStringList>

/*
    THE USER-TEXT FIELDS -- title, creator, copyright, email and url -- CHECKED ON THE WAY
    IN, from a file's metadata read and from a catalog row.

    WHY: a misread pointed one of them at the image data itself. Two iPhone ProRAW DNGs
    came back with a "title" that was the file's own bytes from offset 0 (~10 million
    characters of U+FFFD and control codes), which the catalog stored and every load
    reloaded. Nothing failed until the Filters Titles list was sorted: localeAwareCompare
    normalises both strings on every comparison, and on a string that size the sort never
    finishes -- a permanent beachball the moment a title was edited.

    THE RULE. Real text metadata never contains a C0 control other than tab, CR and LF,
    nor U+FFFD (the replacement character fromUtf8 substitutes for bytes that are not
    UTF-8). Either one means the bytes were not text, so the value is discarded rather
    than repaired. A value that is text but absurdly long is cut to kMaxLen, the same
    cap the IPTC reader applies.

    A TEMPLATE because ImageMetadata and CatalogRow name these fields identically, and
    both entry points must apply the same rule. The original (_title ...) is cleaned
    with its value so an unchanged field still compares equal and is not mistaken for
    an edit by Metadata::writeXMP.
*/
namespace MetaText {

constexpr qsizetype kMaxLen = 1024;

inline bool isBinary(const QString &s)
{
    for (const QChar c : s) {
        const char16_t u = c.unicode();
        if (u == 0xFFFD) return true;
        if (u < 0x20 && u != '\t' && u != '\n' && u != '\r') return true;
    }
    return false;
}

// Returns true when s was changed (discarded or truncated)
inline bool clean(QString &s)
{
    if (s.isEmpty()) return false;
    if (isBinary(s)) { s.clear(); return true; }
    if (s.size() > kMaxLen) { s.truncate(kMaxLen); return true; }
    return false;
}

// Returns the names of the fields that were changed, for the caller to report
template <typename T>
QStringList cleanFields(T &m)
{
    QStringList changed;
    auto one = [&](QString &v, QString &orig, const char *name) {
        const bool a = clean(v);
        const bool b = clean(orig);
        if (a || b) changed << name;
    };
    one(m.title, m._title, "title");
    one(m.creator, m._creator, "creator");
    one(m.copyright, m._copyright, "copyright");
    one(m.email, m._email, "email");
    one(m.url, m._url, "url");
    return changed;
}

} // namespace MetaText

#endif // METATEXT_H
