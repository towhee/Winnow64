#include <QtTest>

#include "Datamodel/filters.h"
#include "Main/global.h"

/*
    THE NESTED KEYWORD CATEGORY, and the trap it set.

    Every Filters category was one level deep until keyword identity went back to the full
    path, and that assumption was written into a dozen loops as
    "for i < category->childCount()" and into Filters::save as
    "{parent->text(0), item->text(0)}". Both are wrong for a tree, and both are wrong
    QUIETLY: a childCount loop sees the roots and misses the leaves, and a save keyed on
    the parent looks for "Heron" under "Bird" and finds nothing, so restore() simply
    restores less than it saved and reports nothing.

    A dropped filter is not a crash and not a wrong picture -- it is the user's filter
    silently coming back empty after a folder change, which is exactly the class of defect
    that gets blamed on something else for a week. Hence this file.
*/
class tst_filters : public QObject
{
    Q_OBJECT

private slots:
    void keywordsBuildAsATree();
    void savedNestedStateSurvivesARebuild();
    void twoKeywordsSharingALeafRestoreIndependently();
    void everyDepthReachesTheQuery();

private:
    /*  The vocabulary the catalog hands over: PATHS, prefix-expanded, so every ancestor
        already carries its own count. Two branches that share a root, and two keywords
        that share a leaf. */
    static QMap<QString, int> vocabulary()
    {
        return {
            {"Location", 9},
            {"Location|Canada", 5},
            {"Location|Canada|BC", 5},
            {"Location|Canada|BC|Vancouver", 3},
            {"Location|USA", 4},
            {"Location|USA|WA", 4},
            {"Location|USA|WA|Vancouver", 2},
            {"Fauna", 7},
            {"Fauna|Bird", 7},
        };
    }

    static QTreeWidgetItem *findByValue(QTreeWidgetItem *category, const QString &value)
    {
        for (int i = 0; i < category->childCount(); ++i) {
            QTreeWidgetItem *child = category->child(i);
            if (child->data(1, Qt::EditRole).toString() == value) return child;
            if (QTreeWidgetItem *deeper = findByValue(child, value)) return deeper;
        }
        return nullptr;
    }
};

void tst_filters::keywordsBuildAsATree()
{
    Filters f(nullptr);
    f.addCategoryItems(vocabulary(), f.keywords);

    /*  Two roots, not nine flat rows. */
    QCOMPARE(f.keywords->childCount(), 2);

    QTreeWidgetItem *location = findByValue(f.keywords, "Location");
    QVERIFY(location);
    QCOMPARE(location->parent(), f.keywords);
    QCOMPARE(location->text(0), QString("Location"));       // labelled by LEAF

    QTreeWidgetItem *bc = findByValue(f.keywords, "Location|Canada|BC");
    QVERIFY(bc);
    QCOMPARE(bc->text(0), QString("BC"));
    QCOMPARE(bc->parent()->data(1, Qt::EditRole).toString(), QString("Location|Canada"));

    /*  COUNTS NEED NO SUMMING. Every image is linked to every ancestor prefix, so the
        count BuildFilters produced for a branch is already its subtree total and each
        node reads its own straight out of the map. If this ever had to add up its
        children, the expansion would have stopped working. */
    QCOMPARE(location->data(2, Qt::EditRole).toInt(), 9);
    QCOMPARE(bc->data(2, Qt::EditRole).toInt(), 5);
}

void tst_filters::savedNestedStateSurvivesARebuild()
{
/*
    THE REGRESSION THIS FILE EXISTS FOR. Check a keyword four levels down, save, rebuild
    the category from scratch, restore. Keyed on the item's PARENT this restored nothing,
    because the parent of "Vancouver" is "BC" and the saved state looked under "Keywords".
*/
    Filters f(nullptr);
    f.addCategoryItems(vocabulary(), f.keywords);

    QTreeWidgetItem *van = findByValue(f.keywords, "Location|Canada|BC|Vancouver");
    QVERIFY(van);
    van->setCheckState(0, Qt::Checked);

    f.save();

    /*  A rebuild is what a folder change does: the items are destroyed and made again. */
    f.keywords->takeChildren();
    QCOMPARE(f.keywords->childCount(), 0);
    f.addCategoryItems(vocabulary(), f.keywords);

    f.restore();

    QTreeWidgetItem *again = findByValue(f.keywords, "Location|Canada|BC|Vancouver");
    QVERIFY(again);
    QCOMPARE(again->checkState(0), Qt::Checked);
}

void tst_filters::twoKeywordsSharingALeafRestoreIndependently()
{
/*
    The label is only the leaf, so a state keyed on the label could not tell these apart
    and would restore whichever it met first -- silently filtering on the wrong place.
    This is the case flat identity could not express at all.
*/
    Filters f(nullptr);
    f.addCategoryItems(vocabulary(), f.keywords);

    QTreeWidgetItem *usa = findByValue(f.keywords, "Location|USA|WA|Vancouver");
    QTreeWidgetItem *bc  = findByValue(f.keywords, "Location|Canada|BC|Vancouver");
    QVERIFY(usa && bc);
    QCOMPARE(usa->text(0), QString("Vancouver"));
    QCOMPARE(bc->text(0), QString("Vancouver"));    // the same label, deliberately

    /*  DIFFERENT STATES ON THE SAME LABEL, which is what makes this case impossible to
        satisfy with a label-keyed save however the lookup happens to be ordered: one key
        cannot carry both an include and an exclude. Checking only one of them would let a
        first-match-wins or last-match-wins lookup pass by luck. */
    bc->setCheckState(0, Qt::Checked);
    // an EXCLUSION keeps its own state too
    usa->setCheckState(0, Qt::PartiallyChecked);

    f.save();
    f.keywords->takeChildren();
    f.addCategoryItems(vocabulary(), f.keywords);
    f.restore();

    QTreeWidgetItem *usaAgain = findByValue(f.keywords, "Location|USA|WA|Vancouver");
    QTreeWidgetItem *bcAgain  = findByValue(f.keywords, "Location|Canada|BC|Vancouver");
    QVERIFY(usaAgain && bcAgain);
    QCOMPARE(bcAgain->checkState(0), Qt::Checked);
    QCOMPARE(usaAgain->checkState(0), Qt::PartiallyChecked);
}

void tst_filters::everyDepthReachesTheQuery()
{
/*
    fillQuery walked children rather than descendants, so a nested keyword never reached
    the catalog query -- the user checked a leaf and the query filtered on nothing. It
    must also bind the PATH rather than the label.
*/
    Filters f(nullptr);
    f.addCategoryItems(vocabulary(), f.keywords);

    findByValue(f.keywords, "Fauna")->setCheckState(0, Qt::Checked);        // depth 1
    findByValue(f.keywords, "Location|Canada|BC|Vancouver")
        ->setCheckState(0, Qt::Checked);
    findByValue(f.keywords, "Location|USA")->setCheckState(0, Qt::PartiallyChecked);

    CatalogQuery q;
    f.fillQuery(q);

    QCOMPARE(q.keywords.size(), 2);
    QVERIFY(q.keywords.contains("Fauna"));
    QVERIFY(q.keywords.contains("Location|Canada|BC|Vancouver"));
    QVERIFY2(!q.keywords.contains("Vancouver"), "the LABEL must never reach the query");
    QCOMPARE(q.excludeKeywords, QStringList{"Location|USA"});
}

QTEST_MAIN(tst_filters)
#include "tst_filters.moc"
