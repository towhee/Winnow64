#include "Utilities/tokenfilename.h"

/*
    See tokenfilename.h. The token table and the expansion are lifted verbatim from
    IngestDlg (which had the reference implementation) so existing templates produce
    byte-identical names through every caller.
*/

namespace TokenFileName {

/*
    The token table, in the order the token editor lists them. This is the single source
    for BOTH the ordered list (tokens()) and the name -> example lookup (exampleMap()), so
    a token added here appears in the editor, validates in isToken() and -- once handled
    in parse() -- expands everywhere.

    Order matters: a QMap sorts its keys, which would scramble the editor's grouping
    (dates, then camera, then sequence), so the ordered vector is the primary form.
*/
static const QVector<QPair<QString, QString>> &table()
{
    static const QVector<QPair<QString, QString>> t = {
        { "ORIGINAL FILENAME", "_C8I0024" },
        { "YYYY",              "2018" },
        { "YY",                "18" },
        { "MONTH",             "JANUARY" },
        { "Month",             "January" },
        { "MON",               "JAN" },
        { "Mon",               "Jan" },
        { "MM",                "01" },
        { "DAY",               "WEDNESDAY" },
        { "Day",               "Wednesday" },
        { "DDD",               "WED" },
        { "Ddd",               "Wed" },
        { "DD",                "07" },
        { "HOUR",              "08" },
        { "MINUTE",            "32" },
        { "SECOND",            "45" },
        { "MILLISECOND",       "167" },
        { "TITLE",             "Hill_Wedding" },
        { "CREATOR",           "Rory Hill" },
        { "COPYRIGHT",         "2018 Rory Hill" },
        { "MAKE",              "Canon" },
        { "MODEL",             "Canon EOS-1D X Mark II" },
        { "DIMENSIONS",        "5472x3648" },
        { "SHUTTER SPEED",     "1/1000 sec" },
        { "APERTURE",          "f/5.6" },
        { "ISO",               "1600" },
        { "FOCAL LENGTH",      "840 mm" },
        { "XX",                "01" },
        { "XXX",               "001" },
        { "XXXX",              "0001" },
        { "XXXXX",             "00001" },
        { "XXXXXX",            "000001" },
        { "XXXXXXX",           "0000001" },
    };
    return t;
}

const QMap<QString, QString> &exampleMap()
{
    static const QMap<QString, QString> m = []() {
        QMap<QString, QString> out;
        for (const auto &p : table()) out.insert(p.first, p.second);
        return out;
    }();
    return m;
}

QStringList tokens()
{
    static const QStringList list = []() {
        QStringList out;
        for (const auto &p : table()) out << p.first;
        return out;
    }();
    return list;
}

bool isToken(const QString &tokenString, int pos, QString &token, int &start, int &end)
{
    if (pos >= tokenString.length()) return false;
    QChar ch = tokenString.at(pos);
    if (ch.unicode() == 8233) return false;     // Paragraph Separator
    if (ch == '{') return false;
    if (pos == 0) return false;

    // look backwards for the opening brace
    bool foundPossibleTokenStart = false;
    int startPos = 0;
    for (int i = pos; i >= 0; i--) {
        ch = tokenString.at(i);
        if (i < pos && ch == '}') return false;
        if (ch == '{') {
            foundPossibleTokenStart = true;
            startPos = i + 1;
        }
        if (foundPossibleTokenStart) break;
    }
    if (!foundPossibleTokenStart) return false;

    // look forwards for the closing brace, and accept only a known token name
    for (int i = pos; i < tokenString.length(); i++) {
        ch = tokenString.at(i);
        if (ch == '}') {
            QString t;
            for (int j = startPos; j < i; j++) t.append(tokenString.at(j));
            if (exampleMap().contains(t)) {
                token = t;
                start = startPos - 1;
                end = i + 1;
                return true;
            }
        }
    }
    return false;
}

QString parse(const ImageMetadata &m, const QFileInfo &info,
              const QString &tokenString, int seqNum)
{
    const QDateTime createdDate = m.createdDate;
    QString s;
    int i = 0;
    while (i < tokenString.length()) {
        QString token;
        int tokenStart = 0, tokenEnd = 0;
        if (isToken(tokenString, i + 1, token, tokenStart, tokenEnd)) {
            QString r;
            if (token == "YYYY")        r = createdDate.date().toString("yyyy");
            if (token == "YY")          r = createdDate.date().toString("yy");
            if (token == "MONTH")       r = createdDate.date().toString("MMMM").toUpper();
            if (token == "Month")       r = createdDate.date().toString("MMMM");
            if (token == "MON")         r = createdDate.date().toString("MMM").toUpper();
            if (token == "Mon")         r = createdDate.date().toString("MMM");
            if (token == "MM")          r = createdDate.date().toString("MM");
            if (token == "DAY")         r = createdDate.date().toString("dddd").toUpper();
            if (token == "Day")         r = createdDate.date().toString("dddd");
            if (token == "DDD")         r = createdDate.date().toString("ddd").toUpper();
            if (token == "Ddd")         r = createdDate.date().toString("ddd");
            if (token == "DD")          r = createdDate.date().toString("dd");
            if (token == "HOUR")        r = createdDate.time().toString("hh");
            if (token == "MINUTE")      r = createdDate.time().toString("mm");
            if (token == "SECOND")      r = createdDate.time().toString("ss");
            if (token == "MILLISECOND") r = createdDate.time().toString("zzz");
            if (token == "TITLE")       r = m.title;
            if (token == "CREATOR")     r = m.creator;
            if (token == "COPYRIGHT")   r = m.copyright;
            if (token == "ORIGINAL FILENAME") r = info.baseName();
            if (token == "MAKE")        r = m.make;
            if (token == "MODEL")       r = m.model;
            if (token == "DIMENSIONS")  r = m.dimensions;
            if (token == "SHUTTER SPEED") r = m.exposureTime;
            if (token == "APERTURE")    r = m.aperture;
            if (token == "ISO")         r = m.ISO;
            if (token == "FOCAL LENGTH") r = m.focalLength;
            /* Sequence {XX}..{XXXXXXX}: the token length is the zero-padded width. */
            if (token.left(2) == "XX")
                r = QString("%1").arg(seqNum, token.length(), 10, QChar('0'));
            s.append(r);
            i = tokenEnd;
        }
        else {
            s.append(tokenString.at(i));
            i++;
        }
    }
    return s;
}

} // namespace TokenFileName
