#include <QtTest>

#include "Datamodel/filterpredicate.h"
#include "Main/global.h"
#include "Utilities/foldertree.h"

/*
    THE FILTER SEMANTICS, PINNED.

    SortFilter::filterAcceptsRow used to walk the Filters QTreeWidget once per
    ROW. Compiling that tree into a FilterPredicate was a change of COST, not of
    meaning, and it was verified as such: a temporary shadow ran both
    implementations over 1,600 randomised check-state combinations on real
    folders and compared them row by row. That shadow went with the old
    implementation, so what it proved is pinned here instead.

    Three rules, and the third is the one that is easy to get wrong:

      o includes are OR-ed WITHIN a category and AND-ed BETWEEN categories;
      o an exclude is an outright rejection evaluated ACROSS categories -- if
        the row carries an excluded value it is out, whatever else matches;
      o an exclude must NOT make its category count as "filtering". A category
        holding only exclusions is not narrowing the set to those items, it is
        subtracting them. Were it to count, every row lacking any of its items
        would be rejected -- which is the whole set.
*/
class tst_filterpredicate : public QObject
{
    Q_OBJECT

private slots:
    void nothingCheckedAcceptsEverything();
    void includesAreOrWithinACategory();
    void includesAreAndBetweenCategories();
    void excludeRejectsWhateverElseMatches();
    void anAncestorPathMatchesEveryDescendant();
    void aCategoryOfOnlyExcludesDoesNotNarrow();
    void includeAllMatchesWithoutComparing();
    void keywordListsMatchByMembership();
    void aCategoryIsReadOncePerRowNotOncePerItem();
    void foldersMatchTheirSubtreeByPathNotName();
    void matchAllNeedsEveryKeyword();
    void readsColumnOnlyForActiveCategories();

private:
    /*  A row as a column -> value map, standing in for what
        index(row, column).data(Qt::EditRole) would return. */
    using Row = QHash<int, QVariant>;
    static auto fetch(const Row &r)
    {
        return [&r](int column) { return r.value(column); };
    }
    static FilterCategory cat(int column,
                              const QVariantList &inc = {},
                              const QVariantList &exc = {})
    {
        FilterCategory c;
        c.column = column;
        for (const QVariant &v : inc) c.includes.append(v);
        for (const QVariant &v : exc) c.excludes.append(v);
        return c;
    }
};

void tst_filterpredicate::nothingCheckedAcceptsEverything()
{
    FilterPredicate p;
    p.categories << cat(G::RatingColumn) << cat(G::LabelColumn);
    QVERIFY(p.acceptsEverything());

    Row r{ { G::RatingColumn, "3" }, { G::LabelColumn, "Red" } };
    QVERIFY(p.accepts(fetch(r)));

    /*  A category with only excludes is still "filtering" -- it can reject --
        even though it does not narrow. */
    p.categories << cat(G::PickColumn, {}, { "false" });
    QVERIFY(!p.acceptsEverything());
}

void tst_filterpredicate::includesAreOrWithinACategory()
{
    FilterPredicate p;
    p.categories << cat(G::RatingColumn, { "3", "4" });

    Row three{ { G::RatingColumn, "3" } };
    Row four { { G::RatingColumn, "4" } };
    Row two  { { G::RatingColumn, "2" } };
    QVERIFY(p.accepts(fetch(three)));
    QVERIFY(p.accepts(fetch(four)));
    QVERIFY(!p.accepts(fetch(two)));
}

void tst_filterpredicate::includesAreAndBetweenCategories()
{
    /*  Check two ratings to see both, then check a camera model to see only
        those two ratings FROM that camera. */
    FilterPredicate p;
    p.categories << cat(G::RatingColumn, { "3", "4" })
                 << cat(G::CameraModelColumn, { "NIKON Z 9" });

    Row hit  { { G::RatingColumn, "4" }, { G::CameraModelColumn, "NIKON Z 9" } };
    Row wrongCamera { { G::RatingColumn, "4" }, { G::CameraModelColumn, "NIKON Z 8" } };
    Row wrongRating { { G::RatingColumn, "2" }, { G::CameraModelColumn, "NIKON Z 9" } };
    QVERIFY(p.accepts(fetch(hit)));
    QVERIFY(!p.accepts(fetch(wrongCamera)));
    QVERIFY(!p.accepts(fetch(wrongRating)));
}

void tst_filterpredicate::excludeRejectsWhateverElseMatches()
{
    /*  "include everything under Location, except the American branch". Exclusion used to
        be how an AMBIGUOUS name was separated -- include Vancouver, exclude USA -- and
        path identity has made that particular need go away, because the two Vancouvers
        are now two different values. The mechanism stays because excluding a BRANCH is a
        question a tree raises rather than answers, and the rule being pinned is unchanged:
        the exclusion is in a DIFFERENT category from the include and must still win. */
    FilterPredicate p;
    p.categories << cat(G::KeywordsAllColumn, { "Location" })
                 << cat(G::KeywordsAllColumn, {}, { "Location|USA" });

    Row bc { { G::KeywordsAllColumn, QStringList{
        "Location", "Location|Canada", "Location|Canada|BC|Vancouver" } } };
    Row wa { { G::KeywordsAllColumn, QStringList{
        "Location", "Location|USA", "Location|USA|WA|Vancouver" } } };
    QVERIFY(p.accepts(fetch(bc)));
    QVERIFY(!p.accepts(fetch(wa)));
}

void tst_filterpredicate::anAncestorPathMatchesEveryDescendant()
{
    /*  WHY THE PREDICATE DID NOT HAVE TO CHANGE when keyword identity went back to the
        full path. The row's keyword column holds every ANCESTOR PREFIX of every path the
        image carries, so filtering on a parent is the same whole-element membership test
        as filtering on a leaf -- includeHit stays QStringList::contains, with no
        separator awareness, no prefix scan and no subtree walk in the loop that runs on
        every row on every filter change.

        The two same-named leaves are the payoff: under flat identity both rows carried
        the bare name "Vancouver" and no filter could tell them apart. */
    Row bc { { G::KeywordsAllColumn, QStringList{
        "Location", "Location|Canada", "Location|Canada|BC",
        "Location|Canada|BC|Vancouver" } } };
    Row wa { { G::KeywordsAllColumn, QStringList{
        "Location", "Location|USA", "Location|USA|WA",
        "Location|USA|WA|Vancouver" } } };

    FilterPredicate ancestor;
    ancestor.categories << cat(G::KeywordsAllColumn, { "Location" });
    QVERIFY(ancestor.accepts(fetch(bc)));
    QVERIFY(ancestor.accepts(fetch(wa)));

    FilterPredicate oneBranch;
    oneBranch.categories << cat(G::KeywordsAllColumn, { "Location|Canada" });
    QVERIFY(oneBranch.accepts(fetch(bc)));
    QVERIFY(!oneBranch.accepts(fetch(wa)));

    FilterPredicate oneLeaf;
    oneLeaf.categories << cat(G::KeywordsAllColumn, { "Location|USA|WA|Vancouver" });
    QVERIFY(!oneLeaf.accepts(fetch(bc)));
    QVERIFY(oneLeaf.accepts(fetch(wa)));

    /*  A BARE LEAF IS NOT A KEYWORD. It matches nothing, and it must not match by
        accident -- a prefix or substring test here would make "Vancouver" hit both rows
        again and quietly undo the whole reversal. */
    FilterPredicate bareLeaf;
    bareLeaf.categories << cat(G::KeywordsAllColumn, { "Vancouver" });
    QVERIFY(!bareLeaf.accepts(fetch(bc)));
    QVERIFY(!bareLeaf.accepts(fetch(wa)));
}

void tst_filterpredicate::aCategoryOfOnlyExcludesDoesNotNarrow()
{
    /*  THE RULE THAT IS EASY TO GET WRONG. If an exclusion made its category
        count as filtering, a row carrying none of that category's values would
        fail the "no match in category" test -- and that is nearly every row.
        Re-injecting exactly this made 1,900 of 20,000 shadow comparisons
        disagree. */
    FilterPredicate p;
    p.categories << cat(G::LabelColumn, {}, { "Red" });

    Row red  { { G::LabelColumn, "Red" } };
    Row blue { { G::LabelColumn, "Blue" } };
    Row none { { G::LabelColumn, "" } };
    QVERIFY(!p.accepts(fetch(red)));
    QVERIFY(p.accepts(fetch(blue)));
    QVERIFY2(p.accepts(fetch(none)),
             "a row with no value in an exclude-only category must still pass");
}

void tst_filterpredicate::includeAllMatchesWithoutComparing()
{
    /*  The search category with a search armed but nothing typed: it matched
        every row, and it did so without reading the column. */
    FilterPredicate p;
    FilterCategory c = cat(G::SearchColumn);
    c.includeAll = true;
    p.categories << c;

    Row anything{};
    QVERIFY(p.accepts(fetch(anything)));
    /*  And it is NOT filtering: it can reject nothing, so the proxy's no-filter fast
        path applies and the column is never read. This was asserted the other way,
        which made every filter pass read every row's Search cell (see isFiltering). */
    QVERIFY(p.acceptsEverything());
    int reads = 0;
    QVERIFY(p.accepts([&](int) { ++reads; return QVariant(); }));
    QCOMPARE(reads, 0);

    /*  An exclude on the same category still rejects -- includeAll only makes the
        INCLUDES moot. */
    p.categories[0].excludes.append("true");
    QVERIFY(!p.acceptsEverything());
    Row excluded { { G::SearchColumn, "true" } };
    QVERIFY(!p.accepts(fetch(excluded)));
}

void tst_filterpredicate::keywordListsMatchByMembership()
{
    /*  A keyword column holds a QStringList, so a filter value matches when the
        list CONTAINS it rather than when the whole list equals it. */
    FilterPredicate p;
    p.categories << cat(G::KeywordsAllColumn, { "Heron" });

    Row heron { { G::KeywordsAllColumn, QStringList{ "Wildlife", "Birds", "Heron" } } };
    Row eagle { { G::KeywordsAllColumn, QStringList{ "Wildlife", "Birds", "Eagle" } } };
    Row empty { { G::KeywordsAllColumn, QStringList{} } };
    QVERIFY(p.accepts(fetch(heron)));
    QVERIFY(!p.accepts(fetch(eagle)));
    QVERIFY(!p.accepts(fetch(empty)));
}

void tst_filterpredicate::aCategoryIsReadOncePerRowNotOncePerItem()
{
    /*  The point of compiling. The tree walk fetched the cell once per ITEM, so
        a category holding 200 keywords read the same cell 200 times; that is
        most of the O(rows x items) this replaced. */
    FilterPredicate p;
    QVariantList many;
    for (int i = 0; i < 200; ++i) many << QString("kw%1").arg(i);
    p.categories << cat(G::KeywordsAllColumn, many);

    int reads = 0;
    Row r { { G::KeywordsAllColumn, QStringList{ "kw199" } } };
    const bool ok = p.accepts([&](int column) { ++reads; return r.value(column); });
    QVERIFY(ok);
    QCOMPARE(reads, 1);
}

void tst_filterpredicate::foldersMatchTheirSubtreeByPathNotName()
{
/*
    The Folders category filters on G::FolderPathsAllColumn -- a row's folder and every
    folder above it (FolderTree::ancestry) -- with the folder's PATH as the item's value.
    So a checked parent takes its whole subtree with the ordinary membership test, a
    neighbour that shares a prefix is not taken, two folders called "DxO" stay two, and
    "this folder only" is include-the-folder, exclude-its-children.
*/
    auto rowIn = [](const QString &folder) {
        return Row{ { G::FolderPathsAllColumn, FolderTree::ancestry(folder) } };
    };
    const Row top     = rowIn("/P/2024");
    const Row inner   = rowIn("/P/2024/DxO");
    const Row beside  = rowIn("/P/2024 raw");
    const Row dxo2025 = rowIn("/P/2025/DxO");

    FilterPredicate branch;
    branch.categories << cat(G::FolderPathsAllColumn, { "/P/2024" });
    QVERIFY(branch.accepts(fetch(top)));
    QVERIFY(branch.accepts(fetch(inner)));
    QVERIFY(!branch.accepts(fetch(beside)));
    QVERIFY(!branch.accepts(fetch(dxo2025)));

    FilterPredicate oneDxo;
    oneDxo.categories << cat(G::FolderPathsAllColumn, { "/P/2024/DxO" });
    QVERIFY(oneDxo.accepts(fetch(inner)));
    QVERIFY(!oneDxo.accepts(fetch(dxo2025)));

    FilterPredicate only;
    only.categories << cat(G::FolderPathsAllColumn, { "/P/2024" }, { "/P/2024/DxO" });
    QVERIFY(only.accepts(fetch(top)));
    QVERIFY(!only.accepts(fetch(inner)));
}

void tst_filterpredicate::matchAllNeedsEveryKeyword()
{
/*
    The Keywords header's "all": Family AND Beach. Ancestors still match through the
    prefix-expanded list, and an exclusion rejects in either mode.
*/
    const Row both    { { G::KeywordsAllColumn, QStringList{"Family", "Beach"} } };
    const Row family  { { G::KeywordsAllColumn, QStringList{"Family"} } };
    const Row nested  { { G::KeywordsAllColumn,
                          QStringList{"Location", "Location|Beach", "Family"} } };

    FilterPredicate any;
    any.categories << cat(G::KeywordsAllColumn, { "Family", "Beach" });
    QVERIFY(any.accepts(fetch(both)));
    QVERIFY(any.accepts(fetch(family)));

    FilterPredicate all = any;
    all.categories[0].matchAll = true;
    QVERIFY(all.accepts(fetch(both)));
    QVERIFY(!all.accepts(fetch(family)));

    FilterPredicate ancestor;
    ancestor.categories << cat(G::KeywordsAllColumn, { "Location", "Family" });
    ancestor.categories[0].matchAll = true;
    QVERIFY(ancestor.accepts(fetch(nested)));

    FilterPredicate withExclude;
    withExclude.categories << cat(G::KeywordsAllColumn, { "Family", "Beach" }, { "Beach" });
    withExclude.categories[0].matchAll = true;
    QVERIFY(!withExclude.accepts(fetch(both)));
}

void tst_filterpredicate::readsColumnOnlyForActiveCategories()
{
/*
    MW::editNeedsRefilter skips the whole-proxy refilter after a rating or label edit
    when this says no active category reads the column. A false "no" would leave a
    filtered view showing an image the edit has just filtered out, so: an idle category
    does not read its column, an include or an exclude makes it read, and includeAll
    alone does not -- it accepts every row, so no edit can change what it admits.
*/
    FilterPredicate p;
    p.categories << cat(G::RatingColumn) << cat(G::LabelColumn);
    QVERIFY(!p.readsColumn(G::RatingColumn));               // present but idle
    QVERIFY(!p.readsColumn(G::LabelColumn));
    QVERIFY(!p.readsColumn(G::PickColumn));                 // not present at all

    p.categories[0] = cat(G::RatingColumn, { "3" });         // include
    QVERIFY(p.readsColumn(G::RatingColumn));
    QVERIFY(!p.readsColumn(G::LabelColumn));

    p.categories[1] = cat(G::LabelColumn, {}, { "Red" });    // exclude only
    QVERIFY(p.readsColumn(G::LabelColumn));

    /*  An armed-but-empty search accepts every row, so it reads nothing -- until it
        also carries an exclude. */
    FilterCategory any = cat(G::SearchColumn);
    any.includeAll = true;
    p.categories << any;
    QVERIFY(!p.readsColumn(G::SearchColumn));
    p.categories.last().excludes.append("true");
    QVERIFY(p.readsColumn(G::SearchColumn));
}

QTEST_MAIN(tst_filterpredicate)
#include "tst_filterpredicate.moc"
