/*
    Pins Utilities/foldertree.h -- the rules both folder trees are built from (the Source
    panel's LibTree and the Filters panel's Folders category). See "The Library Tree
    (LibTree)" in notes/Documentation.txt.

    The cases are the ones that bit elsewhere: a trailing slash that defeats a prefix
    test, "/Photos/2024 raw" read as inside "/Photos/2024", two folders with the same
    NAME, and Windows drive roots.
*/

#include <QtTest>
#include "Utilities/foldertree.h"

using namespace FolderTree;

class TstFolderTree : public QObject
{
    Q_OBJECT
private slots:
    void normalizeKeepsRootsAndDropsTrailingSlash();
    void ancestryUnix();
    void ancestryWindowsAndUnc();
    void parentAndLeaf();
    void atOrUnderNeedsTheSeparator();
    void expandCountsRollsUpExactly();
    void commonAncestorAcrossDrives();
    void hierarchyWithAnchorsSynthesisesParents();
    void hierarchyWithoutAnchorsUsesCommonAncestor();
    void hierarchyFolderOutsideEveryAnchor();
    void hierarchyNestedAnchorOutermostWins();
    void hierarchyDuplicateAnchorNames();
    void hierarchySiblingsInNaturalOrder();
    void outermostKeepsOnlyTheRoots();
    void selectedSiblingsAreTwoRows();
};

void TstFolderTree::normalizeKeepsRootsAndDropsTrailingSlash()
{
    QCOMPARE(normalize("/Photos/"), QString("/Photos"));
    QCOMPARE(normalize("/"), QString("/"));
    QCOMPARE(normalize("C:/"), QString("C:/"));
#ifdef Q_OS_WIN
    // fromNativeSeparators only rewrites backslashes where they ARE the separator
    QCOMPARE(normalize("C:\\Photos\\2020\\"), QString("C:/Photos/2020"));
#endif
}

void TstFolderTree::ancestryUnix()
{
    QCOMPARE(ancestry("/Users/r/Photos"),
             QStringList({"/", "/Users", "/Users/r", "/Users/r/Photos"}));
    QCOMPARE(ancestry("/"), QStringList({"/"}));
    QCOMPARE(ancestry("/Photos/"), QStringList({"/", "/Photos"}));
    QVERIFY(ancestry("").isEmpty());
}

void TstFolderTree::ancestryWindowsAndUnc()
{
    QCOMPARE(ancestry("C:/Photos/2020"),
             QStringList({"C:/", "C:/Photos", "C:/Photos/2020"}));
    QCOMPARE(ancestry("C:/"), QStringList({"C:/"}));
    QCOMPARE(ancestry("//srv/share/x"),
             QStringList({"//srv", "//srv/share", "//srv/share/x"}));
}

void TstFolderTree::parentAndLeaf()
{
    QCOMPARE(parentOf("/Users/r"), QString("/Users"));
    QCOMPARE(parentOf("/Users"), QString("/"));
    QCOMPARE(parentOf("/"), QString());
    QCOMPARE(parentOf("C:/Photos"), QString("C:/"));
    QCOMPARE(parentOf("C:/"), QString());
    QCOMPARE(parentOf("//srv"), QString());
    QCOMPARE(leafOf("/Users/r/DxO"), QString("DxO"));
    QCOMPARE(leafOf("/"), QString("/"));
    QCOMPARE(leafOf("C:/"), QString("C:"));
}

void TstFolderTree::atOrUnderNeedsTheSeparator()
{
    QVERIFY(isAtOrUnder("/Photos/2024", "/Photos/2024"));
    QVERIFY(isAtOrUnder("/Photos/2024/a", "/Photos/2024"));
    QVERIFY(!isAtOrUnder("/Photos/2024 raw", "/Photos/2024"));
    QVERIFY(isAtOrUnder("/Photos", "/"));
    QVERIFY(isAtOrUnder("C:/Photos", "C:/"));
    QVERIFY(!isAtOrUnder("/Photos", ""));
}

void TstFolderTree::expandCountsRollsUpExactly()
{
    QMap<QString, int> raw;
    raw["/P/2020/a"] = 3;
    raw["/P/2020/b"] = 4;
    raw["/P/2021"] = 5;
    raw[""] = 99;                       // a row with no folder is not a folder
    const QMap<QString, int> x = expandCounts(raw);
    QCOMPARE(x.value("/P/2020/a"), 3);
    QCOMPARE(x.value("/P/2020"), 7);
    QCOMPARE(x.value("/P"), 12);
    QCOMPARE(x.value("/"), 12);
    QVERIFY(!x.contains(""));
}

void TstFolderTree::commonAncestorAcrossDrives()
{
    QCOMPARE(commonAncestor({"/P/2020/a", "/P/2021"}), QString("/P"));
    QCOMPARE(commonAncestor({"/P/2020/a"}), QString("/P/2020/a"));
    QCOMPARE(commonAncestor({"C:/P", "D:/P"}), QString());
    QCOMPARE(commonAncestor({"/A/x", "/B/y"}), QString("/"));
}

static QStringList paths(const QVector<Node> &nodes)
{
    QStringList out;
    for (const Node &n : nodes) out << n.path;
    return out;
}

static Node nodeFor(const QVector<Node> &nodes, const QString &path)
{
    for (const Node &n : nodes) if (n.path == path) return n;
    return Node();
}

void TstFolderTree::hierarchyWithAnchorsSynthesisesParents()
{
    /* "2020-2029" and "2020" hold no images and still appear, under the anchor; nothing
       above the anchor does. */
    const QVector<Node> n = hierarchy({"/U/Photos/2020-2029/2020/0101",
                                       "/U/Photos/Zen"},
                                      {"/U/Photos"});
    QCOMPARE(paths(n), QStringList({"/U/Photos", "/U/Photos/2020-2029",
                                    "/U/Photos/2020-2029/2020",
                                    "/U/Photos/2020-2029/2020/0101", "/U/Photos/Zen"}));
    QVERIFY(nodeFor(n, "/U/Photos").isAnchor);
    QCOMPARE(nodeFor(n, "/U/Photos").parent, QString());
    QCOMPARE(nodeFor(n, "/U/Photos/2020-2029/2020").parent,
             QString("/U/Photos/2020-2029"));
    QCOMPARE(nodeFor(n, "/U/Photos/Zen").label, QString("Zen"));
}

void TstFolderTree::hierarchyWithoutAnchorsUsesCommonAncestor()
{
    // one folder: itself
    QCOMPARE(paths(hierarchy({"/P/2020/a"}, {})), QStringList({"/P/2020/a"}));
    // a recursive load: its tree
    QCOMPARE(paths(hierarchy({"/Z/a", "/Z/b"}, {})),
             QStringList({"/Z", "/Z/a", "/Z/b"}));
    // unrelated folders whose only common ancestor is a root: side by side
    const QVector<Node> n = hierarchy({"/A/x", "/B/y"}, {});
    QCOMPARE(paths(n), QStringList({"/A/x", "/B/y"}));
    QVERIFY(nodeFor(n, "/A/x").isAnchor && nodeFor(n, "/B/y").isAnchor);
}

void TstFolderTree::hierarchyFolderOutsideEveryAnchor()
{
    const QVector<Node> n = hierarchy({"/P/a", "/Elsewhere/b", "/Elsewhere/b/c"}, {"/P"});
    QCOMPARE(paths(n), QStringList({"/Elsewhere/b", "/Elsewhere/b/c", "/P", "/P/a"}));
    QVERIFY(nodeFor(n, "/Elsewhere/b").isAnchor);
    QVERIFY(!nodeFor(n, "/Elsewhere/b/c").isAnchor);
}

void TstFolderTree::hierarchyNestedAnchorOutermostWins()
{
    const QVector<Node> n = hierarchy({"/P/2020/a"}, {"/P/2020", "/P"});
    QCOMPARE(paths(n), QStringList({"/P", "/P/2020", "/P/2020/a"}));
    QVERIFY(!nodeFor(n, "/P/2020").isAnchor);
}

void TstFolderTree::hierarchyDuplicateAnchorNames()
{
    const QVector<Node> n = hierarchy({"/A/Photos/x", "/B/Photos/y"},
                                      {"/A/Photos", "/B/Photos"});
    QCOMPARE(nodeFor(n, "/A/Photos").label, QString("Photos (A)"));
    QCOMPARE(nodeFor(n, "/B/Photos").label, QString("Photos (B)"));
    QCOMPARE(nodeFor(n, "/A/Photos/x").label, QString("x"));
}

void TstFolderTree::hierarchySiblingsInNaturalOrder()
{
    const QVector<Node> n = hierarchy({"/P/10", "/P/2", "/P/b", "/P/A"}, {"/P"});
    QCOMPARE(paths(n), QStringList({"/P", "/P/2", "/P/10", "/P/A", "/P/b"}));
}

void TstFolderTree::outermostKeepsOnlyTheRoots()
{
    QCOMPARE(outermost({"/Z", "/Z/a", "/Z/a/b", "/Y/", "/Y"}), QStringList({"/Z", "/Y"}));
    QCOMPARE(outermost({"/P/2024 raw", "/P/2024"}),
             QStringList({"/P/2024 raw", "/P/2024"}));
}

void TstFolderTree::selectedSiblingsAreTwoRows()
{
    /* The reported case: _test1 and _test2 selected in the Folders tree. Anchored on the
       selection they are two top rows; anchored on their common parent they were one. */
    const QVector<Node> n = hierarchy({"/U/Pictures/_test1", "/U/Pictures/_test2"},
                                      outermost({"/U/Pictures/_test1",
                                                 "/U/Pictures/_test2"}));
    QCOMPARE(paths(n), QStringList({"/U/Pictures/_test1", "/U/Pictures/_test2"}));
    QVERIFY(nodeFor(n, "/U/Pictures/_test1").isAnchor);
    QVERIFY(nodeFor(n, "/U/Pictures/_test2").isAnchor);
}

QTEST_GUILESS_MAIN(TstFolderTree)
#include "tst_foldertree.moc"
