#include <QtTest>
#include <QTemporaryDir>
#include <QSettings>

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
    void persistedStateSurvivesSettingsAndARebuild();
    void twoKeywordsSharingALeafRestoreIndependently();
    void everyDepthReachesTheQuery();
    void foldersBuildAsATreeUnderTheirAnchors();
    void folderCountsRollUpAndStateSurvivesARebuild();
    void foldersReachTheQueryByPath();
    void aNestedCategoryIsCountedAtEveryDepth();
    void keywordMatchModeTravelsAndResets();

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

void tst_filters::persistedStateSurvivesSettingsAndARebuild()
{
/*
    MW::saveLibraryState / queueLibraryStateRestore: the checked state goes into QSettings
    as persistableState()'s map and comes back through setStateToRestore() + restore()
    after the categories have been rebuilt from scratch at the next start. Include AND
    exclude must both survive -- an exclusion restored as an inclusion inverts the filter
    silently -- and so must a nested keyword keyed on its path, not its leaf.
*/
    Filters f(nullptr);
    f.addCategoryItems(vocabulary(), f.keywords);
    QTreeWidgetItem *bc  = findByValue(f.keywords, "Location|Canada|BC|Vancouver");
    QTreeWidgetItem *usa = findByValue(f.keywords, "Location|USA|WA|Vancouver");
    QVERIFY(bc && usa);
    bc->setCheckState(0, Qt::Checked);
    usa->setCheckState(0, Qt::PartiallyChecked);

    /*  Through a real QSettings file, because the map crosses one in the app. */
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    {
        QSettings st(dir.filePath("s.ini"), QSettings::IniFormat);
        st.setValue("LibraryState/filters", f.persistableState());
    }
    QVariantMap back;
    {
        QSettings st(dir.filePath("s.ini"), QSettings::IniFormat);
        back = st.value("LibraryState/filters").toMap();
    }

    Filters g(nullptr);                                     // the next start
    g.addCategoryItems(vocabulary(), g.keywords);
    g.setStateToRestore(back);
    g.restore();

    QTreeWidgetItem *bc2  = findByValue(g.keywords, "Location|Canada|BC|Vancouver");
    QTreeWidgetItem *usa2 = findByValue(g.keywords, "Location|USA|WA|Vancouver");
    QVERIFY(bc2 && usa2);
    QCOMPARE(bc2->checkState(0), Qt::Checked);
    QCOMPARE(usa2->checkState(0), Qt::PartiallyChecked);

    /*  And nothing else changed: the restored tree's checked set is EXACTLY the
        original's -- which includes the Search category's own "true" item, checked by
        default in any fresh Filters, so the count is not simply the two set above. */
    auto checkedIn = [](Filters &t) {
        QStringList out;
        QTreeWidgetItemIterator it(&t);
        for (; *it; ++it)
            if ((*it)->parent() && (*it)->checkState(0) != Qt::Unchecked)
                out << (*it)->data(1, Qt::EditRole).toString() + "=" +
                           QString::number(int((*it)->checkState(0)));
        return out;
    };
    QCOMPARE(checkedIn(g), checkedIn(f));
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

/*  PER-FOLDER counts, the shape BuildFilters and Catalog::categoryItems hand over. Two
    folders share the NAME "DxO"; "2020-2029" and "2020" hold no images of their own. */
static QMap<QString, int> folderCounts()
{
    return {
        {"/U/Photos/2020-2029/2020/DxO", 3},
        {"/U/Photos/2020-2029/2021/DxO", 4},
        {"/U/Photos/Zen", 5},
    };
}

void tst_filters::foldersBuildAsATreeUnderTheirAnchors()
{
    Filters f(nullptr);
    f.setFolderAnchors({"/U/Photos"});
    f.addCategoryItems(folderCounts(), f.folders);

    QVERIFY(f.isNestedCategory(f.folders));
    QCOMPARE(f.folders->childCount(), 1);                   // the anchor, not "/"
    QTreeWidgetItem *photos = findByValue(f.folders, "/U/Photos");
    QVERIFY(photos);
    QCOMPARE(photos->text(0), QString("Photos"));

    // synthesised parents, labelled by name, valued by path
    QTreeWidgetItem *decade = findByValue(f.folders, "/U/Photos/2020-2029");
    QVERIFY(decade);
    QCOMPARE(decade->parent(), photos);
    QTreeWidgetItem *dxo20 = findByValue(f.folders, "/U/Photos/2020-2029/2020/DxO");
    QTreeWidgetItem *dxo21 = findByValue(f.folders, "/U/Photos/2020-2029/2021/DxO");
    QVERIFY(dxo20 && dxo21 && dxo20 != dxo21);
    QCOMPARE(dxo20->text(0), QString("DxO"));
    QCOMPARE(f.itemMapKey(f.folders, dxo20), QString("/U/Photos/2020-2029/2020/DxO"));
}

void tst_filters::folderCountsRollUpAndStateSurvivesARebuild()
{
    Filters f(nullptr);
    f.setFolderAnchors({"/U/Photos"});
    f.addCategoryItems(folderCounts(), f.folders);

    // a node counts everything beneath it
    QCOMPARE(findByValue(f.folders, "/U/Photos")->data(3, Qt::EditRole).toInt(), 12);
    QCOMPARE(findByValue(f.folders, "/U/Photos/2020-2029")->data(3, Qt::EditRole).toInt(),
             7);

    // filtered counts arrive per folder too, and roll up the same way
    f.updateFilteredCountPerItem({{"/U/Photos/2020-2029/2020/DxO", 3}}, f.folders);
    QCOMPARE(findByValue(f.folders, "/U/Photos/2020-2029")->data(2, Qt::EditRole).toInt(),
             3);
    QCOMPARE(findByValue(f.folders, "/U/Photos/Zen")->data(2, Qt::EditRole).toInt(), 0);

    // an edit-driven rebuild keeps the check and the expansion, keyed on the path
    QTreeWidgetItem *decade = findByValue(f.folders, "/U/Photos/2020-2029");
    decade->setCheckState(0, Qt::Checked);
    decade->setExpanded(true);
    f.updateCategoryItems(folderCounts(), f.folders);
    QTreeWidgetItem *again = findByValue(f.folders, "/U/Photos/2020-2029");
    QVERIFY(again);
    QCOMPARE(again->checkState(0), Qt::Checked);
    QVERIFY(again->isExpanded());

    // and save/restore across a folder-change rebuild
    f.save();
    f.folders->takeChildren();
    f.addCategoryItems(folderCounts(), f.folders);
    f.restore();
    QCOMPARE(findByValue(f.folders, "/U/Photos/2020-2029")->checkState(0), Qt::Checked);
}

void tst_filters::foldersReachTheQueryByPath()
{
    Filters f(nullptr);
    f.setFolderAnchors({"/U/Photos"});
    f.addCategoryItems(folderCounts(), f.folders);
    findByValue(f.folders, "/U/Photos/2020-2029")->setCheckState(0, Qt::Checked);
    findByValue(f.folders, "/U/Photos/2020-2029/2021/DxO")
        ->setCheckState(0, Qt::PartiallyChecked);

    CatalogQuery q;
    f.fillQuery(q);
    QCOMPARE(q.include.value(G::FolderPathsAllColumn),
             QStringList{"/U/Photos/2020-2029"});
    QCOMPARE(q.exclude.value(G::FolderPathsAllColumn),
             QStringList{"/U/Photos/2020-2029/2021/DxO"});
    QVERIFY2(!q.include.contains(G::FolderNameColumn), "a folder NAME must not reach it");
}

void tst_filters::aNestedCategoryIsCountedAtEveryDepth()
{
/*
    One catalogued root is ONE top-level folder with everything beneath it. Counting the
    category's CHILDREN called that "a single item, nothing to filter", and never saw a
    check below the top level -- the header stayed unlit while the category filtered.
*/
    Filters f(nullptr);
    f.setFolderAnchors({"/U/Photos"});
    f.addCategoryItems(folderCounts(), f.folders);
    QCOMPARE(f.folders->childCount(), 1);
    QVERIFY(!f.isCatFiltering(f.folders));

    findByValue(f.folders, "/U/Photos/2020-2029/2021/DxO")->setCheckState(0, Qt::Checked);
    QVERIFY2(f.isCatFiltering(f.folders), "a check four levels down is still filtering");

    f.addCategoryItems(vocabulary(), f.keywords);
    findByValue(f.keywords, "Location|Canada|BC")->setCheckState(0, Qt::Checked);
    QVERIFY(f.isCatFiltering(f.keywords));
}

void tst_filters::keywordMatchModeTravelsAndResets()
{
/*
    Any/all for Keywords lives on the category header (compiled into the predicate), is
    shown as its label, reaches the catalog query, survives the save/restore around a
    folder add, and goes back to "any" when the filters are reset for a new set.
*/
    Filters f(nullptr);
    f.addCategoryItems(vocabulary(), f.keywords);
    QVERIFY(!f.keywordsMatchAll());
    QCOMPARE(f.keywords->text(2), QString("any"));

    findByValue(f.keywords, "Fauna")->setCheckState(0, Qt::Checked);
    findByValue(f.keywords, "Location|USA")->setCheckState(0, Qt::Checked);

    QSignalSpy spy(&f, &Filters::filterChange);
    f.setKeywordsMatchAll(true);
    QCOMPARE(spy.count(), 1);                       // two included: it changes the result
    QVERIFY(f.keywords->data(0, Filters::MatchAllRole).toBool());
    QCOMPARE(f.keywords->text(2), QString("all"));

    CatalogQuery q;
    f.fillQuery(q);
    QVERIFY(q.keywordsMatchAll);

    f.save();
    f.keywords->setData(0, Filters::MatchAllRole, false);
    f.restore();
    QVERIFY2(f.keywordsMatchAll(), "restore() must bring the mode back with the checks");

    f.reset();
    QVERIFY2(!f.keywordsMatchAll(), "a new set starts at any");
    QCOMPARE(f.keywords->text(2), QString("any"));

    // with fewer than two included the mode changes nothing, so nothing is refiltered
    f.addCategoryItems(vocabulary(), f.keywords);
    findByValue(f.keywords, "Fauna")->setCheckState(0, Qt::Checked);
    spy.clear();
    f.setKeywordsMatchAll(true);
    QCOMPARE(spy.count(), 0);
}

QTEST_MAIN(tst_filters)
#include "tst_filters.moc"
