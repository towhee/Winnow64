#ifndef LRKEYWORDS_H
#define LRKEYWORDS_H

#include <QIODevice>
#include <QList>
#include <QString>
#include <QStringList>
#include <QTextStream>

#include "Metadata/keywordpaths.h"

/*
    THE LIGHTROOM KEYWORD FILE, read and written.

    TWO SHAPES, BOTH REAL, and reading only one of them was this module's bug. Lightroom
    writes either:

      o A FIVE-COLUMN CSV:
            Include On Export, Export Containing Keywords, Export Synonyms,
            Person Type Keyword, <the keyword>
        UTF-8 with a BOM, ordinary CSV quoting, and THE HIERARCHY AS TAB INDENTATION
        INSIDE THE LAST FIELD. Verified against a 3,975-keyword export, trimmed into
        tests/fixtures/lightroom_keywords.csv.

      o A PLAIN INDENTED .TXT: one keyword per line, no columns, no quoting, no BOM, and
        the hierarchy IS the indentation. Verified against a 4,050-line export, trimmed
        into tests/fixtures/lightroom_keywords.txt. This is what "Metadata > Export
        Keywords" writes, and it is the file most users have.

    THE FORMAT IS SNIFFED, NOT TAKEN FROM THE EXTENSION, because the extension does not
    settle it -- a CSV export saved as .txt is a file people have, and the indented
    export is a .txt that contains no columns at all. The first record decides: five or
    more fields whose first four are each blank, Y or N is the CSV, and anything else is
    read as indented text. Sniffing the FIELDS rather than the suffix matters because a
    plain-text keyword may contain commas ("Vancouver, BC"), and splitting one into
    fields would silently truncate it to "Vancouver".

    DEPTH IS COUNTED FROM THE INDENT, ROW BY ROW. A row deeper than the last pushes onto
    the ancestor stack; a shallower row pops back to its own level, however many levels
    that is. Both readers hand the depth to lrPaths and neither builds a path itself, so
    the two cannot come to disagree about what "one level deeper" means.

    TABS ARE THE INDENT. A file indented with SPACES is read too, with the unit inferred
    as the smallest indent in the file, because an editor that helpfully converted the
    tabs should not cost the user their hierarchy. A file with any tab indent is read as
    tabs, which is what Lightroom wrote.

    A SYNONYM IS A ROW WHOSE KEYWORD IS IN {BRACES}, one level deeper than the keyword it
    belongs to; in the CSV its four flag columns are also blank. In the real CSV export
    every brace row is a blank-flag row and every blank-flag row is a brace row.

    THE SYNONYM ATTACHES TO THE NEAREST PRECEDING NON-SYNONYM ROW AT DEPTH - 1, which is
    not the same as "the previous row": two synonyms in a row share one parent, and the
    second's predecessor is the first synonym rather than the keyword.

    Observed depth ran to 8 levels.

    NOT VERIFIED, and therefore handled defensively rather than confidently: square
    brackets, and a flag column of "N". Neither appears anywhere in the real export, so
    the bracket semantics everyone quotes could not be confirmed. A bracketed name is
    read as an ordinary name -- guessing that it means "do not export" and silently
    turning off a keyword's export flag would be a data change made on a rumour.
*/

struct LrKeyword
{
    int depth = 0;                  // 0 = a root
    QString name;                   // the leaf, never a path
    QStringList synonyms;
    bool includeOnExport = true;
    bool exportContaining = true;
    bool exportSynonyms = true;
    bool personType = false;
};

/*
    One CSV record, honouring quotes and embedded newlines. Written out rather than
    split(',') because a keyword may contain a comma -- "Vancouver, BC" is a keyword
    somebody has -- and because a quoted field may span lines.

    Returns false at end of input.
*/
inline bool lrReadRecord(QTextStream &in, QStringList &fields)
{
    fields.clear();
    QString field;
    bool inQuotes = false;
    bool any = false;

    while (!in.atEnd()) {
        const QChar c = in.read(1).at(0);
        any = true;
        if (inQuotes) {
            if (c == '"') {
                /*  A doubled quote inside a quoted field is one literal quote. */
                if (!in.atEnd()) {
                    const QChar next = in.read(1).at(0);
                    if (next == '"') { field += '"'; continue; }
                    in.seek(in.pos() - 1);
                }
                inQuotes = false;
            }
            else field += c;
            continue;
        }
        if (c == '"') { inQuotes = true; continue; }
        if (c == ',') { fields << field; field.clear(); continue; }
        if (c == '\r') continue;
        if (c == '\n') { fields << field; return true; }
        field += c;
    }
    if (!any) return false;
    fields << field;
    return true;
}

/*
    Which of the two shapes a file is in. See the header: the extension does not say.
*/
enum class LrFormat { Csv, Indented };

/*
    Does this record look like a row of the five-column CSV?

    THE FIRST FOUR FIELDS ARE THE TEST, not the field count. A plain-text keyword with
    four commas in it -- unlikely, but a keyword is free text -- would otherwise be
    mistaken for a CSV row and lose everything before its last comma. Flags are blank
    (a synonym row), Y or N; anything else is prose, and prose means indented text.
*/
inline bool lrLooksLikeCsvRecord(const QStringList &fields)
{
    if (fields.size() < 5) return false;
    if (fields.at(0).trimmed().compare("Include On Export", Qt::CaseInsensitive) == 0)
        return true;
    for (int i = 0; i < 4; ++i) {
        const QString v = fields.at(i).trimmed();
        if (v.isEmpty()) continue;
        if (v.compare("Y", Qt::CaseInsensitive) != 0 &&
            v.compare("N", Qt::CaseInsensitive) != 0) return false;
    }
    return true;
}

/*
    Add one row, or attach one synonym to the row it belongs to.

    SHARED BY BOTH READERS on purpose. The brace rule and the flag defaults are the two
    places the formats could quietly diverge, and a synonym that attached in the CSV but
    became a keyword called "{Insurance}" in the .txt is exactly the kind of difference
    nobody notices until it is in their vocabulary. flags is empty for indented text,
    which is what gives every flag its default.
*/
inline void lrAppendRow(QList<LrKeyword> &out, int depth, const QString &trimmed,
                        const QStringList &flags)
{
    if (trimmed.isEmpty()) return;

    if (trimmed.startsWith('{') && trimmed.endsWith('}')) {
        const QString syn = trimmed.mid(1, trimmed.size() - 2).trimmed();
        if (syn.isEmpty()) return;
        for (int i = out.size() - 1; i >= 0; --i) {
            if (out.at(i).depth == depth - 1) { out[i].synonyms << syn; return; }
        }
        return;
    }

    LrKeyword k;
    k.depth = depth;
    k.name = trimmed;
    auto flag = [&flags](int i, bool dflt) {
        const QString v = flags.value(i).trimmed();
        if (v.isEmpty()) return dflt;
        return v.compare("Y", Qt::CaseInsensitive) == 0;
    };
    k.includeOnExport  = flag(0, true);
    k.exportContaining = flag(1, true);
    k.exportSynonyms   = flag(2, true);
    k.personType       = flag(3, false);
    out << k;
}

/*
    The five-column CSV. Depth is the tab count inside the last field.

    UNPARSEABLE ROWS ARE SKIPPED, NOT GUESSED AT. A row with fewer than five fields, or
    with an empty keyword, tells us nothing; inventing a name for it would put a keyword
    in the user's vocabulary that no file ever mentioned.
*/
inline QList<LrKeyword> lrParseCsv(const QString &text)
{
    QList<LrKeyword> out;
    QString buf = text;
    QTextStream in(&buf, QIODevice::ReadOnly);

    QStringList fields;
    bool first = true;
    while (lrReadRecord(in, fields)) {
        if (fields.size() < 5) continue;
        /*  The header row names its own columns; recognise it rather than assuming the
            first record is one, because a file may have been saved without it. */
        if (first) {
            first = false;
            if (fields.at(0).trimmed().compare("Include On Export",
                                               Qt::CaseInsensitive) == 0) continue;
        }

        /*  The keyword is the LAST field, not field 4: a keyword containing a comma
            arrives quoted and reassembled, but a malformed row could still split into
            more than five. */
        const QString raw = fields.last();
        int depth = 0;
        while (depth < raw.size() && raw.at(depth) == '\t') ++depth;
        lrAppendRow(out, depth, raw.trimmed(), fields);
    }
    return out;
}

/*
    The plain indented .txt: the whole line is the keyword and the indent is the depth.

    NO SPLITTING, ON ANYTHING. There are no fields here, so a comma, a quote and a
    newline are all ordinary characters -- which is the point of reading this format
    separately rather than running it through the CSV reader and hoping.
*/
inline QList<LrKeyword> lrParseIndented(const QString &text)
{
    QList<LrKeyword> out;
    const QStringList lines = text.split('\n');

    /*  ONE INDENT UNIT FOR THE WHOLE FILE, decided before any row is read. Deciding per
        row cannot work: "    Bear" is depth 1 in a four-space file and depth 2 in a
        two-space one, and only the file as a whole says which. Tabs win outright when
        the file has any, because that is what Lightroom writes. */
    bool tabs = false;
    int unit = 0;
    for (const QString &line : lines) {
        if (line.startsWith('\t')) { tabs = true; break; }
        int n = 0;
        while (n < line.size() && line.at(n) == ' ') ++n;
        if (n > 0 && (unit == 0 || n < unit)) unit = n;
    }
    if (unit == 0) unit = 1;

    for (const QString &line : lines) {
        QString row = line;
        if (row.endsWith('\r')) row.chop(1);
        const QString trimmed = row.trimmed();
        if (trimmed.isEmpty()) continue;
        /*  A CSV header sitting on top of an otherwise column-less file is still a
            header, and is not a keyword called "Include On Export,Export...". */
        if (trimmed.startsWith("Include On Export,")) continue;

        int n = 0;
        const QChar ch = tabs ? QChar('\t') : QChar(' ');
        while (n < row.size() && row.at(n) == ch) ++n;
        lrAppendRow(out, tabs ? n : n / unit, trimmed, QStringList());
    }
    return out;
}

/*
    Parse a Lightroom keyword export in either shape. Returns the rows in file order,
    with synonyms already attached to their keyword rather than left as rows of their own.

    READ WHOLE, because the format cannot be known until the first record has been read
    and the reader for it cannot start half way through. A keyword vocabulary is a few
    thousand short lines; there is nothing here to stream.
*/
inline QList<LrKeyword> lrParse(QIODevice *device)
{
    QList<LrKeyword> out;
    if (!device) return out;

    QString text = QString::fromUtf8(device->readAll());
    if (!text.isEmpty() && text.at(0) == QChar(0xFEFF)) text.remove(0, 1);
    if (text.isEmpty()) return out;

    QString probe = text;
    QTextStream sniff(&probe, QIODevice::ReadOnly);
    QStringList fields;
    LrFormat fmt = LrFormat::Indented;
    while (lrReadRecord(sniff, fields)) {
        /*  A leading blank line decides nothing; the first record that says something
            decides everything. */
        if (fields.size() == 1 && fields.at(0).trimmed().isEmpty()) continue;
        if (lrLooksLikeCsvRecord(fields)) fmt = LrFormat::Csv;
        break;
    }

    return fmt == LrFormat::Csv ? lrParseCsv(text) : lrParseIndented(text);
}

/*
    Turn parsed rows into full paths, in file order, using the depth column.

    A ROW DEEPER THAN ITS PREDECESSOR + 1 IS CLAMPED rather than dropped. Lightroom does
    not write such a file, but a hand-edited one can, and losing a branch silently is
    worse than filing it one level too shallow where the user can see and move it.
*/
inline QStringList lrPaths(const QList<LrKeyword> &rows)
{
    QStringList paths;
    QStringList stack;
    for (const LrKeyword &k : rows) {
        const int depth = qMin(k.depth, stack.size());
        stack = stack.mid(0, depth);
        stack << k.name;
        paths << stack.join('|');
    }
    return paths;
}

/*
    Write a vocabulary out in the same format, so a round trip through Lightroom is
    possible. paths must be sorted so that a parent precedes its children -- pathfold
    order does that, since a path is a prefix of its children.
*/
inline void lrWrite(QTextStream &out, const QStringList &paths,
                    const QHash<QString, QStringList> &synonymsByPath)
{
    out.setEncoding(QStringConverter::Utf8);
    out.setGenerateByteOrderMark(true);
    out << "Include On Export,Export Containing Keywords,Export Synonyms,"
           "Person Type Keyword,\n";

    auto quoted = [](const QString &s) {
        /*  Quote only when the value needs it, which is what Lightroom does -- quoting
            everything would still parse but would not diff against a real export. */
        if (!s.contains(',') && !s.contains('"') && !s.contains('\n')) return s;
        QString q = s;
        q.replace("\"", "\"\"");
        return '"' + q + '"';
    };

    for (const QString &path : paths) {
        const QStringList nodes = keywordNodes(path);
        if (nodes.isEmpty()) continue;
        const int depth = nodes.size() - 1;
        out << "Y,Y,Y,N," << quoted(QString(depth, '\t') + nodes.last()) << "\n";
        /*  Synonyms follow their keyword, one level deeper, with blank flags -- the
            shape the real export uses. */
        for (const QString &syn : synonymsByPath.value(keywordFold(path)))
            out << ",,,," << quoted(QString(depth + 1, '\t') + '{' + syn + '}') << "\n";
    }
}

#endif // LRKEYWORDS_H
