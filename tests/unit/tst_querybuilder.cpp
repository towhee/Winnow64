#include <QtTest>
#include <QComboBox>
#include <QLineEdit>
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
    QLineEdit *text = nullptr;
    for (QLineEdit *e : w.findChildren<QLineEdit *>())
        if (e->toolTip().startsWith("The same query")) text = e;
    QVERIFY(text);
    text->setText("rating:5 -label:Red");
    emit text->returnPressed();
    QCOMPARE(w.expr().toText(), QString("rating:5 -label:Red"));
}

QTEST_MAIN(tst_querybuilder)
#include "tst_querybuilder.moc"
