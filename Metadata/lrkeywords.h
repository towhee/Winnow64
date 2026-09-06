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

    THE FORMAT WAS TAKEN FROM A REAL EXPORT, not from documentation, and it is not what
    the commonly repeated description says. Verified against a 3,975-keyword export
    (tests/fixtures/lightroom_keywords.csv is a trimmed copy preserving every shape):

      o It is a CSV with FIVE columns, not a tab-indented .txt:
            Include On Export, Export Containing Keywords, Export Synonyms,
            Person Type Keyword, <the keyword>
      o UTF-8 with a BOM.
      o THE HIERARCHY IS TAB INDENTATION INSIDE THE LAST FIELD. Depth is the number of
        leading tabs; the CSV quoting is ordinary, so a keyword containing a comma is
        quoted like any other field.
      o A SYNONYM IS A ROW WHOSE KEYWORD IS IN {BRACES}, one level deeper than the
        keyword it belongs to, and its four flag columns are BLANK. In the real export
        every brace row is a blank-flag row and every blank-flag row is a brace row.
      o Observed depth ran to 8 levels.

    THE SYNONYM ATTACHES TO THE NEAREST PRECEDING NON-SYNONYM ROW AT DEPTH - 1, which is
    not the same as "the previous row": two synonyms in a row share one parent, and the
    second's predecessor is the first synonym rather than the keyword.

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
    Parse a Lightroom keyword export. Returns the rows in file order, with synonyms
    already attached to their keyword rather than left as rows of their own.

    UNPARSEABLE ROWS ARE SKIPPED, NOT GUESSED AT. A row with fewer than five fields, or
    with an empty keyword, tells us nothing; inventing a name for it would put a keyword
    in the user's vocabulary that no file ever mentioned.
*/
inline QList<LrKeyword> lrParse(QIODevice *device)
{
    QList<LrKeyword> out;
    if (!device) return out;

    QTextStream in(device);
    in.setEncoding(QStringConverter::Utf8);      // handles and drops the BOM

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
        const QString trimmed = raw.trimmed();
        if (trimmed.isEmpty()) continue;

        int depth = 0;
        while (depth < raw.size() && raw.at(depth) == '\t') ++depth;

        if (trimmed.startsWith('{') && trimmed.endsWith('}')) {
            /*  A SYNONYM. It belongs to the nearest preceding row at depth - 1 that is
                not itself a synonym -- consecutive synonyms share a parent, so walking
                back only one row would attach the second to the first. */
            const QString syn = trimmed.mid(1, trimmed.size() - 2).trimmed();
            if (syn.isEmpty()) continue;
            for (int i = out.size() - 1; i >= 0; --i) {
                if (out.at(i).depth == depth - 1) { out[i].synonyms << syn; break; }
            }
            continue;
        }

        LrKeyword k;
        k.depth = depth;
        k.name = trimmed;
        auto flag = [&fields](int i, bool dflt) {
            const QString v = fields.value(i).trimmed();
            if (v.isEmpty()) return dflt;
            return v.compare("Y", Qt::CaseInsensitive) == 0;
        };
        k.includeOnExport  = flag(0, true);
        k.exportContaining = flag(1, true);
        k.exportSynonyms   = flag(2, true);
        k.personType       = flag(3, false);
        out << k;
    }
    return out;
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
