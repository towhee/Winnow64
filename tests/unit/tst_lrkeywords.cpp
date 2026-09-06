#include <QtTest>
#include <QBuffer>
#include <QFile>

#include "Metadata/lrkeywords.h"

/*
    THE LIGHTROOM KEYWORD FILE.

    PINNED AGAINST A REAL EXPORT, which matters more here than usual: the format that is
    commonly described -- a tab-indented .txt with [brackets] and {braces} -- is not what
    Lightroom actually wrote. A 3,975-keyword export turned out to be a five-column CSV
    with the hierarchy as TAB indentation inside the last field. Everything below was read
    off that file, and tests/fixtures/lightroom_keywords.csv is a trimmed copy preserving
    each shape it contains: nesting, a synonym, consecutive synonyms, an ampersand, and a
    leaf name used in two different branches.

    So these cases are not "does the parser agree with itself" -- the fixture is evidence,
    and a change that breaks them is a change that stops reading the user's own file.
*/
class tst_lrkeywords : public QObject
{
    Q_OBJECT

private slots:
    void parsesTheRealFixture();
    void hierarchyComesFromTabDepth();
    void synonymsAttachToTheirKeyword();
    void consecutiveSynonymsShareAParent();
    void sameLeafInTwoBranchesStaysTwoKeywords();
    void aQuotedFieldWithACommaSurvives();
    void abruptDepthIsClampedNotDropped();
    void bracketsAreNotGuessedAt();
    void writeRoundTripsThroughParse();

private:
    static QList<LrKeyword> parseText(const QByteArray &csv)
    {
        QBuffer buf;
        buf.setData(csv);
        buf.open(QIODevice::ReadOnly);
        return lrParse(&buf);
    }
    static QList<LrKeyword> parseFixture()
    {
        QFile f(QFINDTESTDATA("../fixtures/lightroom_keywords.csv"));
        if (!f.open(QIODevice::ReadOnly)) return {};
        return lrParse(&f);
    }
};

void tst_lrkeywords::parsesTheRealFixture()
{
    const QList<LrKeyword> rows = parseFixture();
    QVERIFY2(!rows.isEmpty(), "the committed Lightroom export fixture did not parse");

    /*  The header is recognised and not taken for a keyword called
        "Include On Export". */
    for (const LrKeyword &k : rows)
        QVERIFY(k.name != "Include On Export");

    const QStringList paths = lrPaths(rows);
    QVERIFY(paths.contains("Category"));
    QVERIFY(paths.contains("Category|Forest status"));
    /*  An ampersand is an ordinary character in a keyword and must survive the read --
        it is also what proves the XMP writer has to escape on the way out. */
    QVERIFY(paths.contains("Category|Forest status|F&B"));
    QVERIFY(paths.contains("Location|Canada|BC|Vancouver"));
    QVERIFY(paths.contains("Sooke"));
}

void tst_lrkeywords::hierarchyComesFromTabDepth()
{
    const QList<LrKeyword> rows = parseFixture();
    QHash<QString, int> depthOf;
    for (const LrKeyword &k : rows) depthOf.insert(k.name, k.depth);

    QCOMPARE(depthOf.value("Category"), 0);
    QCOMPARE(depthOf.value("Forest status"), 1);
    QCOMPARE(depthOf.value("F&B"), 2);
    QCOMPARE(depthOf.value("BC"), 2);
    QCOMPARE(depthOf.value("Vancouver"), 3);

    /*  A NAME IS NEVER A PATH. The parser must not join anything: the depth column and
        lrPaths do that, and a name carrying a separator would be a second, disagreeing
        way to express hierarchy. */
    for (const LrKeyword &k : rows)
        QVERIFY2(!k.name.contains('|'), qPrintable("name held a separator: " + k.name));
}

void tst_lrkeywords::synonymsAttachToTheirKeyword()
{
    const QList<LrKeyword> rows = parseFixture();

    /*  A synonym is NOT a keyword of its own: "{Insurance}" must not appear as a row. */
    for (const LrKeyword &k : rows) {
        QVERIFY(!k.name.startsWith('{'));
        QVERIFY(k.name != "Insurance");
    }

    bool found = false;
    for (const LrKeyword &k : rows) {
        if (k.name != "Household Inventory") continue;
        found = true;
        QVERIFY(k.synonyms.contains("Insurance"));
    }
    QVERIFY2(found, "the keyword carrying the synonyms was not parsed");
}

void tst_lrkeywords::consecutiveSynonymsShareAParent()
{
/*
    THE RULE THAT IS EASY TO GET WRONG, and the reason it is stated as "the nearest
    preceding NON-SYNONYM row at depth - 1" rather than "the previous row". Two synonyms
    in a row both belong to the keyword above them; attaching the second to the first
    would lose it, because a synonym is not a node and has nothing to hold it.
*/
    const QList<LrKeyword> rows = parseFixture();
    for (const LrKeyword &k : rows) {
        if (k.name != "Household Inventory") continue;
        QCOMPARE(k.synonyms.size(), 2);
        QVERIFY(k.synonyms.contains("Insurance"));
        QVERIFY(k.synonyms.contains("Contents"));
    }
}

void tst_lrkeywords::sameLeafInTwoBranchesStaysTwoKeywords()
{
/*
    The case the whole path-identity reversal exists for, arriving through the importer:
    the fixture has Vancouver under BC and Vancouver under Washington. They must produce
    two different PATHS, or an import would silently merge them back together.
*/
    const QStringList paths = lrPaths(parseFixture());
    QVERIFY(paths.contains("Location|Canada|BC|Vancouver"));
    QVERIFY(paths.contains("Location|USA|Washington|Vancouver"));
    QCOMPARE(paths.count("Location|Canada|BC|Vancouver"), 1);
}

void tst_lrkeywords::aQuotedFieldWithACommaSurvives()
{
/*
    "Vancouver, BC" is a keyword somebody has. Splitting the record on commas would tear
    it in half and shift every column after it, so the reader honours CSV quoting -- and
    a doubled quote inside a quoted field is one literal quote.
*/
    const QByteArray csv =
        "Include On Export,Export Containing Keywords,Export Synonyms,"
        "Person Type Keyword,\n"
        "Y,Y,Y,N,Places\n"
        "Y,Y,Y,N,\"\tVancouver, BC\"\n"
        "Y,Y,Y,N,\"\tThe \"\"Old\"\" Town\"\n";

    const QList<LrKeyword> rows = parseText(csv);
    QCOMPARE(rows.size(), 3);
    QCOMPARE(rows.at(1).name, QString("Vancouver, BC"));
    QCOMPARE(rows.at(1).depth, 1);
    QCOMPARE(rows.at(2).name, QString("The \"Old\" Town"));

    const QStringList paths = lrPaths(rows);
    QVERIFY(paths.contains("Places|Vancouver, BC"));
}

void tst_lrkeywords::abruptDepthIsClampedNotDropped()
{
/*
    Lightroom does not write a row three levels deeper than its predecessor, but a
    hand-edited file can. Filing it one level too shallow leaves it visible and movable;
    dropping it loses a keyword silently, which is the worse of the two.
*/
    const QByteArray csv =
        "Y,Y,Y,N,Top\n"
        "Y,Y,Y,N,\t\t\tOrphan\n";

    const QStringList paths = lrPaths(parseText(csv));
    QCOMPARE(paths.size(), 2);
    QCOMPARE(paths.at(1), QString("Top|Orphan"));
}

void tst_lrkeywords::bracketsAreNotGuessedAt()
{
/*
    Square brackets are widely described as marking a keyword that is not exported, and
    NOTHING IN THE REAL EXPORT CONFIRMS IT -- there is not one bracket in 3,975 rows, nor
    a single "N" in Include On Export. So a bracketed name is read as a name. Acting on
    the rumour would mean silently turning off a keyword's export flag on an import,
    which is a data change made on an unverified belief.
*/
    const QByteArray csv = "Y,Y,Y,N,[Private]\n";
    const QList<LrKeyword> rows = parseText(csv);
    QCOMPARE(rows.size(), 1);
    QCOMPARE(rows.at(0).name, QString("[Private]"));
    QCOMPARE(rows.at(0).includeOnExport, true);

    /*  A flag that IS present is honoured, which is the half that can be tested. */
    const QList<LrKeyword> off = parseText(QByteArray("N,Y,Y,N,Hidden\n"));
    QCOMPARE(off.at(0).includeOnExport, false);
}

void tst_lrkeywords::writeRoundTripsThroughParse()
{
/*
    What is written must read back as what was written -- otherwise exporting and
    re-importing a vocabulary would quietly reshape it.
*/
    const QStringList paths = {
        "Category",
        "Category|Forest status",
        "Category|Forest status|F&B",
        "Location",
        "Location|Canada",
        "Location|Canada|BC",
        "Location|Canada|BC|Vancouver, BC",
        "Sooke"
    };
    QHash<QString, QStringList> syn;
    syn.insert(keywordFold("Category|Forest status"), {"Woods", "Timber"});

    QByteArray written;
    {
        QBuffer buf(&written);
        buf.open(QIODevice::WriteOnly);
        QTextStream out(&buf);
        lrWrite(out, paths, syn);
    }

    const QList<LrKeyword> rows = parseText(written);
    QCOMPARE(lrPaths(rows), paths);

    for (const LrKeyword &k : rows) {
        if (k.name != "Forest status") continue;
        QCOMPARE(k.synonyms.size(), 2);
        QVERIFY(k.synonyms.contains("Timber"));
    }
}

QTEST_APPLESS_MAIN(tst_lrkeywords)
#include "tst_lrkeywords.moc"
