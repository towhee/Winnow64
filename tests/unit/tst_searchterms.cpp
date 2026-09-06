#include <QtTest>

#include "Utilities/searchterms.h"

/*
    The shared search grammar (Utilities/searchterms.h).

    WHAT IS ACTUALLY BEING PINNED is that ONE parse drives two very different engines.
    Winnow has two search scopes -- Folders over the datamodel and Catalog over the index --
    deliberately paired as "here" and "everywhere", which promises the same words mean the
    same thing. They did not: to the datamodel's QString::contains, "heron OR eagle" was a
    single literal string containing the word OR.

    So most of these assert the SAME query both ways: matches() is what the datamodel
    does, positiveFts()/negativeFts() are what the catalog does, and they must agree about
    which pictures are wanted.

    THE PARSE IS AN EXPRESSION TREE, not a flat list of groups, since brackets were added.
    The shape assertions therefore read positive/negatives rather than groups, and the two
    rules brackets brought with them -- AND binding tighter than OR, and a negation being
    lifted out of wherever it was written -- are pinned in their own tests below.
*/
class tst_searchterms : public QObject
{
    Q_OBJECT

private slots:
    void bareWordsAreAnded();
    void orGroupsAlternatives();
    void negationExcludes();
    void quotedPhraseStaysTogether();
    void bracketsGroup();
    void andBindsTighterThanOr();
    void negationInsideBracketsIsLifted();
    void hyphenInsideAWordIsNotNegation();
    void emptyAndWhitespaceParseToNothing();
    void ftsTermsAreQuoted();
    void allNegativeHasNoPositive();
};

void tst_searchterms::bareWordsAreAnded()
{
    const SearchTerms t = SearchTerms::parse("heron nanaimo");
    QCOMPARE(t.positive.kind, SearchTerms::Node::All);
    QCOMPARE(t.positive.kids.size(), 2);

    QVERIFY(t.matches("heron at nanaimo harbour"));
    QVERIFY(!t.matches("heron at victoria"));       // one term missing is a miss
    QVERIFY(!t.matches("nanaimo harbour"));

    /* Case is ignored on both sides: a keyword may have been capitalised by whichever
       application wrote it, and the user is typing from memory. */
    QVERIFY(t.matches("HERON AT NANAIMO"));

    QCOMPARE(t.positiveFts(), QString("\"heron\"* AND \"nanaimo\"*"));
    QVERIFY(t.negativeFts().isEmpty());
}

void tst_searchterms::orGroupsAlternatives()
{
/*
    The query that used to mean two different things in the two boxes.
*/
    const SearchTerms t = SearchTerms::parse("heron OR eagle");
    QCOMPARE(t.positive.kind, SearchTerms::Node::Any);
    QCOMPARE(t.positive.kids.size(), 2);

    QVERIFY(t.matches("a heron"));
    QVERIFY(t.matches("an eagle"));
    QVERIFY(!t.matches("an osprey"));

    QCOMPARE(t.positiveFts(), QString("(\"heron\"* OR \"eagle\"*)"));

    /* OR chains into ONE node of three alternatives rather than nesting one per
       operator, so the FTS expression is flat and readable. */
    const SearchTerms three = SearchTerms::parse("heron OR eagle OR osprey");
    QCOMPARE(three.positive.kind, SearchTerms::Node::Any);
    QCOMPARE(three.positive.kids.size(), 3);
    QVERIFY(three.matches("an osprey"));
    QCOMPARE(three.positiveFts(),
             QString("(\"heron\"* OR \"eagle\"* OR \"osprey\"*)"));
}

void tst_searchterms::bracketsGroup()
{
/*
    Brackets are the reason the precedence rule below can be conventional: whatever the
    default grouping is, the other one has to be expressible.
*/
    const SearchTerms t = SearchTerms::parse("(heron OR eagle) nanaimo");
    QCOMPARE(t.positive.kind, SearchTerms::Node::All);
    QCOMPARE(t.positive.kids.size(), 2);
    QCOMPARE(t.positive.kids.first().kind, SearchTerms::Node::Any);

    QVERIFY(t.matches("eagle at nanaimo"));
    QVERIFY(t.matches("heron at nanaimo"));
    QVERIFY(!t.matches("eagle at victoria"));       // the AND still has to be satisfied
    QVERIFY(!t.matches("an osprey at nanaimo"));

    QCOMPARE(t.positiveFts(), QString("(\"heron\"* OR \"eagle\"*) AND \"nanaimo\"*"));

    /* Nesting, and a redundant bracket, which must not survive into the expression. */
    QCOMPARE(SearchTerms::parse("((a b) OR c) d").positiveFts(),
             QString("((\"a\"* AND \"b\"*) OR \"c\"*) AND \"d\"*"));
    QCOMPARE(SearchTerms::parse("(a)").positiveFts(), QString("\"a\"*"));

    /*  UNBALANCED IS TOLERATED, NOT REJECTED. The box is read while it is being typed,
        so a missing ')' closes at the end of the text and a stray one is dropped --
        half a query narrows rather than failing. */
    QCOMPARE(SearchTerms::parse("(a OR b").positiveFts(), QString("(\"a\"* OR \"b\"*)"));
    QCOMPARE(SearchTerms::parse("a)").positiveFts(), QString("\"a\"*"));
    /*  A stray ')' must not eat the rest of the query, which is what it did before the
        parser knew how many brackets were open: at the top there is no group to end, so
        it is dropped and the terms around it survive. */
    QCOMPARE(SearchTerms::parse(")a").positiveFts(), QString("\"a\"*"));
    QCOMPARE(SearchTerms::parse("a) b").positiveFts(), QString("\"a\"* AND \"b\"*"));

    /*  A BRACKET IS SYNTAX WHEREVER IT IS UNQUOTED, so a bare one is no longer a term.
        Quoting is the escape hatch, and it is the only one -- pinned because the old
        parser searched for "(((" literally and someone relying on that has to be told
        what to type instead. */
    QVERIFY(SearchTerms::parse("(((").isEmpty());
    QCOMPARE(SearchTerms::parse("\"(((\"").positiveFts(), QString("\"(((\"*"));
    QVERIFY(SearchTerms::parse("\"DSC(2)\"").matches("DSC(2).jpg"));
}

void tst_searchterms::andBindsTighterThanOr()
{
/*
    THE CONVENTIONAL PRECEDENCE, and a deliberate change: "heron OR eagle nanaimo" used
    to mean (heron OR eagle) AND nanaimo, because a flat list of groups was all the
    parser could express and OR simply bound to the term before it. Brackets can now say
    that, so the reading every other search box uses is the one left un-bracketed.
*/
    const SearchTerms t = SearchTerms::parse("heron OR eagle nanaimo");
    QCOMPARE(t.positive.kind, SearchTerms::Node::Any);
    QCOMPARE(t.positive.kids.size(), 2);

    QVERIFY(t.matches("heron at victoria"));        // the OR needs nothing else
    QVERIFY(t.matches("eagle at nanaimo"));
    QVERIFY(!t.matches("eagle at victoria"));       // eagle alone does not satisfy it

    QCOMPARE(t.positiveFts(),
             QString("(\"heron\"* OR (\"eagle\"* AND \"nanaimo\"*))"));

    /* The old meaning, spelled the way it now has to be. */
    QVERIFY(SearchTerms::parse("(heron OR eagle) nanaimo").matches("eagle at nanaimo"));
    QVERIFY(!SearchTerms::parse("(heron OR eagle) nanaimo").matches("heron at victoria"));
}

void tst_searchterms::negationInsideBracketsIsLifted()
{
/*
    NEGATION APPLIES TO THE WHOLE QUERY wherever it is written, because the index has to
    apply it as a separate NOT EXISTS -- FTS5's MATCH has no "everything" token for a NOT
    nested in an OR to subtract from. Lifting it at PARSE time is what keeps the two
    engines agreeing: both compile the same lifted form, so neither can quietly mean
    something the other does not. This asserts both sides of that.
*/
    const SearchTerms t = SearchTerms::parse("tide -(heron OR eagle)");
    QCOMPARE(t.negatives.size(), 1);
    QCOMPARE(t.positiveFts(), QString("\"tide\"*"));
    QCOMPARE(t.negativeFts(), QString("(\"heron\"* OR \"eagle\"*)"));

    QVERIFY(t.matches("low tide with an osprey"));
    QVERIFY(!t.matches("low tide with a heron"));
    QVERIFY(!t.matches("low tide with an eagle"));

    /* Written inside an OR it is lifted just the same, so "a OR -b" is a AND NOT b. The
       rule is one sentence and it is the same sentence for both engines. */
    const SearchTerms inOr = SearchTerms::parse("a OR -b");
    QCOMPARE(inOr.positiveFts(), QString("\"a\"*"));
    QCOMPARE(inOr.negativeFts(), QString("\"b\"*"));
    QVERIFY(!inOr.matches("a and b"));

    /* Two negations cancel, whichever way they are spelled. */
    const SearchTerms twice = SearchTerms::parse("NOT -heron");
    QVERIFY(twice.negatives.isEmpty());
    QCOMPARE(twice.positiveFts(), QString("\"heron\"*"));
}

void tst_searchterms::negationExcludes()
{
    for (const QString &q : {QString("tide -heron"), QString("tide NOT heron")}) {
        const SearchTerms t = SearchTerms::parse(q);
        QCOMPARE(t.negatives.size(), 1);
        QVERIFY2(t.matches("high tide with an eagle"), qPrintable(q));
        QVERIFY2(!t.matches("low tide with a heron"), qPrintable(q));

        QCOMPARE(t.positiveFts(), QString("\"tide\"*"));
        QCOMPARE(t.negativeFts(), QString("\"heron\"*"));
    }

    /* Several negatives are OR-ed for the index, because the caller applies them as one
       NOT EXISTS: an image carrying EITHER must be excluded. */
    const SearchTerms two = SearchTerms::parse("tide -heron -eagle");
    QCOMPARE(two.negativeFts(), QString("\"heron\"* OR \"eagle\"*"));
    QVERIFY(two.matches("low tide with an osprey"));
    QVERIFY(!two.matches("low tide with an eagle"));
}

void tst_searchterms::quotedPhraseStaysTogether()
{
    const SearchTerms t = SearchTerms::parse("\"great blue\"");
    QCOMPARE(t.positive.kind, SearchTerms::Node::Term);
    QCOMPARE(t.positive.text, QString("great blue"));

    QVERIFY(t.matches("a great blue heron"));
    QVERIFY(!t.matches("a great white egret and a blue sky"));

    /* A phrase gets NO prefix star: "great blue*" would prefix-match only the last word,
       which is not what quoting it asked for. */
    QCOMPARE(t.positiveFts(), QString("\"great blue\""));

    /* An operator inside quotes is text, not syntax. */
    const SearchTerms lit = SearchTerms::parse("\"black or white\"");
    QCOMPARE(lit.positive.kind, SearchTerms::Node::Term);
    QVERIFY(lit.matches("a black or white cat"));
}

void tst_searchterms::hyphenInsideAWordIsNotNegation()
{
/*
    Only a LEADING hyphen negates. Lens names are full of hyphens ("100-400mm") and
    treating one as an operator would quietly invert what the user asked for.
*/
    const SearchTerms t = SearchTerms::parse("100-400mm");
    QCOMPARE(t.positive.kind, SearchTerms::Node::Term);
    QVERIFY(t.negatives.isEmpty());
    QVERIFY(t.matches("NIKKOR Z 100-400mm f/4.5"));

    /* Lower-case "or"/"not" are words a caption may contain, not operators. */
    const SearchTerms words = SearchTerms::parse("black or white");
    QCOMPARE(words.positive.kids.size(), 3);
    QVERIFY(words.matches("black or white"));
    QVERIFY(!words.matches("black and white"));
}

void tst_searchterms::emptyAndWhitespaceParseToNothing()
{
/*
    An empty parse matches EVERYTHING, which is correct for "no restriction" but would be
    wrong to present as a search -- so the callers test isEmpty() and treat it as no
    search at all rather than as a query that found the whole folder.
*/
    QVERIFY(SearchTerms::parse("").isEmpty());
    QVERIFY(SearchTerms::parse("   ").isEmpty());
    QVERIFY(SearchTerms::parse("\t\n ").isEmpty());
    /* Operators with no operands are not a query either. */
    QVERIFY(SearchTerms::parse("OR").isEmpty());
    QVERIFY(SearchTerms::parse("NOT").isEmpty());

    QVERIFY(SearchTerms::parse("").matches("anything at all"));
    QVERIFY(SearchTerms::parse("").positiveFts().isEmpty());
}

void tst_searchterms::ftsTermsAreQuoted()
{
/*
    Unquoted, a term containing FTS5 punctuation is a SYNTAX ERROR, and a syntax error in
    MATCH fails the whole query rather than returning nothing -- so a user typing an
    apostrophe mid-word would watch the search break instead of narrow.
*/
    QCOMPARE(SearchTerms::parse("O'Brien").positiveFts(), QString("\"O'Brien\"*"));
    QCOMPARE(SearchTerms::parse("f/4.5").positiveFts(), QString("\"f/4.5\"*"));

    /* A quote never survives into a term -- the tokenizer treats every one of them as a
       phrase delimiter, so an UNBALANCED quote splits the word rather than producing a
       term containing a quote to escape. Pinned because it is the reason the doubling in
       ftsFor is defensive rather than load-bearing: if the tokenizer ever kept a quote,
       this assertion changes and the escaping starts earning its place. */
    const SearchTerms t = SearchTerms::parse("say\"what");
    QCOMPARE(t.positive.kids.size(), 2);
    QCOMPARE(t.positiveFts(), QString("\"say\"* AND \"what\"*"));
}

void tst_searchterms::allNegativeHasNoPositive()
{
/*
    FTS5's MATCH has no "everything" token, so "-heron" cannot be written as one
    expression -- Catalog::search has to apply it as a standalone NOT EXISTS. hasPositive
    is how it knows.
*/
    const SearchTerms t = SearchTerms::parse("-heron");
    QVERIFY(!t.isEmpty());
    QVERIFY(!t.hasPositive());
    QVERIFY(t.positiveFts().isEmpty());
    QCOMPARE(t.negativeFts(), QString("\"heron\"*"));

    QVERIFY(t.matches("an eagle"));
    QVERIFY(!t.matches("a heron"));

    QVERIFY(SearchTerms::parse("tide -heron").hasPositive());
}

QTEST_APPLESS_MAIN(tst_searchterms)
#include "tst_searchterms.moc"
