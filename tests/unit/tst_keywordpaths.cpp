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
    void pruningDropsOnlyProperAncestors();
    void pruningIsWhatMakesATidyMergeRatherThanDouble();
    void aDropOnAChildlessKeywordIsAMergeWhateverItIsCalled();
    void severalSpellingsCanBeMergedIntoOneKeyword();
    void aDropOnItsOwnLeafIsAMergeNotANesting();
    void aDropOnABranchWithThatChildMergesRatherThanDuplicates();
    void aDropOnABranchWithoutThatChildNamesTheNewPath();
    void dropResolutionFoldsCase();
    void aKeywordAlreadyAtItsTargetResolvesToNothing();
    void onlyTheLeafOfTheCheckedKeywordMoves();
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

void tst_keywordpaths::pruningDropsOnlyProperAncestors()
{
/*
    An ancestor of another path in the same list goes; a path that merely shares a PREFIX
    STRING does not. "Fauna|Bird" must survive beside "Fauna|Birdsong|Dawn", which is the
    same separator trap keywordIsDescendant guards, in the other direction.
*/
    QCOMPARE(keywordPruneAncestors({"Location", "Location|Canada", "Location|Canada|BC"}),
             QStringList() << "Location|Canada|BC");

    QCOMPARE(keywordPruneAncestors({"Fauna|Bird", "Fauna|Birdsong|Dawn"}),
             QStringList() << "Fauna|Bird" << "Fauna|Birdsong|Dawn");

    /* Unrelated paths and a lone path are returned as they came, in order. */
    QCOMPARE(keywordPruneAncestors({"Fauna|Bird|Heron", "Location|Canada"}),
             QStringList() << "Fauna|Bird|Heron" << "Location|Canada");
}

void tst_keywordpaths::pruningIsWhatMakesATidyMergeRatherThanDouble()
{
/*
    THE CASE THE TIDY CREATES. An image already filed as Location|Canada|BC also carries a
    flat "Canada" from an older application. Moving that flat keyword to Location|Canada
    -- what MW::applyKeywordTidyPlan does -- would leave the image claiming both, which is
    one fact written twice: prefix expansion already answers a search for Location|Canada
    from the deeper path alone. Pruning is what turns the move into a merge.

    And the write survives a read-back unchanged, which is the property that stops a tidy
    from drifting: composing these paths into dc:subject and lr:hierarchicalSubject and
    reading them back must give the pruned list, not the one that went in.
*/
    const QStringList merged = keywordPruneAncestors({"Location|Canada",
                                                      "Location|Canada|BC"});
    QCOMPARE(merged, QStringList() << "Location|Canada|BC");

    QCOMPARE(keywordEffectivePaths({"BC"}, merged), merged);
}

/*
    THE DROP RULE. Three outcomes, and the point of pinning them is that only ONE of them
    may create a vocabulary node -- the keyword list is the user's, and a drag that
    quietly grew it a level deeper than they meant would be the one thing this feature
    must never do.
*/
void tst_keywordpaths::aDropOnAChildlessKeywordIsAMergeWhateverItIsCalled()
{
/*
    THE CASE THAT DEFINES THE RULE. "Animals" dropped on Fauna|Bird|Ferruginous Hawk says
    "these two pictures ARE that bird" -- the node's own name has nothing to do with it.
    Matching on the name instead produced Fauna|Bird|Ferruginous Hawk|Animals, which is
    not a keyword anybody wants and quietly grew the vocabulary a level.

    A KEYWORD IS A NODE WITH NO CHILDREN; a branch is a node with children. That is the
    whole of the distinction, and it is the one the user makes when choosing where to
    drop.
*/
    QCOMPARE(keywordDropTarget("Animals", "Fauna|Bird|Ferruginous Hawk", QStringList()),
             QString("Fauna|Bird|Ferruginous Hawk"));
}

void tst_keywordpaths::severalSpellingsCanBeMergedIntoOneKeyword()
{
/*
    What the childless rule is FOR, beyond the case that found it: a library holds the
    same bird spelled three ways, and checking all three and dropping them on the one
    real keyword is the shortest path from that to a tidy vocabulary. Each resolves to
    the same node, which the caller merges into rather than creating twice.
*/
    const QString hawk = "Fauna|Bird|Ferruginous Hawk";
    QCOMPARE(keywordDropTarget("Ferruginous hawk", hawk, QStringList()), hawk);
    QCOMPARE(keywordDropTarget("Ferr. Hawk", hawk, QStringList()), hawk);
    QCOMPARE(keywordDropTarget("Buteo regalis", hawk, QStringList()), hawk);
}

void tst_keywordpaths::aDropOnItsOwnLeafIsAMergeNotANesting()
{
/*
    "Squirrel" dropped ON Fauna|Animal|Squirrel is the commonest gesture there is: the
    user found the stray's real home and dropped it there. Appending the leaf again would
    give them Fauna|Animal|Squirrel|Squirrel, which is the self-nesting artifact the tidy
    dialog already has to rank last.
*/
    QCOMPARE(keywordDropTarget("Squirrel", "Fauna|Animal|Squirrel", {"Chipmunk"}),
             QString("Fauna|Animal|Squirrel"));
}

void tst_keywordpaths::aDropOnABranchWithThatChildMergesRatherThanDuplicates()
{
/*
    The branch already has the keyword as a child. Nothing is created; the images simply
    move into the node that is already there.
*/
    QCOMPARE(keywordDropTarget("Squirrel", "Fauna|Animal",
                               {"Bear", "Squirrel", "Vole"}),
             QString("Fauna|Animal|Squirrel"));
}

void tst_keywordpaths::aDropOnABranchWithoutThatChildNamesTheNewPath()
{
/*
    The one case that creates, and the counterpart of the childless rule above: the SAME
    keyword dropped on a node with children is filed UNDER it rather than merged INTO it.
    Several checked keywords dropped on one branch each get their own child -- they are
    never all collapsed onto the branch itself.
*/
    QCOMPARE(keywordDropTarget("Bunny", "Fauna|Animal", {"Bear"}),
             QString("Fauna|Animal|Bunny"));
    QCOMPARE(keywordDropTarget("Vole", "Fauna|Animal", {"Bear"}),
             QString("Fauna|Animal|Vole"));
}

void tst_keywordpaths::dropResolutionFoldsCase()
{
/*
    Case decides all three outcomes, and it must decide them the way the rest of keyword
    identity does -- keywordFold, not toLower. A vocabulary spelling "Squirrel" and a
    stray spelling "squirrel" are the same keyword, so this is a merge; creating
    Fauna|Animal|squirrel beside it would be the duplicate the whole exercise is about.
*/
    QCOMPARE(keywordDropTarget("squirrel", "Fauna|Animal", {"Squirrel"}),
             QString("Fauna|Animal|Squirrel"));
    QCOMPARE(keywordDropTarget("SQUIRREL", "Fauna|Animal|Squirrel", QStringList()),
             QString("Fauna|Animal|Squirrel"));
}

void tst_keywordpaths::aKeywordAlreadyAtItsTargetResolvesToNothing()
{
/*
    Dropping a keyword onto the branch it already sits in. Empty rather than the path it
    already has, so the caller reports "already filed" instead of rewriting every image
    in it to the value it is holding.
*/
    QCOMPARE(keywordDropTarget("Fauna|Animal|Vole", "Fauna|Animal", {"Vole"}),
             QString());
}

void tst_keywordpaths::onlyTheLeafOfTheCheckedKeywordMoves()
{
/*
    Filing "Trip|Kenya" under Location gives Location|Kenya, not Location|Trip|Kenya.
    "Trip" is the spelling being abandoned; carrying it along would rebuild the branch the
    move exists to leave behind.
*/
    QCOMPARE(keywordDropTarget("Trip|Kenya", "Location", {"Canada"}),
             QString("Location|Kenya"));
}

QTEST_APPLESS_MAIN(tst_keywordpaths)
#include "tst_keywordpaths.moc"
