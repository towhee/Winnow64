#ifndef QUERYEXPR_H
#define QUERYEXPR_H

#include <QHash>
#include <QJsonObject>
#include <QList>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QVariant>
#include <QVector>
#include <functional>

/*
    A QUERY: the structured search behind the Filters Search row, the Query Builder and
    the Queries panel. See notes/Documentation.txt "Queries and the Query Builder".

    ONE EXPRESSION, TWO SPELLINGS. A query is a tree -- groups (match ALL / ANY / NONE of
    their children) of rules (field + operator + values) -- and it can be written as TEXT
    in the Search row or built with the Query Builder. The two are views of the same
    thing: parse() and toText() round-trip, so a query built by hand can be edited as
    text and reopened in the builder without loss. Saved Queries are stored as JSON
    (toJson), a PUBLISHED FORMAT: fields and operators are added, never renamed.

    THE TEXT GRAMMAR extends the Search row's (Utilities/searchterms.h), which it
    replaces for the datamodel. Everything that grammar accepted means what it did:

        heron nanaimo                  both words appear anywhere (AND is the default)
        heron OR eagle                 either (AND binds tighter than OR)
        "great blue"                   the phrase
        -heron   NOT heron             must not appear
        (heron OR eagle) tide          brackets group

    and a word may now be QUALIFIED by a field:

        rating:>=3   rating:3..5       numbers: = < <= > >= and a..b ranges
        label:red,yellow               any of these values; -label:red none of them
        keyword:"Fauna|Bird"           has this keyword (or one beneath it)
        title:heron   title:=Heron     contains / is exactly
        title:her*   title:*ron        starts with / ends with
        title:*      -title:*          has any value / is empty
        captured:2024-05               a date or a prefix of one (year, month, day)
        captured:<2024-05-01           before / after (> ) / a..b between
        collection:"Trip 2024"         in this collection

    OR and NOT are operators in UPPER CASE only, as before. A quoted value is literal:
    no wildcard, no comparison prefix. A word whose "field" is not a field name is an
    ERROR (reported by error(), and searched as a plain word meanwhile) -- unless it is
    not a plain name at all (10:30, http://...), which is simply a word.

    NEGATION IS NOT LIFTED. The old grammar moved every negative to the top of the query
    because FTS5 could not express a nested NOT. This evaluator has no such limit:
    "(heron OR -eagle)" means what it says. The catalog's FTS compilation (SearchTerms)
    still lifts; it is only used by a CAPPED catalog search.
*/
namespace Query {

enum class Type { Words, Text, Number, Date, Enum, List };

/*  Stored in JSON by NAME (opName/opFromName), never by number. */
enum class Op {
    Contains, NotContains, Is, IsNot, StartsWith, EndsWith, Empty, NotEmpty,
    Lt, Le, Gt, Ge, Between,        // numbers; dates use Lt/Gt as before/after
    AnyOf, NoneOf                   // enums and lists
};

struct Field
{
    QString key;            // the qualifier in text and the id in JSON: "rating"
    QString label;          // shown in the builder: "Rating"
    int column = -1;        // G::dataModelColumns the value is read from
    Type type = Type::Text;
    QStringList values;     // an Enum's choices (display spelling)
    QString hint;           // placeholder for the value editor
    /*  A number field's unit: what is typed is multiplied by this before comparing with
        the column (File size is typed in MB and held in bytes). */
    double scale = 1.0;
    /*  One of the REST OF THE DATAMODEL's user-facing columns, listed after the main
        fields (and a separator) in the builder. */
    bool extra = false;
};

/*  The registry, in builder order: the main fields, then the extras (Field::extra),
    sorted by label. */
const QVector<Field> &fields();
const Field *field(const QString &key);         // case-insensitive; nullptr if unknown
/*  The operators a field type offers, in builder order, and their builder labels. */
QList<Op> opsFor(Type t);
QString opLabel(Op op, Type t);
QString opName(Op op);
bool opFromName(const QString &name, Op &op);
/*  Operators that take no value / two values. */
bool opTakesNoValue(Op op);
bool opTakesTwoValues(Op op);

/*
    LIST VALUES THAT ARE NAMES FOR SOMETHING ELSE. G::CollectionsColumn holds collection
    ids, but a person writes collection:"Trip 2024". The resolver turns a field's written
    value into the value(s) the column holds (a name can match more than one collection).
    MW installs it; with none installed the value is compared as written. Applied once
    per compile (Expr::prepare), never per row.
*/
using Resolver = std::function<QStringList(const QString &fieldKey, const QString &value)>;
void setResolver(Resolver r);

struct Node
{
    enum Kind { Group, Rule };
    enum Match { All, Any, None };

    Kind kind = Group;
    // Group
    Match match = All;
    QList<Node> kids;
    // Rule
    QString field;
    Op op = Op::Contains;
    QStringList values;

    // filled by Expr::prepare: folded / numeric / resolved forms of values
    QStringList prepValues;
    QVector<double> prepNumbers;

    static Node group(Match m) { Node n; n.kind = Group; n.match = m; return n; }
    static Node rule(const QString &f, Op o, const QStringList &v)
    {
        Node n; n.kind = Rule; n.field = f; n.op = o; n.values = v; return n;
    }
    bool operator==(const Node &o) const;
};

class Expr
{
public:
    Expr() : root(Node::group(Node::All)) {}

    static Expr parse(const QString &text);
    static Expr fromJson(const QJsonObject &o);
    static Expr fromJsonText(const QString &json);

    QString toText() const;
    QJsonObject toJson() const;
    QString toJsonText() const;

    /*  No rules anywhere: matches every row and is not a search. */
    bool isEmpty() const;
    /*  Why the text did not fully parse, or empty. The expression is still usable. */
    QString error() const { return err; }

    /*  Fold values, parse numbers and run the resolver. Call once before matches(); a
        copy made after prepare() stays prepared. */
    void prepare();
    /*  Does the row satisfy the query? valueFor(column) returns the row's EditRole value
        for a G:: column -- the same callable FilterPredicate::accepts uses. */
    bool matches(const std::function<QVariant(int)> &valueFor) const;
    /*  Every G:: column the query reads -- so an edit to one of them refilters. */
    QSet<int> columnsRead() const;

    Node root;

private:
    QString err;
};

}   // namespace Query

#endif // QUERYEXPR_H
