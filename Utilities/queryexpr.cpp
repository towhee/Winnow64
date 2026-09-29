#include "Utilities/queryexpr.h"
#include "Main/global.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <algorithm>
#include <cmath>

namespace Query {

namespace {

Resolver gResolver;

/*  Fields whose blank value means zero -- an unrated image is rated 0, so "rating:<2"
    finds it. Everything else treats blank as missing: it matches no comparison and
    only "is empty". */
bool emptyIsZero(const QString &key) { return key == "rating"; }

/*  Values a person writes for a yes/no field, folded to what the columns hold. */
QString canon(const QString &v)
{
    const QString s = v.trimmed().toLower();
    if (s == "yes" || s == "y") return "true";
    if (s == "no" || s == "n") return "false";
    return s;
}

/*  "1/250" is how a shutter speed is written; everything else is a plain number. */
bool toNumber(const QString &raw, double &out)
{
/*
    The LEADING number, with an optional "/denominator": "1/250", "5.6", and also the
    unit-bearing text some columns hold -- exposure compensation is "+0.7 EV".
*/
    static const QRegularExpression re(
        R"(^\s*([+-]?(?:\d+\.?\d*|\.\d+))(?:\s*/\s*(\d+\.?\d*))?)");
    const QRegularExpressionMatch m = re.match(raw);
    if (!m.hasMatch()) return false;
    const double a = m.captured(1).toDouble();
    if (m.captured(2).isEmpty()) { out = a; return true; }
    const double b = m.captured(2).toDouble();
    if (b == 0) return false;
    out = a / b;
    return true;
}

bool nearlyEqual(double a, double b)
{
    return std::fabs(a - b) <= 1e-6 * qMax(1.0, std::fabs(b));
}

}   // namespace

void setResolver(Resolver r) { gResolver = std::move(r); }

const QVector<Field> &fields()
{
/*
    THE FIELDS, in the order the builder lists them. key is the published name: it is
    what the text grammar and saved queries use, so it is added to, never renamed.
*/
    static const QVector<Field> f = [] {
        QVector<Field> v;
        auto add = [&v](const char *key, const QString &label, int col, Type t,
                        const QStringList &values = {}, const QString &hint = {}) {
            Field x;
            x.key = QString::fromLatin1(key);
            x.label = label;
            x.column = col;
            x.type = t;
            x.values = values;
            x.hint = hint;
            v << x;
        };
        add("text", "Any text", G::SearchTextColumn, Type::Words, {},
            "word or \"a phrase\"");
        add("keyword", "Keyword", G::KeywordsAllColumn, Type::List, {},
            "Fauna|Bird  (includes everything beneath)");
        add("collection", "Collection", G::CollectionsColumn, Type::List, {},
            "collection name");
        add("folder", "Folder", G::FolderPathsAllColumn, Type::List, {},
            "folder path (includes everything beneath)");
        add("rating", "Rating", G::RatingColumn, Type::Number, {}, "0 to 5");
        add("label", "Color label", G::LabelColumn, Type::Enum,
            {"Red", "Yellow", "Green", "Blue", "Purple"});
        add("pick", "Pick", G::PickColumn, Type::Enum, {"Picked", "Unpicked", "Rejected"});
        add("captured", "Capture date", G::CreatedColumn, Type::Date, {},
            "2024, 2024-05 or 2024-05-01");
        add("modified", "File modified", G::ModifiedColumn, Type::Date, {},
            "2024, 2024-05 or 2024-05-01");
        add("year", "Year", G::YearColumn, Type::Number, {}, "2024");
        add("month", "Month", G::MonthColumn, Type::Number, {}, "1 to 12");
        add("day", "Day of month", G::DayColumn, Type::Number, {}, "1 to 31");
        add("title", "Title", G::TitleColumn, Type::Text);
        add("creator", "Creator", G::CreatorColumn, Type::Text);
        add("copyright", "Copyright", G::CopyrightColumn, Type::Text);
        add("filename", "File name", G::NameColumn, Type::Text);
        add("type", "File type", G::TypeColumn, Type::Text, {}, "NEF, JPG ...");
        add("make", "Camera make", G::CameraMakeColumn, Type::Text);
        add("camera", "Camera model", G::CameraModelColumn, Type::Text);
        add("lens", "Lens", G::LensColumn, Type::Text);
        add("iso", "ISO", G::ISOColumn, Type::Number, {}, "3200");
        add("aperture", "Aperture", G::ApertureColumn, Type::Number, {}, "5.6");
        add("shutter", "Shutter speed (s)", G::ShutterspeedColumn, Type::Number, {},
            "1/250 or 0.5");
        add("focal", "Focal length (mm)", G::FocalLengthColumn, Type::Number, {}, "400");
        add("width", "Width (px)", G::WidthColumn, Type::Number);
        add("height", "Height (px)", G::HeightColumn, Type::Number);
        add("mp", "Megapixels", G::MegaPixelsColumn, Type::Number, {}, "24");
        add("gps", "Has GPS", G::HasGPSColumn, Type::Enum, {"Yes", "No"});
        add("developed", "Developed", G::DevelopColumn, Type::Enum, {"Yes", "No"});

        /*  THE REST OF THE DATAMODEL: every user-facing (non-diagnostic) column the main
            list does not already cover. Left out on purpose: the icon, the row number,
            the macOS thumbnail handles, the Search result flag, load timing and the icon
            aspect -- bookkeeping, not facts about a photograph. Sorted by label below,
            after a separator in the builder. */
        const int firstExtra = v.size();
        const QStringList yesNo{"Yes", "No"};
        add("foldername", "Folder name", G::FolderNameColumn, Type::Text);
        add("ingested", "Ingested", G::IngestedColumn, Type::Enum, yesNo);
        add("video", "Video", G::VideoColumn, Type::Enum, yesNo);
        add("sidecar", "Has sidecar", G::SidecarColumn, Type::Enum, yesNo);
        add("exposurecomp", "Exposure compensation (EV)", G::ExposureCompensationColumn,
            Type::Number, {}, "-0.7 or +1");
        add("duration", "Duration", G::DurationColumn, Type::Text);
        add("focusx", "Focus point X", G::FocusXColumn, Type::Number);
        add("focusy", "Focus point Y", G::FocusYColumn, Type::Number);
        add("gpscoord", "GPS coordinates", G::GPSCoordColumn, Type::Text);
        add("filesize", "File size (MB)", G::ByteSizeColumn, Type::Number, {}, "25");
        v.last().scale = 1000000.0;
        add("dimensions", "Dimensions", G::DimensionsColumn, Type::Text, {}, "6000x4000");
        add("aspect", "Aspect ratio", G::AspectRatioColumn, Type::Number, {}, "1.5");
        add("croppeddimensions", "Cropped dimensions", G::CroppedDimensionsColumn,
            Type::Text);
        add("croppedaspect", "Cropped aspect ratio", G::CroppedAspectRatioColumn,
            Type::Number, {}, "1.5");
        add("orientation", "Orientation", G::OrientationColumn, Type::Number);
        add("rotation", "Rotation", G::RotationColumn, Type::Number, {}, "0, 90, 180, 270");
        add("email", "Email", G::EmailColumn, Type::Text);
        add("url", "Url", G::UrlColumn, Type::Text);
        add("keywordtext", "Keywords as written", G::KeywordsColumn, Type::List, {},
            "exactly as in the file (dc:subject)");
        for (int i = firstExtra; i < v.size(); ++i) v[i].extra = true;
        std::sort(v.begin() + firstExtra, v.end(), [](const Field &a, const Field &b) {
            return a.label.compare(b.label, Qt::CaseInsensitive) < 0;
        });
        return v;
    }();
    return f;
}

const Field *field(const QString &key)
{
    for (const Field &f : fields())
        if (f.key.compare(key, Qt::CaseInsensitive) == 0) return &f;
    return nullptr;
}

QList<Op> opsFor(Type t)
{
    switch (t) {
    case Type::Words:  return {Op::Contains, Op::NotContains};
    case Type::Text:   return {Op::Contains, Op::NotContains, Op::Is, Op::IsNot,
                               Op::StartsWith, Op::EndsWith, Op::NotEmpty, Op::Empty};
    case Type::Number: return {Op::Is, Op::IsNot, Op::Lt, Op::Le, Op::Gt, Op::Ge,
                               Op::Between, Op::NotEmpty, Op::Empty};
    case Type::Date:   return {Op::Is, Op::IsNot, Op::Lt, Op::Gt, Op::Between,
                               Op::NotEmpty, Op::Empty};
    case Type::Enum:   return {Op::AnyOf, Op::NoneOf};
    case Type::List:   return {Op::AnyOf, Op::NoneOf, Op::NotEmpty, Op::Empty};
    }
    return {};
}

QString opLabel(Op op, Type t)
{
    const bool date = t == Type::Date;
    const bool list = t == Type::List;
    switch (op) {
    case Op::Contains:    return "contains";
    case Op::NotContains: return "does not contain";
    case Op::Is:          return date ? "is in" : "is";
    case Op::IsNot:       return date ? "is not in" : "is not";
    case Op::StartsWith:  return "starts with";
    case Op::EndsWith:    return "ends with";
    case Op::Empty:       return list ? "has none" : "is empty";
    case Op::NotEmpty:    return list ? "has any" : "is not empty";
    case Op::Lt:          return date ? "is before" : "is less than";
    case Op::Le:          return date ? "is on or before" : "is at most";
    case Op::Gt:          return date ? "is after" : "is greater than";
    case Op::Ge:          return date ? "is on or after" : "is at least";
    case Op::Between:     return "is between";
    case Op::AnyOf:       return list ? "includes any of" : "is any of";
    case Op::NoneOf:      return list ? "includes none of" : "is none of";
    }
    return QString();
}

QString opName(Op op)
{
    switch (op) {
    case Op::Contains:    return "contains";
    case Op::NotContains: return "notContains";
    case Op::Is:          return "is";
    case Op::IsNot:       return "isNot";
    case Op::StartsWith:  return "startsWith";
    case Op::EndsWith:    return "endsWith";
    case Op::Empty:       return "empty";
    case Op::NotEmpty:    return "notEmpty";
    case Op::Lt:          return "lt";
    case Op::Le:          return "le";
    case Op::Gt:          return "gt";
    case Op::Ge:          return "ge";
    case Op::Between:     return "between";
    case Op::AnyOf:       return "anyOf";
    case Op::NoneOf:      return "noneOf";
    }
    return QString();
}

bool opFromName(const QString &name, Op &op)
{
    static const QList<Op> all{Op::Contains, Op::NotContains, Op::Is, Op::IsNot,
                               Op::StartsWith, Op::EndsWith, Op::Empty, Op::NotEmpty,
                               Op::Lt, Op::Le, Op::Gt, Op::Ge, Op::Between,
                               Op::AnyOf, Op::NoneOf};
    for (Op o : all) if (opName(o) == name) { op = o; return true; }
    return false;
}

bool opTakesNoValue(Op op) { return op == Op::Empty || op == Op::NotEmpty; }
bool opTakesTwoValues(Op op) { return op == Op::Between; }

bool Node::operator==(const Node &o) const
{
    if (kind != o.kind) return false;
    if (kind == Group) return match == o.match && kids == o.kids;
    return field == o.field && op == o.op && values == o.values;
}

/* ---------------------------------------------------------------------------------
   Text: tokenize, parse, print
   --------------------------------------------------------------------------------- */

namespace {

struct Tok
{
    QString text;
    bool quoted = false;        // a whole-token phrase: never an operator or qualifier
};

/*  "key:" with a plain-name key (an optional leading '-' allowed): a quote after this
    belongs to the VALUE and does not end the token. */
bool hasQualifier(const QString &cur)
{
    const int c = cur.indexOf(':');
    if (c <= 0) return false;
    QString key = cur.left(c);
    if (key.startsWith('-')) key.remove(0, 1);
    if (key.isEmpty()) return false;
    for (const QChar ch : key) if (!ch.isLetter()) return false;
    return true;
}

QList<Tok> tokenize(const QString &raw)
{
    QList<Tok> out;
    QString cur;
    bool inQuote = false;
    bool valueQuote = false;        // the quote belongs to a qualified token's value
    auto flush = [&](bool quoted) {
        if (cur.isEmpty()) return;
        out << Tok{cur, quoted};
        cur.clear();
    };
    for (const QChar c : raw) {
        if (c == '"') {
            if (valueQuote) {                           // closes a value quote
                cur += c;
                valueQuote = false;
                inQuote = false;
                continue;
            }
            if (!inQuote && hasQualifier(cur)) {        // opens a value quote
                cur += c;
                valueQuote = true;
                inQuote = true;
                continue;
            }
            /*  A phrase. A '-' just before it is NOT, kept as its own token. */
            if (!inQuote && cur == "-") { out << Tok{"-", false}; cur.clear(); }
            flush(inQuote);
            inQuote = !inQuote;
            continue;
        }
        if (!inQuote && (c == '(' || c == ')')) {
            flush(false);
            out << Tok{QString(c), false};
            continue;
        }
        if (!inQuote && c.isSpace()) {
            flush(false);
            continue;
        }
        cur += c;
    }
    if (valueQuote) cur += '"';                         // close what was left open
    flush(inQuote && !valueQuote);
    return out;
}

/*  A value as written, split on commas outside quotes. quoted[i] says whether value i
    was quoted -- a quoted value is literal (no wildcard, no comparison). */
void splitValues(const QString &s, QStringList &vals, QList<bool> &quoted, bool split)
{
    QString cur;
    bool inQ = false, wasQ = false;
    for (const QChar c : s) {
        if (c == '"') { inQ = !inQ; wasQ = true; continue; }
        if (split && c == ',' && !inQ) {
            vals << cur; quoted << wasQ;
            cur.clear(); wasQ = false;
            continue;
        }
        cur += c;
    }
    vals << cur;
    quoted << wasQ;
}

Node negate(const Node &n)
{
    if (n.kind == Node::Rule) {
        Node r = n;
        switch (n.op) {
        case Op::Contains:    r.op = Op::NotContains; return r;
        case Op::NotContains: r.op = Op::Contains;    return r;
        case Op::Is:          r.op = Op::IsNot;       return r;
        case Op::IsNot:       r.op = Op::Is;          return r;
        case Op::Empty:       r.op = Op::NotEmpty;    return r;
        case Op::NotEmpty:    r.op = Op::Empty;       return r;
        case Op::AnyOf:       r.op = Op::NoneOf;      return r;
        case Op::NoneOf:      r.op = Op::AnyOf;       return r;
        default: break;
        }
        Node g = Node::group(Node::None);
        g.kids << n;
        return g;
    }
    if (n.match == Node::Any) {
        Node g = Node::group(Node::None);
        g.kids = n.kids;
        return g;
    }
    if (n.match == Node::None) {
        if (n.kids.size() == 1) return n.kids.first();
        Node g = Node::group(Node::Any);
        g.kids = n.kids;
        return g;
    }
    Node g = Node::group(Node::None);
    g.kids << n;
    return g;
}

/*  A qualified token's rule. rest is what follows "key:". */
Node qualifiedRule(const Field &f, const QString &rest)
{
    const QString key = f.key;
    const bool startsQuoted = rest.startsWith('"');

    if (f.type == Type::Words) {
        QStringList v; QList<bool> q;
        splitValues(rest, v, q, false);
        return Node::rule(key, Op::Contains, {v.value(0)});
    }

    if (!startsQuoted && rest == "*") return Node::rule(key, Op::NotEmpty, {});

    if (f.type == Type::Enum || f.type == Type::List) {
        QStringList v; QList<bool> q;
        splitValues(rest, v, q, true);
        QStringList keep;
        for (const QString &s : v) if (!s.trimmed().isEmpty()) keep << s.trimmed();
        return Node::rule(key, Op::AnyOf, keep);
    }

    if (f.type == Type::Number || f.type == Type::Date) {
        QString r = rest;
        Op op = Op::Is;
        if (!startsQuoted) {
            if (r.startsWith(">="))      { op = Op::Ge; r = r.mid(2); }
            else if (r.startsWith("<=")) { op = Op::Le; r = r.mid(2); }
            else if (r.startsWith('>'))  { op = Op::Gt; r = r.mid(1); }
            else if (r.startsWith('<'))  { op = Op::Lt; r = r.mid(1); }
            else if (r.startsWith('='))  { r = r.mid(1); }
            const int dots = r.indexOf("..");
            if (op == Op::Is && dots > 0) {
                QStringList a; QList<bool> q;
                splitValues(r.left(dots), a, q, false);
                QStringList b;
                splitValues(r.mid(dots + 2), b, q, false);
                return Node::rule(key, Op::Between, {a.value(0), b.value(0)});
            }
        }
        QStringList v; QList<bool> q;
        splitValues(r, v, q, false);
        return Node::rule(key, op, {v.value(0)});
    }

    // Text
    QString r = rest;
    if (!startsQuoted && r.startsWith('=')) {
        QStringList v; QList<bool> q;
        splitValues(r.mid(1), v, q, false);
        return Node::rule(key, Op::Is, {v.value(0)});
    }
    const bool lead = r.startsWith('*');
    const bool trail = r.size() > 1 && r.endsWith('*');
    const QString core = r.mid(lead ? 1 : 0, r.size() - (lead ? 1 : 0) - (trail ? 1 : 0));
    QStringList v; QList<bool> q;
    splitValues(core, v, q, false);
    const QString value = v.value(0);
    if (lead && trail) return Node::rule(key, Op::Contains, {value});
    if (trail)         return Node::rule(key, Op::StartsWith, {value});
    if (lead)          return Node::rule(key, Op::EndsWith, {value});
    return Node::rule(key, Op::Contains, {value});
}

struct Parser
{
    QList<Tok> toks;
    int i = 0;
    QString err;

    bool isOp(const Tok &t, const char *s) const { return !t.quoted && t.text == s; }

    Node termNode(const Tok &t)
    {
        if (t.quoted) return Node::rule("text", Op::Contains, {t.text});
        QString s = t.text;
        if (s.size() > 1 && s.startsWith('-')) {
            Tok rest{s.mid(1), false};
            return negate(termNode(rest));
        }
        const int c = s.indexOf(':');
        if (c > 0 && hasQualifier(s)) {
            const QString key = s.left(c);
            if (const Field *f = field(key)) return qualifiedRule(*f, s.mid(c + 1));
            if (err.isEmpty())
                err = QString("\"%1\" is not a field. Searched for it as a word.").arg(key);
        }
        return Node::rule("text", Op::Contains, {s});
    }

    bool unary(Node &out, int depth)
    {
        if (i >= toks.size()) return false;
        const Tok t = toks.at(i);
        if (isOp(t, "NOT") || isOp(t, "-")) {
            ++i;
            Node u;
            if (!unary(u, depth)) return false;
            out = negate(u);
            return true;
        }
        if (isOp(t, "(")) {
            ++i;
            out = orExpr(depth + 1);
            if (i < toks.size() && isOp(toks.at(i), ")")) ++i;
            return true;
        }
        if (isOp(t, ")")) { ++i; return false; }
        ++i;
        out = termNode(t);
        return true;
    }

    Node orExpr(int depth)
    {
        QList<Node> alts;
        Node cur = Node::group(Node::All);
        bool sawOr = false;
        while (i < toks.size()) {
            const Tok &t = toks.at(i);
            if (isOp(t, ")")) {
                if (depth > 0) break;
                ++i;                                    // a stray ')': dropped
                continue;
            }
            if (isOp(t, "OR")) { ++i; alts << cur; cur = Node::group(Node::All); sawOr = true;
                                 continue; }
            if (isOp(t, "AND")) { ++i; continue; }
            Node u;
            if (unary(u, depth)) cur.kids << u;
        }
        alts << cur;
        if (!sawOr) {
            /*  A group holding only one group IS that group, so "((a b))" and the
                "(a b)" it means print alike. */
            if (cur.kids.size() == 1 && cur.kids.first().kind == Node::Group)
                return cur.kids.first();
            return cur;
        }
        Node any = Node::group(Node::Any);
        for (const Node &a : alts) {
            if (a.kids.isEmpty()) continue;             // "a OR OR b"
            any.kids << (a.kids.size() == 1 ? a.kids.first() : a);
        }
        return any;
    }
};

bool needsQuotes(const QString &v)
{
    if (v.isEmpty()) return true;
    if (v == "OR" || v == "AND" || v == "NOT") return true;
    if (v.startsWith('-') || v.startsWith('<') || v.startsWith('>') || v.startsWith('=')
        || v.startsWith('*') || v.endsWith('*') || v.contains("..")) return true;
    for (const QChar c : v)
        if (c.isSpace() || c == '(' || c == ')' || c == ',' || c == ':') return true;
    return false;
}

QString q(const QString &raw)
{
    QString v = raw;
    v.remove('"');                          // the grammar has no escape for a quote
    return needsQuotes(v) ? "\"" + v + "\"" : v;
}

QString ruleText(const Node &n)
{
    const Field *f = field(n.field);
    const QString k = f ? f->key : n.field;
    const QString v0 = n.values.value(0);
    const QString v1 = n.values.value(1);
    auto list = [&n]() {
        QStringList parts;
        for (const QString &v : n.values) parts << q(v);
        return parts.join(',');
    };

    if (f && f->type == Type::Words) {
        if (n.op == Op::NotContains) return "-" + q(v0);
        return q(v0);
    }
    switch (n.op) {
    case Op::Contains:    return k + ":" + q(v0);
    case Op::NotContains: return "-" + k + ":" + q(v0);
    case Op::Is:
        if (f && f->type == Type::Text) return k + ":=" + q(v0);
        return k + ":" + q(v0);
    case Op::IsNot:
        if (f && f->type == Type::Text) return "-" + k + ":=" + q(v0);
        return "-" + k + ":" + q(v0);
    case Op::StartsWith:  return k + ":" + q(v0) + "*";
    case Op::EndsWith:    return k + ":*" + q(v0);
    case Op::Empty:       return "-" + k + ":*";
    case Op::NotEmpty:    return k + ":*";
    case Op::Lt:          return k + ":<" + q(v0);
    case Op::Le:          return k + ":<=" + q(v0);
    case Op::Gt:          return k + ":>" + q(v0);
    case Op::Ge:          return k + ":>=" + q(v0);
    case Op::Between:     return k + ":" + q(v0) + ".." + q(v1);
    case Op::AnyOf:       return k + ":" + list();
    case Op::NoneOf:      return "-" + k + ":" + list();
    }
    return QString();
}

QString nodeText(const Node &n, bool root)
{
    if (n.kind == Node::Rule) return ruleText(n);
    QStringList parts;
    for (const Node &k : n.kids) parts << nodeText(k, false);
    switch (n.match) {
    case Node::All: {
        const QString s = parts.join(' ');
        return root ? s : "(" + s + ")";
    }
    case Node::Any: {
        const QString s = parts.join(" OR ");
        return root ? s : "(" + s + ")";
    }
    case Node::None:
        if (parts.isEmpty()) return root ? QString() : "()";
        if (parts.size() == 1) return "-" + parts.first();
        return "-(" + parts.join(" OR ") + ")";
    }
    return QString();
}

bool nodeIsEmpty(const Node &n)
{
    if (n.kind == Node::Rule) return false;
    for (const Node &k : n.kids) if (!nodeIsEmpty(k)) return false;
    return true;
}

/* ---------------------------------------------------------------------------------
   JSON
   --------------------------------------------------------------------------------- */

QJsonObject nodeJson(const Node &n)
{
    QJsonObject o;
    if (n.kind == Node::Rule) {
        o["f"] = n.field;
        o["op"] = opName(n.op);
        o["v"] = QJsonArray::fromStringList(n.values);
        return o;
    }
    QJsonArray kids;
    for (const Node &k : n.kids) kids << nodeJson(k);
    o[n.match == Node::All ? "all" : n.match == Node::Any ? "any" : "none"] = kids;
    return o;
}

Node nodeFromJson(const QJsonObject &o)
{
    for (const char *g : {"all", "any", "none"}) {
        if (!o.contains(g)) continue;
        Node n = Node::group(QString(g) == "all" ? Node::All
                             : QString(g) == "any" ? Node::Any : Node::None);
        for (const QJsonValue &v : o.value(g).toArray()) n.kids << nodeFromJson(v.toObject());
        return n;
    }
    Op op = Op::Contains;
    opFromName(o.value("op").toString(), op);
    QStringList vals;
    for (const QJsonValue &v : o.value("v").toArray()) vals << v.toString();
    return Node::rule(o.value("f").toString(), op, vals);
}

/* ---------------------------------------------------------------------------------
   Evaluation
   --------------------------------------------------------------------------------- */

void prepareNode(Node &n)
{
    if (n.kind == Node::Group) {
        for (Node &k : n.kids) prepareNode(k);
        return;
    }
    n.prepValues.clear();
    n.prepNumbers.clear();
    const Field *f = field(n.field);
    if (!f) return;
    for (const QString &v : n.values) {
        if (f->type == Type::Enum) n.prepValues << canon(v);
        else if (f->type == Type::List && gResolver) {
            const QStringList r = gResolver(f->key, v.trimmed());
            n.prepValues << (r.isEmpty() ? QStringList{v.trimmed()} : r);
        }
        else n.prepValues << v.trimmed();
        double d = 0;
        n.prepNumbers << (toNumber(v, d) ? d * f->scale : std::nan(""));
    }
}

bool ruleMatches(const Node &n, const std::function<QVariant(int)> &valueFor)
{
    const Field *f = field(n.field);
    if (!f) return true;                        // an unknown field restricts nothing
    const QVariant data = valueFor(f->column);
    const QString v0 = n.prepValues.value(0);

    switch (f->type) {
    case Type::Words:
    case Type::Text: {
        const QString s = data.typeId() == QMetaType::QStringList
                              ? data.toStringList().join(' ') : data.toString();
        switch (n.op) {
        case Op::Contains:    return s.contains(v0, Qt::CaseInsensitive);
        case Op::NotContains: return !s.contains(v0, Qt::CaseInsensitive);
        case Op::Is:          return s.trimmed().compare(v0, Qt::CaseInsensitive) == 0;
        case Op::IsNot:       return s.trimmed().compare(v0, Qt::CaseInsensitive) != 0;
        case Op::StartsWith:  return s.trimmed().startsWith(v0, Qt::CaseInsensitive);
        case Op::EndsWith:    return s.trimmed().endsWith(v0, Qt::CaseInsensitive);
        case Op::Empty:       return s.trimmed().isEmpty();
        case Op::NotEmpty:    return !s.trimmed().isEmpty();
        default:              return true;
        }
    }
    case Type::Number: {
        const QString s = data.toString().trimmed();
        double x = 0;
        bool have = toNumber(s, x);
        if (!have && s.isEmpty() && emptyIsZero(f->key)) { x = 0; have = true; }
        if (n.op == Op::Empty)    return !have;
        if (n.op == Op::NotEmpty) return have;
        if (!have) return n.op == Op::IsNot;
        const double a = n.prepNumbers.value(0, std::nan(""));
        const double b = n.prepNumbers.value(1, std::nan(""));
        if (std::isnan(a)) return true;         // no usable value: no restriction
        switch (n.op) {
        case Op::Is:      return nearlyEqual(x, a);
        case Op::IsNot:   return !nearlyEqual(x, a);
        case Op::Lt:      return x < a && !nearlyEqual(x, a);
        case Op::Le:      return x < a || nearlyEqual(x, a);
        case Op::Gt:      return x > a && !nearlyEqual(x, a);
        case Op::Ge:      return x > a || nearlyEqual(x, a);
        case Op::Between:
            if (std::isnan(b)) return true;
            return (x > qMin(a, b) || nearlyEqual(x, qMin(a, b)))
                && (x < qMax(a, b) || nearlyEqual(x, qMax(a, b)));
        default:          return true;
        }
    }
    case Type::Date: {
        /*  Dates are held as "yyyy-MM-dd hh:mm:ss.zzz", so a value is compared with the
            same-length PREFIX of it: "2024-05" is all of May, and "before 2024-05" is
            before May began. */
        const QString s = data.toString().trimmed();
        if (n.op == Op::Empty)    return s.isEmpty();
        if (n.op == Op::NotEmpty) return !s.isEmpty();
        if (s.isEmpty()) return n.op == Op::IsNot;
        if (v0.isEmpty()) return true;
        auto cmp = [&s](const QString &v) { return s.left(v.size()).compare(v); };
        switch (n.op) {
        case Op::Is:      return cmp(v0) == 0;
        case Op::IsNot:   return cmp(v0) != 0;
        case Op::Lt:      return cmp(v0) < 0;
        case Op::Le:      return cmp(v0) <= 0;
        case Op::Gt:      return cmp(v0) > 0;
        case Op::Ge:      return cmp(v0) >= 0;
        case Op::Between: {
            const QString v1 = n.prepValues.value(1);
            if (v1.isEmpty()) return true;
            const QString lo = qMin(v0, v1), hi = qMax(v0, v1);
            return cmp(lo) >= 0 && cmp(hi) <= 0;
        }
        default:          return true;
        }
    }
    case Type::Enum: {
        const QString s = canon(data.toString());
        const bool hit = n.prepValues.contains(s);
        if (n.op == Op::NoneOf) return !hit;
        return n.prepValues.isEmpty() || hit;
    }
    case Type::List: {
        const QStringList list = data.typeId() == QMetaType::QStringList
                                     ? data.toStringList()
                                     : (data.toString().isEmpty() ? QStringList()
                                                                  : QStringList{data.toString()});
        if (n.op == Op::Empty)    return list.isEmpty();
        if (n.op == Op::NotEmpty) return !list.isEmpty();
        bool hit = false;
        for (const QString &v : n.prepValues)
            if (list.contains(v, Qt::CaseInsensitive)) { hit = true; break; }
        if (n.op == Op::NoneOf) return !hit;
        return n.prepValues.isEmpty() || hit;
    }
    }
    return true;
}

bool nodeMatches(const Node &n, const std::function<QVariant(int)> &valueFor)
{
    if (n.kind == Node::Rule) return ruleMatches(n, valueFor);
    /*  An EMPTY group restricts nothing, whatever its match: a group the user has just
        added in the builder must not empty the grid before it has a rule in it. */
    if (n.kids.isEmpty()) return true;
    switch (n.match) {
    case Node::All:
        for (const Node &k : n.kids) if (!nodeMatches(k, valueFor)) return false;
        return true;
    case Node::Any:
        for (const Node &k : n.kids) if (nodeMatches(k, valueFor)) return true;
        return false;
    case Node::None:
        for (const Node &k : n.kids) if (nodeMatches(k, valueFor)) return false;
        return true;
    }
    return true;
}

void collectColumns(const Node &n, QSet<int> &out)
{
    if (n.kind == Node::Rule) {
        if (const Field *f = field(n.field)) out.insert(f->column);
        return;
    }
    for (const Node &k : n.kids) collectColumns(k, out);
}

}   // namespace

Expr Expr::parse(const QString &text)
{
    Parser p;
    p.toks = tokenize(text);
    Expr e;
    Node n = p.orExpr(0);
    if (n.kind == Node::Rule) {
        Node g = Node::group(Node::All);
        g.kids << n;
        n = g;
    }
    e.root = n;
    e.err = p.err;
    return e;
}

QString Expr::toText() const { return nodeText(root, true); }

QJsonObject Expr::toJson() const
{
    QJsonObject o;
    o["version"] = 1;
    o["root"] = nodeJson(root);
    return o;
}

QString Expr::toJsonText() const
{
    return QString::fromUtf8(QJsonDocument(toJson()).toJson(QJsonDocument::Compact));
}

Expr Expr::fromJson(const QJsonObject &o)
{
    Expr e;
    if (o.contains("root")) e.root = nodeFromJson(o.value("root").toObject());
    if (e.root.kind == Node::Rule) {
        Node g = Node::group(Node::All);
        g.kids << e.root;
        e.root = g;
    }
    return e;
}

Expr Expr::fromJsonText(const QString &json)
{
    return fromJson(QJsonDocument::fromJson(json.toUtf8()).object());
}

bool Expr::isEmpty() const { return nodeIsEmpty(root); }

void Expr::prepare() { prepareNode(root); }

bool Expr::matches(const std::function<QVariant(int)> &valueFor) const
{
    return nodeMatches(root, valueFor);
}

QSet<int> Expr::columnsRead() const
{
    QSet<int> out;
    collectColumns(root, out);
    return out;
}

}   // namespace Query
