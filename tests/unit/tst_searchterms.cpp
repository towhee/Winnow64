#include <QtTest>

#include "Utilities/searchterms.h"

/*
    The shared search grammar (Utilities/searchterms.h).

    WHAT IS ACTUALLY BEING PINNED is that ONE parse drives two very different engines.
    Winnow has two search boxes -- F2 over the datamodel and Shift+F2 over the index --
    deliberately paired as "here" and "everywhere", which promises the same words mean the
    same thing. They did not: to the datamodel's QString::contains, "heron OR eagle" was a
    single literal string containing the word OR.

    So most of these assert the SAME query both ways: matches() is what the datamodel
    does, positiveFts()/negativeFts() are what the catalog does, and they must agree about
    which pictures are wanted.
*/
class tst_searchterms : public QObject
{
    Q_OBJECT

private slots:
    void bareWordsAreAnded();
    void orGroupsAlternatives();
    void negationExcludes();
    void quotedPhraseStaysTogether();
    void hyphenInsideAWordIsNotNegation();
    void emptyAndWhitespaceParseToNothing();
    void ftsTermsAreQuoted();
    void allNegativeHasNoPositive();
};

void tst_searchterms::bareWordsAreAnded()
{
    const SearchTerms t = SearchTerms::parse("heron nanaimo");
    QCOMPARE(t.groups.size(), 2);

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
    QCOMPARE(t.groups.size(), 1);
    QCOMPARE(t.groups.first().alternatives.size(), 2);

    QVERIFY(t.matches("a heron"));
    QVERIFY(t.matches("an eagle"));
    QVERIFY(!t.matches("an osprey"));

    QCOMPARE(t.positiveFts(), QString("(\"heron\"* OR \"eagle\"*)"));

    /* OR chains into ONE group rather than making a new one per operator. */
    const SearchTerms three = SearchTerms::parse("heron OR eagle OR osprey");
    QCOMPARE(three.groups.size(), 1);
    QCOMPARE(three.groups.first().alternatives.size(), 3);
    QVERIFY(three.matches("an osprey"));

    /* Mixed: (a OR b) AND c. */
    const SearchTerms mixed = SearchTerms::parse("heron OR eagle nanaimo");
    QCOMPARE(mixed.groups.size(), 2);
    QVERIFY(mixed.matches("eagle at nanaimo"));
    QVERIFY(!mixed.matches("eagle at victoria"));
}

void tst_searchterms::negationExcludes()
{
    for (const QString &q : {QString("tide -heron"), QString("tide NOT heron")}) {
        const SearchTerms t = SearchTerms::parse(q);
        QCOMPARE(t.groups.size(), 2);
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
    QCOMPARE(t.groups.size(), 1);
    QCOMPARE(t.groups.first().alternatives.first(), QString("great blue"));

    QVERIFY(t.matches("a great blue heron"));
    QVERIFY(!t.matches("a great white egret and a blue sky"));

    /* A phrase gets NO prefix star: "great blue*" would prefix-match only the last word,
       which is not what quoting it asked for. */
    QCOMPARE(t.positiveFts(), QString("\"great blue\""));

    /* An operator inside quotes is text, not syntax. */
    const SearchTerms lit = SearchTerms::parse("\"black or white\"");
    QCOMPARE(lit.groups.size(), 1);
    QVERIFY(lit.matches("a black or white cat"));
}

void tst_searchterms::hyphenInsideAWordIsNotNegation()
{
/*
    Only a LEADING hyphen negates. Lens names are full of hyphens ("100-400mm") and
    treating one as an operator would quietly invert what the user asked for.
*/
    const SearchTerms t = SearchTerms::parse("100-400mm");
    QCOMPARE(t.groups.size(), 1);
    QVERIFY(!t.groups.first().negated);
    QVERIFY(t.matches("NIKKOR Z 100-400mm f/4.5"));

    /* Lower-case "or"/"not" are words a caption may contain, not operators. */
    const SearchTerms words = SearchTerms::parse("black or white");
    QCOMPARE(words.groups.size(), 3);
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
    QCOMPARE(SearchTerms::parse("(((").positiveFts(), QString("\"(((\"*"));

    /* A quote never survives into a term -- the tokenizer treats every one of them as a
       phrase delimiter, so an UNBALANCED quote splits the word rather than producing a
       term containing a quote to escape. Pinned because it is the reason the doubling in
       ftsFor is defensive rather than load-bearing: if the tokenizer ever kept a quote,
       this assertion changes and the escaping starts earning its place. */
    const SearchTerms t = SearchTerms::parse("say\"what");
    QCOMPARE(t.groups.size(), 2);
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
