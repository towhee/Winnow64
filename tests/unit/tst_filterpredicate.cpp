#include <QtTest>

#include "Datamodel/filterpredicate.h"
#include "Main/global.h"

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
    void aCategoryOfOnlyExcludesDoesNotNarrow();
    void includeAllMatchesWithoutComparing();
    void keywordListsMatchByMembership();
    void aCategoryIsReadOncePerRowNotOncePerItem();

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
    /*  "include Vancouver, exclude USA" -- the keyword vocabulary is flat, so
        an ambiguous name is separated by ruling one of its parents out rather
        than by descending a tree. The exclusion is in a DIFFERENT category from
        the include and must still win. */
    FilterPredicate p;
    p.categories << cat(G::KeywordsAllColumn, { "Vancouver" })
                 << cat(G::KeywordsAllColumn, {}, { "USA" });

    Row bc { { G::KeywordsAllColumn, QStringList{ "Vancouver", "Canada" } } };
    Row wa { { G::KeywordsAllColumn, QStringList{ "Vancouver", "USA" } } };
    QVERIFY(p.accepts(fetch(bc)));
    QVERIFY(!p.accepts(fetch(wa)));
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
    QVERIFY(!p.acceptsEverything());        // it IS filtering, it just matches all
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

QTEST_MAIN(tst_filterpredicate)
#include "tst_filterpredicate.moc"
