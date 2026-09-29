#include <QtTest>
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include "Dialogs/querybuilder.h"

/*
    tst_querybuilder drives the Query Builder (Dialogs/querybuilder.h) headless.

    It exists because the builder crashed the first time anyone opened it: each value
    box's completer was replaced on every field change, and QLineEdit::setCompleter had
    already deleted the one being replaced -- so loading a query (which sets the field,
    then refreshes it) freed it twice. Constructing the dialog and loading a query is
    enough to catch that class of bug, so this does exactly that, in both modes, and
    checks that what goes in comes back out.
*/
class tst_querybuilder : public QObject
{
    Q_OBJECT

private slots:
    void opensEmptyInBothModes();
    void loadsAndReturnsANestedQuery();
    void suggestionsCanBeSetAgain();
    void textBoxRebuildsTheRows();
    void aValueTheLibraryLacksIsRed();
    void testButtonCountsAndClearsWhenTheQueryChanges();
};

void tst_querybuilder::opensEmptyInBothModes()
{
    QueryBuilderDialog adHoc(QueryBuilderDialog::AdHoc);
    QVERIFY(adHoc.expr().isEmpty());          // the blank starting row is not a rule
    QueryBuilderDialog saved(QueryBuilderDialog::SavedQuery);
    saved.setName("Mine");
    QCOMPARE(saved.name(), QString("Mine"));
    QVERIFY(saved.expr().isEmpty());
}

void tst_querybuilder::loadsAndReturnsANestedQuery()
{
    const QString text = "rating:>=3 (keyword:Fauna|Bird OR title:heron) "
                         "-(label:Red iso:>3200) captured:2024-05-01..2024-06-30";
    QueryBuilderDialog dlg(QueryBuilderDialog::SavedQuery);
    dlg.setExpr(Query::Expr::parse(text));
    QCOMPARE(dlg.expr().toText(), text);
    // and again: a second load reuses every row widget's completer
    dlg.setExpr(Query::Expr::parse("heron OR eagle"));
    QCOMPARE(dlg.expr().toText(), QString("heron OR eagle"));

    /*  THE REPLACED ROWS ARE GONE FROM VIEW. They are deleted later, and a deferred
        delete does not run inside a modal dialog's exec(), so a row that was only taken
        out of the layout stayed painted over the group header. Visible now: the root
        group's Match box and two rules of two combos each. */
    int shown = 0;
    for (QComboBox *c : dlg.findChildren<QComboBox *>())
        if (c->isVisibleTo(&dlg)) ++shown;
    QCOMPARE(shown, 5);
}

void tst_querybuilder::suggestionsCanBeSetAgain()
{
    QueryBuilderDialog dlg(QueryBuilderDialog::AdHoc);
    int asked = 0;
    dlg.builder()->setSuggestions([&asked](const QString &) {
        ++asked;
        return QStringList{"Z 9", "Z 8"};
    });
    dlg.setExpr(Query::Expr::parse("camera:Z8 lens:*"));
    dlg.builder()->setSuggestions([](const QString &) { return QStringList{"x"}; });
    QVERIFY(asked > 0);
    QCOMPARE(dlg.expr().toText(), QString("camera:Z8 lens:*"));
}

void tst_querybuilder::textBoxRebuildsTheRows()
{
    QueryBuilderWidget w;
    auto *text = w.findChild<QPlainTextEdit *>();
    QVERIFY(text);
    text->setPlainText("rating:5\n-label:Red");     // a line break reads as a space
    QTest::keyClick(text, Qt::Key_Return);
    QCOMPARE(w.expr().toText(), QString("rating:5 -label:Red"));
}

static QLineEdit *valueBox(QWidget *w, const QString &text)
{
    for (QLineEdit *e : w->findChildren<QLineEdit *>())
        if (e->text() == text && e->isVisibleTo(w)) return e;
    return nullptr;
}

void tst_querybuilder::aValueTheLibraryLacksIsRed()
{
    QueryBuilderDialog dlg(QueryBuilderDialog::AdHoc);
    dlg.builder()->setSuggestions([](const QString &key) {
        return key == "camera" ? QStringList{"Z 9", "Z 8"} : QStringList();
    });
    dlg.setExpr(Query::Expr::parse("camera:Z7 iso:12345"));
    QLineEdit *bad = valueBox(&dlg, "Z7");
    QVERIFY(bad);
    QVERIFY(bad->styleSheet().contains("#d06060"));
    // a number field offers nothing, so nothing typed there is "missing"
    QLineEdit *iso = valueBox(&dlg, "12345");
    QVERIFY(iso);
    QVERIFY(iso->styleSheet().isEmpty());
    // on its way to a match is not a mistake
    bad->setText("Z");
    QVERIFY(bad->styleSheet().isEmpty());
}

void tst_querybuilder::testButtonCountsAndClearsWhenTheQueryChanges()
{
    QueryBuilderDialog dlg(QueryBuilderDialog::SavedQuery);
    QPushButton *test = nullptr;
    for (QPushButton *b : dlg.findChildren<QPushButton *>())
        if (b->text() == "Test") test = b;
    QVERIFY(test);
    QVERIFY(!test->isVisibleTo(&dlg));              // no counter, no button
    dlg.setCounter([](const Query::Expr &) { return QPair<int, int>{1284, 43050}; });
    QVERIFY(test->isVisibleTo(&dlg));

    dlg.setExpr(Query::Expr::parse("rating:>=3"));
    test->click();
    QLabel *result = nullptr;
    for (QLabel *l : dlg.findChildren<QLabel *>())
        if (l->text().contains("loaded images match")) result = l;
    QVERIFY(result);
    QVERIFY(result->text().startsWith(QLocale().toString(1284)));

    // the count belongs to the query it was made for
    auto *text = dlg.findChild<QPlainTextEdit *>();
    text->setPlainText("rating:5");
    QTest::keyClick(text, Qt::Key_Return);
    QVERIFY(result->text().isEmpty());
}

QTEST_MAIN(tst_querybuilder)
#include "tst_querybuilder.moc"
