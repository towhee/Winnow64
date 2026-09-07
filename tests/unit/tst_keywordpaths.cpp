#include <QtTest>

#include "Metadata/keywordpaths.h"

/*
    THE PATH ALGEBRA, which is the whole of keyword identity in one place.

    Winnow is reverting the flat keyword decision: a keyword's identity is its full
    hierarchical path again, not its leaf name. Three functions carry that, and all three
    are shared by the datamodel and the catalog index -- which is the reason they are
    free functions in a header rather than members of either. If the two disagreed about
    how a path splits, the category list and the search would disagree about the same
    picture, which is exactly the failure the flat version was written to end.

    What each case here is really defending:

    LEAF CONSUMPTION is the answer to the Lightroom double. A hierarchical file lists the
    same tag twice, and taking both at face value gave the image a root "Heron" AND a
    "Fauna|Bird|Heron" -- two keywords where the user has one, with the image count split
    between them. That was the defect that forced flattening in the first place, so it is
    the thing that has to be provably fixed before path identity is affordable.

    PREFIX EXPANSION is what keeps ancestor search free. Because every ancestor is linked
    to the image directly, the filter predicate stays QStringList::contains and the
    catalog join stays indexed equality -- no subtree walk, no LIKE, no recursive CTE.

    THE DESCENDANT TEST has two callers that MUST agree: rejecting a drop of a node onto
    its own descendant, and counting the images a rename would rewrite. If they diverge,
    the confirmation dialog reports a number the operation does not go on to touch.
*/

class tst_keywordpaths : public QObject
{
    Q_OBJECT

private slots:
    void aPathSurvivesIntact();
    void aFlatLeafIsConsumedByItsPath();
    void consumptionIsCaseInsensitive();
    void anUnmatchedFlatKeywordBecomesARoot();
    void aFlatOnlyFileIsNotADegradedCase();
    void malformedPathsAreNormalisedNotDuplicated();
    void effectivePathsAreDeterministic();
    void prefixExpansionEmitsEveryAncestor();
    void siblingsShareAncestorsInTheExpansion();
    void sameLeafUnderTwoParentsStaysTwoKeywords();
    void parentOfARootIsEmpty();
    void descendantTestRespectsTheSeparator();
};

void tst_keywordpaths::aPathSurvivesIntact()
{
    const QStringList out = keywordEffectivePaths(QStringList(),
                                                  {"Fauna|Bird|Heron"});
    QCOMPARE(out, QStringList() << "Fauna|Bird|Heron");
}

void tst_keywordpaths::aFlatLeafIsConsumedByItsPath()
{
/*
    The Lightroom shape: dc:subject carries the leaf, lr:hierarchicalSubject the path.
    One keyword, so one path out -- and NOT a stray root "Heron" beside it.
*/
    const QStringList out = keywordEffectivePaths({"Heron"}, {"Fauna|Bird|Heron"});
    QCOMPARE(out.size(), 1);
    QCOMPARE(out.at(0), QString("Fauna|Bird|Heron"));
    QVERIFY2(!out.contains("Heron"), "the leaf must be consumed, not listed again");
}

void tst_keywordpaths::consumptionIsCaseInsensitive()
{
/*
    A library that has been through more than one application carries both spellings.
    Matching case-sensitively would let "heron" escape consumption and reappear as a root,
    which is the double coming back through the side door.
*/
    const QStringList out = keywordEffectivePaths({"heron"}, {"Fauna|Bird|Heron"});
    QCOMPARE(out.size(), 1);
    QCOMPARE(out.at(0), QString("Fauna|Bird|Heron"));
}

void tst_keywordpaths::anUnmatchedFlatKeywordBecomesARoot()
{
/*
    A keyword the hierarchy says nothing about is still a keyword. It becomes a depth-1
    path, which is what the Keywords dock later shows as an unfiled "?" tag.
*/
    const QStringList out = keywordEffectivePaths({"Heron", "Sooke"},
                                                  {"Fauna|Bird|Heron"});
    QCOMPARE(out.size(), 2);
    QCOMPARE(out.at(0), QString("Fauna|Bird|Heron"));   // hierarchy first
    QCOMPARE(out.at(1), QString("Sooke"));
}

void tst_keywordpaths::aFlatOnlyFileIsNotADegradedCase()
{
/*
    Phone images, IPTC-only files and non-Adobe DAMs carry dc:subject and nothing else.
    Under path identity that is a tree of depth one, not a special case -- which is the
    answer to "the hierarchy is not universal", the second argument that produced the
    flat model.
*/
    const QStringList out = keywordEffectivePaths({"Beach", "Sunset"}, QStringList());
    QCOMPARE(out, QStringList() << "Beach" << "Sunset");
}

void tst_keywordpaths::malformedPathsAreNormalisedNotDuplicated()
{
/*
    " Fauna | Bird " and "Fauna||Bird" are the same path written badly. If they reached
    the index unnormalised they would be two rows with one identity between them, and
    ON CONFLICT could not see the collision because the folded strings differ.
*/
    const QStringList out = keywordEffectivePaths(QStringList(),
                                                  {"Fauna|Bird",
                                                   " Fauna | Bird ",
                                                   "Fauna||Bird"});
    QCOMPARE(out, QStringList() << "Fauna|Bird");
}

void tst_keywordpaths::effectivePathsAreDeterministic()
{
/*
    Order is not cosmetic. The row store interns these strings and hands out ids by first
    appearance, so an unstable order would make re-indexing an unchanged image a rewrite
    rather than a no-op.
*/
    const QStringList a = keywordEffectivePaths({"Sooke", "Heron"},
                                                {"Fauna|Bird|Heron", "Location|BC"});
    const QStringList b = keywordEffectivePaths({"Sooke", "Heron"},
                                                {"Fauna|Bird|Heron", "Location|BC"});
    QCOMPARE(a, b);
    QCOMPARE(a, QStringList() << "Fauna|Bird|Heron" << "Location|BC" << "Sooke");
}

void tst_keywordpaths::prefixExpansionEmitsEveryAncestor()
{
    const QStringList out = keywordPrefixExpand({"Fauna|Bird|Heron"});
    QCOMPARE(out, QStringList() << "Fauna" << "Fauna|Bird" << "Fauna|Bird|Heron");
}

void tst_keywordpaths::siblingsShareAncestorsInTheExpansion()
{
/*
    Two paths under one parent expand to five entries, not six: the shared ancestors are
    emitted once. That de-duplication is the reason the row-memory cost of expansion is
    roughly 2x rather than the 3x a naive count suggests, so it is worth pinning.
*/
    const QStringList out = keywordPrefixExpand({"Fauna|Bird|Heron",
                                                 "Fauna|Bird|Eagle"});
    QCOMPARE(out, QStringList() << "Fauna" << "Fauna|Bird" << "Fauna|Bird|Heron"
                                << "Fauna|Bird|Eagle");
    QCOMPARE(out.count("Fauna"), 1);
}

void tst_keywordpaths::sameLeafUnderTwoParentsStaysTwoKeywords()
{
/*
    THE CASE THE WHOLE REVERSAL IS FOR. The user's own Lightroom vocabulary has 59 names
    that appear in more than one place; Bear Lake is both a BC lake and a Colorado one.
    Flat identity merged them into a single filter entry with a summed count and no way
    to tell them apart. Here they are two keywords, and their shared ancestors collapse
    while the leaves do not.
*/
    const QString bc  = "Location|Canada|BC|Northern BC|Bear Lake";
    const QString usa = "Location|USA|Colorado|Rocky Mountain NP|Bear Lake";
    const QStringList out = keywordPrefixExpand(QStringList() << bc << usa);

    QVERIFY(out.contains("Location|Canada|BC|Northern BC|Bear Lake"));
    QVERIFY(out.contains("Location|USA|Colorado|Rocky Mountain NP|Bear Lake"));
    QCOMPARE(out.count("Location"), 1);            // the shared ancestor collapses
    QVERIFY2(!out.contains("Bear Lake"), "a leaf is not a keyword of its own here");
}

void tst_keywordpaths::parentOfARootIsEmpty()
{
    QCOMPARE(keywordParentPath("Fauna|Bird|Heron"), QString("Fauna|Bird"));
    QCOMPARE(keywordParentPath("Fauna"), QString());
}

void tst_keywordpaths::descendantTestRespectsTheSeparator()
{
/*
    A bare startsWith would make "Fauna|Birdsong" a descendant of "Fauna|Bird", which
    would silently reject a legal drag and over-count a rename. The separator is the test.
*/
    QVERIFY(keywordIsDescendant("fauna|bird|heron", "fauna|bird"));
    QVERIFY(keywordIsDescendant("fauna|bird", "fauna|bird"));      // a node is its own
    QVERIFY(!keywordIsDescendant("fauna|birdsong", "fauna|bird"));
    QVERIFY(!keywordIsDescendant("fauna", "fauna|bird"));          // the other direction
    QVERIFY(!keywordIsDescendant("fauna|bird", QString()));
}

QTEST_APPLESS_MAIN(tst_keywordpaths)
#include "tst_keywordpaths.moc"
