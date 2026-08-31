#ifndef SEARCHTERMS_H
#define SEARCHTERMS_H

#include <QString>
#include <QStringList>

/*
    One grammar for both search boxes. See notes/Documentation.txt "Searching Here and
    Everywhere".

    WHY THIS EXISTS. Winnow has two searches that look identical and behaved differently.
    F2 searches the DATAMODEL (Filters > Search) and did a plain QString::contains;
    Shift+F2 searches the INDEX (the Catalog dock) and hands the text to FTS5. The two
    shortcuts are deliberately paired to read as "search here" and "search everywhere",
    which promises that the same words mean the same thing -- and they did not. "heron OR
    eagle" found images in one and nothing in the other, because to contains() it was a
    single literal string containing the word OR.

    So the text is parsed ONCE, here, into terms, and each search compiles those terms its
    own way. Neither box owns the grammar any more, and a term type added here reaches
    both at the same time.

    THE GRAMMAR, kept to what a search box is expected to do:

        heron nanaimo     both must appear                    (AND is the default)
        heron OR eagle    either may appear
        "great blue"      the phrase, not the two words
        -heron            must NOT appear
        NOT heron         the same thing, spelled out

    AND is accepted and ignored, because it is the default and users type it anyway.
    OR and NOT are recognised in UPPER CASE only, so a photograph captioned "black or
    white" is searched for as written rather than parsed as an operator.

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
    /* One thing the user asked for. alternatives are OR-ed together ("heron OR eagle" is
       one Group with two alternatives); every Group must be satisfied. */
    struct Group
    {
        QStringList alternatives;
        bool negated = false;
    };

    static SearchTerms parse(const QString &raw)
    {
        SearchTerms t;
        const QStringList tokens = tokenize(raw);

        bool negateNext = false;
        bool orPending = false;

        for (const QString &tok : tokens) {
            if (tok == "AND") continue;             // the default; accept and ignore
            if (tok == "OR")  { orPending = true;  continue; }
            if (tok == "NOT") { negateNext = true; continue; }

            QString word = tok;
            /* A leading '-' negates, but only when something follows it: a lone "-", or a
               hyphen inside a word (Nikon Z-9), is text the user typed. */
            if (word.size() > 1 && word.startsWith('-') && !word.startsWith("--")) {
                negateNext = true;
                word = word.mid(1);
            }
            if (word.isEmpty()) continue;

            /* OR binds to the group before it. "heron OR eagle OR osprey" is one group of
               three, not three groups. An OR with nothing before it is a stray operator
               and the word simply starts a new group. */
            if (orPending && !t.groups.isEmpty()) {
                t.groups.last().alternatives << word;
            }
            else {
                Group g;
                g.alternatives << word;
                g.negated = negateNext;
                t.groups << g;
            }
            orPending = false;
            negateNext = false;
        }
        return t;
    }

    bool isEmpty() const { return groups.isEmpty(); }

    /* True when at least one group is something to FIND rather than to avoid. An
       all-negative query ("-heron") has none, which matters to the index: FTS5's MATCH
       has no "everything" token to subtract from, so the caller must apply the negatives
       as a separate NOT EXISTS rather than as one expression. */
    bool hasPositive() const
    {
        for (const Group &g : groups) if (!g.negated) return true;
        return false;
    }

    /*
        Does this text satisfy the query -- the DATAMODEL compilation.

        Case-insensitive on both sides. G::SearchTextColumn is written lower-cased on the
        metadata path but not on the file-data path that precedes it, so a case-sensitive
        compare here would quietly find different things depending on how far a row had
        got through loading.
    */
    bool matches(const QString &text) const
    {
        for (const Group &g : groups) {
            bool hit = false;
            for (const QString &a : g.alternatives) {
                if (text.contains(a, Qt::CaseInsensitive)) { hit = true; break; }
            }
            if (hit == g.negated) return false;
        }
        return true;
    }

    /* The FTS5 expression for the groups to FIND, or empty when there are none. */
    QString positiveFts() const { return ftsFor(false); }
    /* The FTS5 expression for the groups to AVOID, or empty when there are none. The
       caller applies this as a NOT EXISTS, not as FTS5's NOT operator, which is binary
       and so cannot stand on its own. */
    QString negativeFts() const { return ftsFor(true); }

    QList<Group> groups;

private:
    /*
        Split on whitespace, keeping quoted runs together.

        A phrase keeps its quotes stripped here and is re-quoted on the way into FTS,
        because the datamodel wants the bare text and FTS wants it escaped -- the two
        engines disagree about what quoting means, so the parse holds neither's spelling.
    */
    static QStringList tokenize(const QString &raw)
    {
        QStringList out;
        QString cur;
        bool inQuote = false;

        for (const QChar &c : raw) {
            if (c == '"') {
                /* A quote ENDS whatever was being accumulated, opening or closing. Both
                   halves matter: without the closing flush '""' would merge the words on
                   either side of it, and without the opening one 'say"what more"' would
                   glue 'say' onto the phrase and search for something the user never
                   typed. */
                if (inQuote || !cur.isEmpty()) { out << cur; cur.clear(); }
                inQuote = !inQuote;
                continue;
            }
            if (!inQuote && c.isSpace()) {
                if (!cur.isEmpty()) { out << cur; cur.clear(); }
                continue;
            }
            cur += c;
        }
        if (!cur.isEmpty()) out << cur;

        out.removeAll(QString());
        return out;
    }

    /*
        Compile to FTS5.

        EVERY TERM IS QUOTED before '*' is appended. Unquoted, a term containing any of
        FTS5's punctuation is a syntax error, and a syntax error in MATCH fails the WHOLE
        query rather than returning nothing -- so a user typing an apostrophe or a hyphen
        mid-word would watch the search break instead of narrow.

        A multi-word phrase is NOT given a prefix '*'. "great blue*" would prefix-match
        only the last word, which is not what quoting it asked for.
    */
    QString ftsFor(bool negated) const
    {
        QStringList andParts;
        for (const Group &g : groups) {
            if (g.negated != negated) continue;
            QStringList orParts;
            for (const QString &a : g.alternatives) {
                QString q = a;
                /* FTS5 escapes a quote by doubling it. Defensive rather than
                   load-bearing: tokenize() treats every quote as a phrase delimiter and
                   keeps none, so no term reaching here contains one today. Kept so that
                   changing the tokenizer cannot silently produce a broken MATCH. */
                q.replace('"', "\"\"");
                const bool phrase = a.contains(' ');
                orParts << ('"' + q + (phrase ? "\"" : "\"*"));
            }
            if (orParts.isEmpty()) continue;
            andParts << (orParts.size() > 1 ? "(" + orParts.join(" OR ") + ")"
                                            : orParts.first());
        }
        /* Negatives are OR-ed: "not heron and not eagle" excludes an image carrying
           EITHER, which is what the caller's single NOT EXISTS has to be given. */
        if (negated) return andParts.join(" OR ");
        return andParts.join(" AND ");
    }
};

#endif // SEARCHTERMS_H
