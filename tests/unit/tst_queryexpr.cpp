#include <QtTest>
#include "Main/global.h"
#include "Utilities/queryexpr.h"

/*
    tst_queryexpr pins the query grammar behind the Search row, the Query Builder and the
    Queries panel (Utilities/queryexpr.h).

    Three promises matter most: plain words mean what they meant before field rules
    existed; text and JSON both ROUND-TRIP (the builder and the Search row are two views
    of one query, and a saved Query is a published format); and each operator compares
    the way the column actually stores its value -- dates as text, ratings blank when
    unrated, keywords with every ancestor.
*/
class tst_queryexpr : public QObject
{
    Q_OBJECT

private slots:
    void cleanup();

    void plainWordsKeepTheirMeaning();
    void nestedNotIsHonoured();
    void textRoundTrips_data();
    void textRoundTrips();
    void builderTreeSurvivesText();
    void jsonRoundTrips();
    void numbers();
    void dates();
    void enumsAndLists();
    void textOperators();
    void unknownFieldIsAnErrorAndAWord();
    void emptyGroupsRestrictNothing();
    void columnsRead();

private:
    using Row = QHash<int, QVariant>;
    static bool hit(const QString &text, const Row &row)
    {
        Query::Expr e = Query::Expr::parse(text);
        e.prepare();
        return e.matches([&row](int c) { return row.value(c); });
    }
    static Row words(const QString &s) { return Row{{G::SearchTextColumn, s}}; }
};

void tst_queryexpr::cleanup()
{
    Query::setResolver(nullptr);
}

void tst_queryexpr::plainWordsKeepTheirMeaning()
{
    const Row r = words("great blue heron at nanaimo");
    QVERIFY(hit("heron nanaimo", r));
    QVERIFY(!hit("heron eagle", r));
    QVERIFY(hit("eagle OR heron", r));
    QVERIFY(hit("\"great blue\"", r));
    QVERIFY(!hit("\"blue great\"", r));
    QVERIFY(!hit("-heron", r));
    QVERIFY(!hit("NOT heron", r));
    QVERIFY(hit("HERON", r));                       // case-insensitive
    QVERIFY(hit("eron", r));                        // a substring, as before
    QVERIFY(hit("(eagle OR heron) nanaimo", r));
    QVERIFY(hit("heron or", words("black or white heron")));  // "or" is a word
    // AND binds tighter than OR: heron OR (eagle AND tofino)
    QVERIFY(hit("heron OR eagle tofino", r));
}

void tst_queryexpr::nestedNotIsHonoured()
{
    /*  The old grammar lifted every negative to the top, so "(heron OR -eagle)" meant
        "heron AND NOT eagle". A nested NOT now means what it says. */
    QVERIFY(hit("(heron OR -eagle)", words("eagle and heron")));
    QVERIFY(hit("(heron OR -eagle)", words("owl")));
    QVERIFY(!hit("(heron OR -eagle)", words("eagle")));
}

void tst_queryexpr::textRoundTrips_data()
{
    QTest::addColumn<QString>("text");
    // each is already canonical: parse -> toText gives it back exactly
    QTest::newRow("words") << "heron nanaimo";
    QTest::newRow("or") << "heron OR eagle";
    QTest::newRow("phrase") << "\"great blue\" -heron";
    QTest::newRow("brackets") << "(heron OR eagle) tide";
    QTest::newRow("numbers") << "rating:>=3 iso:100..400 -rating:5 focal:<400";
    QTest::newRow("dates") << "captured:2024-05 captured:<2024-05-01 "
                              "captured:2024-05-01..2024-06-30";
    // a value is quoted only when it has to be: "|" does not need it
    QTest::newRow("lists") << "label:Red,Yellow -label:Red keyword:Fauna|Bird keyword:*";
    QTest::newRow("quoted value") << "keyword:\"Location|North America\" -title:\"a, b\"";
    QTest::newRow("text ops") << "title:=Heron title:her* title:*ron -title:* "
                                 "creator:\"Rory Hill\"";
    QTest::newRow("none group") << "-(heron OR eagle) rating:>=3";
    QTest::newRow("nested") << "(camera:\"Z 9\" OR (camera:Z8 lens:*)) -(rating:<2 OR "
                               "label:Red)";
    QTest::newRow("uncomparable not") << "-rating:<3";
}

void tst_queryexpr::textRoundTrips()
{
    QFETCH(QString, text);
    const Query::Expr e = Query::Expr::parse(text);
    QVERIFY2(e.error().isEmpty(), qPrintable(e.error()));
    QCOMPARE(e.toText(), text);
    QVERIFY(Query::Expr::parse(e.toText()).root == e.root);
}

void tst_queryexpr::builderTreeSurvivesText()
{
    /*  A tree as the builder makes it: ALL of { a rule, ANY of {two rules}, NONE of {a
        rule, a group} }. Text and back must give the same tree. */
    using namespace Query;
    Node any = Node::group(Node::Any);
    any.kids << Node::rule("keyword", Op::AnyOf, {"Fauna|Bird"})
             << Node::rule("title", Op::Contains, {"heron"});
    Node inner = Node::group(Node::All);
    inner.kids << Node::rule("iso", Op::Gt, {"3200"})
               << Node::rule("aperture", Op::Le, {"4"});
    Node none = Node::group(Node::None);
    none.kids << Node::rule("label", Op::AnyOf, {"Red"}) << inner;
    Expr e;
    e.root = Node::group(Node::All);
    e.root.kids << Node::rule("rating", Op::Ge, {"3"}) << any << none;

    const Expr back = Expr::parse(e.toText());
    QVERIFY2(back.error().isEmpty(), qPrintable(back.error()));
    QVERIFY2(back.root == e.root, qPrintable(e.toText() + "  ->  " + back.toText()));
}

void tst_queryexpr::jsonRoundTrips()
{
    const Query::Expr e = Query::Expr::parse(
        "(heron OR eagle) rating:>=3 -label:Red,Purple captured:2024-05-01..2024-06-30 "
        "-(camera:Z8 lens:*)");
    const Query::Expr back = Query::Expr::fromJsonText(e.toJsonText());
    QVERIFY(back.root == e.root);
    QCOMPARE(e.toJson().value("version").toInt(), 1);
    // an empty or unreadable definition is the empty query, never a crash
    QVERIFY(Query::Expr::fromJsonText("").isEmpty());
    QVERIFY(Query::Expr::fromJsonText("{nonsense").isEmpty());
}

void tst_queryexpr::numbers()
{
    const Row unrated{{G::RatingColumn, ""}};
    const Row three{{G::RatingColumn, "3"}, {G::ISOColumn, 800},
                    {G::ShutterspeedColumn, 0.004}, {G::ApertureColumn, 5.6}};
    QVERIFY(hit("rating:<2", unrated));             // unrated is 0
    QVERIFY(!hit("rating:>=3", unrated));
    QVERIFY(hit("rating:>=3", three));
    QVERIFY(hit("rating:3", three));
    QVERIFY(!hit("-rating:3", three));
    QVERIFY(hit("rating:2..4", three));
    QVERIFY(hit("iso:>400", three));
    QVERIFY(!hit("iso:<=400", three));
    QVERIFY(hit("shutter:1/250", three));           // shutter is written as a fraction
    QVERIFY(hit("shutter:<1/100", three));
    QVERIFY(hit("aperture:5.6", three));
    QVERIFY(hit("-focal:*", three));                // no focal length at all
    QVERIFY(!hit("focal:>0", three));
}

void tst_queryexpr::dates()
{
    // the column holds "yyyy-MM-dd hh:mm:ss.zzz" text
    const Row may{{G::CreatedColumn, "2024-05-17 10:31:02.000"}};
    QVERIFY(hit("captured:2024", may));
    QVERIFY(hit("captured:2024-05", may));
    QVERIFY(hit("captured:2024-05-17", may));
    QVERIFY(!hit("captured:2024-06", may));
    QVERIFY(hit("captured:<2024-06", may));
    QVERIFY(!hit("captured:<2024-05", may));        // before May began
    QVERIFY(hit("captured:>2024-04-30", may));
    QVERIFY(!hit("captured:>2024-05", may));        // after May ended
    QVERIFY(hit("captured:2024-05-01..2024-05-31", may));
    QVERIFY(hit("captured:2024-05-31..2024-05-01", may));   // either order
    QVERIFY(!hit("captured:2024-06-01..2024-06-30", may));
    QVERIFY(hit("-captured:*", Row{{G::CreatedColumn, ""}}));
}

void tst_queryexpr::enumsAndLists()
{
    const Row r{{G::LabelColumn, "Red"}, {G::PickColumn, "Rejected"},
                {G::HasGPSColumn, "True"}, {G::DevelopColumn, false},
                {G::KeywordsAllColumn, QStringList{"Fauna", "Fauna|Bird",
                                                   "Fauna|Bird|Heron"}},
                {G::CollectionsColumn, QStringList{"12"}}};
    QVERIFY(hit("label:red,yellow", r));            // case-insensitive
    QVERIFY(!hit("-label:red", r));
    QVERIFY(hit("pick:rejected", r));
    QVERIFY(hit("gps:yes", r));
    QVERIFY(hit("developed:no", r));
    QVERIFY(hit("keyword:\"Fauna|Bird\"", r));      // a branch reaches its leaves
    QVERIFY(!hit("keyword:\"Fauna|Mammal\"", r));
    QVERIFY(!hit("-keyword:*", r));
    QVERIFY(hit("-keyword:Flora", r));

    // collections by NAME: the resolver maps it to the ids the column holds
    Query::setResolver([](const QString &field, const QString &value) {
        if (field == "collection" && value.compare("Trip 2024", Qt::CaseInsensitive) == 0)
            return QStringList{"12"};
        return QStringList();
    });
    QVERIFY(hit("collection:\"trip 2024\"", r));
    QVERIFY(!hit("collection:Other", r));
    QVERIFY(hit("-collection:Other", r));
}

void tst_queryexpr::textOperators()
{
    const Row r{{G::TitleColumn, "Great Blue Heron"}, {G::LensColumn, ""}};
    QVERIFY(hit("title:blue", r));
    QVERIFY(hit("title:\"great blue\"", r));
    QVERIFY(hit("title:=\"great blue heron\"", r));
    QVERIFY(!hit("title:=heron", r));
    QVERIFY(hit("title:great*", r));
    QVERIFY(hit("title:*heron", r));
    QVERIFY(!hit("title:heron*", r));
    QVERIFY(hit("title:*", r));
    QVERIFY(hit("-lens:*", r));
    QVERIFY(hit("title:\"*\"", Row{{G::TitleColumn, "a*b"}}));  // quoted is literal
}

void tst_queryexpr::unknownFieldIsAnErrorAndAWord()
{
    const Query::Expr e = Query::Expr::parse("colour:red");
    QVERIFY(!e.error().isEmpty());
    QVERIFY(hit("colour:red", words("x colour:red y")));
    // not a plain name: simply a word, no error
    QVERIFY(Query::Expr::parse("10:30").error().isEmpty());
    QVERIFY(hit("10:30", words("at 10:30 am")));
}

void tst_queryexpr::emptyGroupsRestrictNothing()
{
    using namespace Query;
    Expr e;
    e.root.kids << Node::group(Node::Any) << Node::group(Node::None);
    e.prepare();
    QVERIFY(e.isEmpty());
    QVERIFY(e.matches([](int) { return QVariant(); }));
    QVERIFY(Expr::parse("").isEmpty());
    QVERIFY(Expr::parse("   ").isEmpty());
}

void tst_queryexpr::columnsRead()
{
    const QSet<int> cols =
        Query::Expr::parse("heron rating:>=3 -(label:Red keyword:*)").columnsRead();
    QCOMPARE(cols, (QSet<int>{G::SearchTextColumn, G::RatingColumn, G::LabelColumn,
                              G::KeywordsAllColumn}));
}

QTEST_MAIN(tst_queryexpr)
#include "tst_queryexpr.moc"
