#ifndef SEARCHTERMS_H
#define SEARCHTERMS_H

#include <QList>
#include <QString>
#include <QStringList>

/*
    One grammar for both search boxes. See notes/Documentation.txt "Searching Folders and
    Catalog".

    WHY THIS EXISTS. Winnow has two searches that look identical and behaved differently.
    F2 searches the DATAMODEL (Filters > Search) and did a plain QString::contains;
    The Catalog scope searches the INDEX and hands the text to FTS5. The two
    shortcuts are deliberately paired to read as "search here" and "search everywhere",
    which promises that the same words mean the same thing -- and they did not. "heron OR
    eagle" found images in one and nothing in the other, because to contains() it was a
    single literal string containing the word OR.

    So the text is parsed ONCE, here, into an expression, and each search compiles that
    expression its own way. Neither box owns the grammar any more, and a term type added
    here reaches both at the same time.

    THE GRAMMAR, kept to what a search box is expected to do:

        heron nanaimo           both must appear              (AND is the default)
        heron OR eagle          either may appear
        (heron OR eagle) tide   brackets group
        "great blue"            the phrase, not the two words
        -heron                  must NOT appear
        NOT heron               the same thing, spelled out

    AND is accepted and ignored, because it is the default and users type it anyway.
    OR and NOT are recognised in UPPER CASE only, so a photograph captioned "black or
    white" is searched for as written rather than parsed as an operator.

    AND BINDS TIGHTER THAN OR, the conventional precedence, so "heron OR eagle nanaimo"
    is heron OR (eagle AND nanaimo). It used to be the other way round -- OR bound to the
    term before it, making that query (heron OR eagle) AND nanaimo -- because a flat list
    of groups was all the parser could express and the tighter reading had no way to be
    written down. Brackets are that way, so the odd rule is gone and the query above is
    now spelled with them.

    A BRACKET IS SYNTAX WHEREVER IT IS UNQUOTED, including mid-word: a filename that
    really contains one is searched for by quoting it ("DSC(2)"). Unbalanced brackets are
    tolerated rather than rejected -- a missing ')' closes at the end of the text and a
    stray one is dropped -- because the box is read while it is being typed and half a
    query should narrow, not fail.

    NEGATION APPLIES TO THE WHOLE QUERY, wherever it is written. "-heron", and "(a OR
    -heron)" too, mean "and not heron": every negated term is lifted out of the
    expression at parse time into negatives. That is not a general boolean algebra, and
    it is deliberate -- FTS5 has no "everything" token for a NOT to subtract from, so the
    index applies negatives as a separate NOT EXISTS and could not honour one nested
    inside an OR. Lifting them HERE is what keeps the two engines agreeing: both
    compilations consume the same lifted form, so neither can quietly mean something the
    other does not.

    ONE DELIBERATE DIFFERENCE REMAINS between the two compilations, and it cannot be
    removed without making one of them worse. The datamodel matches a SUBSTRING and the
    index matches a PREFIX, because that is what each engine is good at: contains() over a
    few thousand loaded rows is free, while an infix search over a quarter of a million
    FTS rows cannot use the index at all. So "eron" finds Heron here and not everywhere.
    The narrowing direction is the same in both, which is what the pairing promises; the
    edge is documented rather than papered over.
*/

class SearchTerms
{
public:
    /*
        One node of the parsed expression. A Term is a word or a phrase; All and Any are
        its AND and OR. Nothing here is negated -- see the header: negatives are lifted
        out during the parse and held separately.

        An All with no children is the EMPTY expression, which matches everything. That
        is the right answer for "no restriction" and the wrong thing to present as a
        search, so callers test isEmpty() rather than running it.
    */
    struct Node
    {
        enum Kind { Term, All, Any };
        Kind kind = All;
        QString text;                   // Term only
        QList<Node> kids;               // All and Any only

        bool isNothing() const
        {
            return kind == Term ? text.isEmpty() : kids.isEmpty();
        }
    };

    static SearchTerms parse(const QString &raw)
    {
        SearchTerms t;
        const QList<Token> toks = tokenize(raw);
        int i = 0;
        t.positive = parseOr(toks, i, t.negatives, 0);
        return t;
    }

    bool isEmpty() const { return positive.isNothing() && negatives.isEmpty(); }

    /* True when there is something to FIND rather than only things to avoid. An
       all-negative query ("-heron") has nothing, which matters to the index: FTS5's MATCH
       has no "everything" token to subtract from, so the caller must apply the negatives
       as a separate NOT EXISTS rather than as one expression. */
    bool hasPositive() const { return !positive.isNothing(); }

    /*
        Does this text satisfy the query -- the DATAMODEL compilation.

        Case-insensitive on both sides. G::SearchTextColumn is written lower-cased on the
        metadata path but not on the file-data path that precedes it, so a case-sensitive
        compare here would quietly find different things depending on how far a row had
        got through loading.
    */
    bool matches(const QString &text) const
    {
        if (!evaluate(positive, text)) return false;
        for (const Node &n : negatives) if (evaluate(n, text)) return false;
        return true;
    }

    /* The FTS5 expression for what to FIND, or empty when there is nothing. */
    QString positiveFts() const { return ftsOf(positive, true); }
    /* The FTS5 expression for what to AVOID, or empty when there is nothing. The caller
       applies this as a NOT EXISTS, not as FTS5's NOT operator, which is binary and so
       cannot stand on its own. Several negatives are OR-ed, because that one NOT EXISTS
       must exclude an image carrying ANY of them. */
    QString negativeFts() const
    {
        QStringList parts;
        for (const Node &n : negatives) {
            const QString s = ftsOf(n, false);
            if (!s.isEmpty()) parts << s;
        }
        return parts.join(" OR ");
    }

    /* What to find, and what to exclude from it. See the header for why the second is a
       list rather than part of the first. */
    Node positive;
    QList<Node> negatives;

private:
    /*
        A token, and whether the user QUOTED it. The flag is what makes an operator
        escapable: "OR" between quotes is the word, "(" between quotes is the character,
        and neither reaches the parser as syntax.
    */
    struct Token
    {
        QString text;
        bool quoted = false;
    };

    /*
        Split on whitespace and brackets, keeping quoted runs together.

        A phrase keeps its quotes stripped here and is re-quoted on the way into FTS,
        because the datamodel wants the bare text and FTS wants it escaped -- the two
        engines disagree about what quoting means, so the parse holds neither's spelling.
    */
    static QList<Token> tokenize(const QString &raw)
    {
        QList<Token> out;
        QString cur;
        bool inQuote = false;

        auto flush = [&out, &cur](bool quoted) {
            if (cur.isEmpty()) return;
            out << Token{cur, quoted};
            cur.clear();
        };

        for (const QChar &c : raw) {
            if (c == '"') {
                /* A quote ENDS whatever was being accumulated, opening or closing. Both
                   halves matter: without the closing flush '""' would merge the words on
                   either side of it, and without the opening one 'say"what more"' would
                   glue 'say' onto the phrase and search for something the user never
                   typed. */
                flush(inQuote);
                inQuote = !inQuote;
                continue;
            }
            if (!inQuote && (c == '(' || c == ')')) {
                flush(false);
                out << Token{QString(c), false};
                continue;
            }
            if (!inQuote && c.isSpace()) {
                flush(false);
                continue;
            }
            cur += c;
        }
        /* An unterminated quote is still a phrase: the user is mid-way through typing it
           and the text before the cursor is what they have asked for so far. */
        flush(inQuote);
        return out;
    }

    static bool isWord(const Token &t, const char *w)
    {
        return !t.quoted && t.text == QLatin1String(w);
    }

    static Node nothing() { return Node{Node::All, QString(), {}}; }

    /* One child is not a group. Collapsing here is what keeps positiveFts from
       parenthesising every single term. */
    static Node simplify(Node n)
    {
        if (n.kind != Node::Term && n.kids.size() == 1) return n.kids.first();
        return n;
    }

    /*  depth is how many brackets are open, and the ONLY thing it decides is what a ')'
        means. Inside one it ends the group; at the top there is nothing to end, so it is
        a stray the user has yet to finish typing and is dropped -- without the depth,
        ")heron" would end a group that was never opened and silently discard the rest of
        the query. */
    static Node parseOr(const QList<Token> &toks, int &i, QList<Node> &negatives,
                        int depth)
    {
        Node n;
        n.kind = Node::Any;
        for (;;) {
            const Node a = parseAnd(toks, i, negatives, depth);
            if (!a.isNothing()) n.kids << a;
            if (i < toks.size() && isWord(toks.at(i), "OR")) { ++i; continue; }
            break;
        }
        return simplify(n);
    }

    static Node parseAnd(const QList<Token> &toks, int &i, QList<Node> &negatives,
                         int depth)
    {
        Node n;
        n.kind = Node::All;
        while (i < toks.size()) {
            const Token &tk = toks.at(i);
            if (isWord(tk, "OR")) break;
            if (isWord(tk, ")")) {
                if (depth > 0) break;
                ++i;                                    // stray close; see parseOr
                continue;
            }
            if (isWord(tk, "AND")) { ++i; continue; }   // the default; accept and ignore

            const int before = i;
            const Node u = parseUnary(toks, i, negatives, depth);
            if (!u.isNothing()) n.kids << u;
            /* Nothing consumed means nothing ever will be. Defensive: every path in
               parseUnary advances today, and a parser that can loop is worse than a
               parser that drops a token. */
            if (i == before) ++i;
        }
        return simplify(n);
    }

    static Node parseUnary(const QList<Token> &toks, int &i, QList<Node> &negatives,
                           int depth)
    {
        bool negate = false;

        while (i < toks.size()) {
            const Token &tk = toks.at(i);
            if (tk.quoted) break;
            if (tk.text == "NOT" || tk.text == "-") { negate = !negate; ++i; continue; }
            /* A leading '-' negates, but only when something follows it: a lone "-", or a
               hyphen inside a word (Nikon Z-9, 100-400mm), is text the user typed. */
            if (tk.text.size() > 1 && tk.text.startsWith('-')
                && !tk.text.startsWith("--")) {
                Node term;
                term.kind = Node::Term;
                term.text = tk.text.mid(1);
                ++i;
                /* The hyphen is one more negation, so "NOT -heron" is heron again --
                   the same double-negative the two spellings have to agree on. */
                if (!negate) { negatives << term; return nothing(); }
                return term;
            }
            break;
        }

        const Node p = parsePrimary(toks, i, negatives, depth);
        if (p.isNothing()) return nothing();
        if (negate) { negatives << p; return nothing(); }
        return p;
    }

    static Node parsePrimary(const QList<Token> &toks, int &i, QList<Node> &negatives,
                             int depth)
    {
        if (i >= toks.size()) return nothing();
        const Token tk = toks.at(i);

        if (isWord(tk, "(")) {
            ++i;
            const Node inner = parseOr(toks, i, negatives, depth + 1);
            /* A missing ')' closes at the end of the text rather than failing -- see the
               header on half-typed queries. */
            if (i < toks.size() && isWord(toks.at(i), ")")) ++i;
            return inner;
        }
        /* A stray ')' or a bare operator has no operand and is simply dropped. */
        if (isWord(tk, ")") || isWord(tk, "OR") || isWord(tk, "AND")
            || isWord(tk, "NOT")) {
            ++i;
            return nothing();
        }

        ++i;
        Node term;
        term.kind = Node::Term;
        term.text = tk.text;
        return term;
    }

    static bool evaluate(const Node &n, const QString &text)
    {
        switch (n.kind) {
        case Node::Term:
            return n.text.isEmpty() || text.contains(n.text, Qt::CaseInsensitive);
        case Node::Any:
            /* An empty Any cannot arise from the parser (simplify drops it), and if one
               ever did, "no alternatives" is no restriction. */
            if (n.kids.isEmpty()) return true;
            for (const Node &k : n.kids) if (evaluate(k, text)) return true;
            return false;
        case Node::All:
            for (const Node &k : n.kids) if (!evaluate(k, text)) return false;
            return true;
        }
        return true;
    }

    /*
        The INDEX compilation. topLevel suppresses the brackets around an outermost AND,
        which needs none and reads better without.
    */
    static QString ftsOf(const Node &n, bool topLevel)
    {
        if (n.isNothing()) return QString();

        if (n.kind == Node::Term) {
            QString q = n.text;
            /* FTS5 escapes a quote by doubling it. Defensive rather than load-bearing:
               tokenize() treats every quote as a phrase delimiter and keeps none, so no
               term reaching here contains one today. Kept so that changing the tokenizer
               cannot silently produce a broken MATCH. */
            q.replace('"', "\"\"");
            /* A phrase gets NO prefix star: "great blue"* would prefix-match only the
               last word, which is not what quoting it asked for. */
            const bool phrase = n.text.contains(' ');
            return '"' + q + (phrase ? "\"" : "\"*");
        }

        QStringList parts;
        for (const Node &k : n.kids) {
            const QString s = ftsOf(k, false);
            if (!s.isEmpty()) parts << s;
        }
        if (parts.isEmpty()) return QString();
        if (parts.size() == 1) return parts.first();

        const QString joined = parts.join(n.kind == Node::Any ? " OR " : " AND ");
        /* An OR is always bracketed: it is the operand of whatever encloses it, and at
           the top of the expression the brackets say which operator won. */
        if (n.kind == Node::Any) return "(" + joined + ")";
        return topLevel ? joined : "(" + joined + ")";
    }
};

#endif // SEARCHTERMS_H
