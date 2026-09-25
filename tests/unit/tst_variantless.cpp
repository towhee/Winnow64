#include <QtTest>
#include <QStandardItemModel>
#include <QSortFilterProxyModel>
#include <QRandomGenerator>
#include <algorithm>

#include "Datamodel/variantless.h"

/*
    SortFilter::lessThan compares stored keys with winnowVariantLessThan instead of
    letting QSortFilterProxyModel fetch both values and call its private
    isVariantLessThan. Qt still does the SORTING -- SortFilter replaces only the
    comparator -- so the proxy's order is unchanged exactly when the two comparators
    agree. Two checks:

    comparatorAgreesPairwise: for every ordered pair of values, under every case and
    locale setting, the copy answers what Qt's base lessThan answers.

    sameOrderAsQt: two proxies over the same model -- a stock one, and one whose
    lessThan reads stored keys the way SortFilter does -- driven through the same
    sorts in the same sequence, must hold the same rows in the same order. (Qt's sort
    is stable relative to the proxy's CURRENT order, not source order, which is why
    this compares two proxies with the same history rather than a std::stable_sort
    written here: that first draft of this test disagreed with Qt on every descending
    tie for exactly that reason.)
*/
class BaseProxy : public QSortFilterProxyModel
{
public:
    bool baseLess(const QModelIndex &l, const QModelIndex &r) const
    {
        return QSortFilterProxyModel::lessThan(l, r);
    }
};

/*  The SortFilter arrangement in miniature: keys by SOURCE row, compared with the copy. */
class KeyedProxy : public QSortFilterProxyModel
{
public:
    QList<QVariant> keys;
protected:
    bool lessThan(const QModelIndex &l, const QModelIndex &r) const override
    {
        return winnowVariantLessThan(keys.at(l.row()), keys.at(r.row()),
                                     sortCaseSensitivity(), isSortLocaleAware());
    }
};

class tst_variantless : public QObject
{
    Q_OBJECT

private slots:
    void comparatorAgreesPairwise_data();
    void comparatorAgreesPairwise();
    void sameOrderAsQt_data();
    void sameOrderAsQt();

private:
    static void fillModel(QStandardItemModel &m, const QList<QVariant> &vals)
    {
        m.setRowCount(vals.size());
        m.setColumnCount(1);
        for (int r = 0; r < vals.size(); ++r)
            if (vals.at(r).isValid()) m.setData(m.index(r, 0), vals.at(r));
    }
    static QList<int> order(const QSortFilterProxyModel &p)
    {
        QList<int> out;
        for (int r = 0; r < p.rowCount(); ++r)
            out << p.mapToSource(p.index(r, 0)).row();
        return out;
    }
    static void addDataSets();
};

void tst_variantless::addDataSets()

{
    QTest::addColumn<QList<QVariant>>("vals");

    QList<QVariant> names;
    for (const char *s : {"b", "B", "a", "A", "\xc3\xa9", "e", "E", "z", "", "10",
                          "2", "Zebra", "apple", " apple", "DSC_0001.NEF",
                          "dsc_0001.nef", "DSC_0010.NEF", "DSC_0002.jpg"})
        names << QString::fromUtf8(s);
    names << QVariant() << QString("b") << QVariant();
    QTest::newRow("file names, dupes, unset") << names;

    QList<QVariant> ints{5, 3, QVariant(), 3, -1, 0, 1000, 5, QVariant()};
    QTest::newRow("ints (ISO, focal length)") << ints;

    QList<QVariant> longs{qlonglong(5000000000LL), qlonglong(12), qlonglong(12),
                          qlonglong(-3), QVariant()};
    QTest::newRow("byte sizes") << longs;

    QList<QVariant> dbl{2.8, 1.4, 16.0, 2.8, QVariant(), 0.0};
    QTest::newRow("apertures") << dbl;

    QList<QVariant> bools{true, false, QVariant(), true, false};
    QTest::newRow("bools (Search, Compare)") << bools;

    QList<QVariant> dates{QDate(2020, 5, 1), QDate(1999, 1, 1), QVariant(),
                          QDate(2020, 5, 1)};
    QTest::newRow("dates") << dates;

    /*  The left value's TYPE picks the branch, so a column that holds mixed types
        exercises the asymmetry Qt has and the copy must reproduce. */
    QList<QVariant> mixed{QString("5"), 3, QVariant(), 2.5, QString("abc"), 10};
    QTest::newRow("mixed types") << mixed;

    /*  Stability at volume: many equal keys, which is where an order that differs only
        in tie-breaking would show. Seeded, so a failure reproduces. */
    QRandomGenerator rng(20260924);
    QList<QVariant> many;
    for (int i = 0; i < 3000; ++i) {
        const int k = rng.bounded(40);
        many << (k == 0 ? QVariant()
                        : QVariant(QString("IMG_%1").arg(k, 4, 10, QChar('0'))
                                   + (rng.bounded(2) ? ".NEF" : ".nef")));
    }
    QTest::newRow("3000 names, heavy ties") << many;
}

void tst_variantless::comparatorAgreesPairwise_data() { addDataSets(); }
void tst_variantless::sameOrderAsQt_data() { addDataSets(); }

void tst_variantless::comparatorAgreesPairwise()
{
    QFETCH(QList<QVariant>, vals);
    if (vals.size() > 200) vals = vals.mid(0, 200);         // n^2 pairs; the ties are
                                                             // still there at 200
    QStandardItemModel m;
    fillModel(m, vals);
    BaseProxy p;
    p.setSourceModel(&m);
    for (Qt::CaseSensitivity cs : {Qt::CaseSensitive, Qt::CaseInsensitive}) {
        for (bool locale : {false, true}) {
            p.setSortCaseSensitivity(cs);
            p.setSortLocaleAware(locale);
            for (int a = 0; a < vals.size(); ++a) {
                for (int b = 0; b < vals.size(); ++b) {
                    const bool qt = p.baseLess(m.index(a, 0), m.index(b, 0));
                    /*  What SortFilter's keys hold: the data() value, which for an
                        unset cell is an INVALID QVariant, not the stored one. */
                    const QVariant ka = m.index(a, 0).data(), kb = m.index(b, 0).data();
                    const bool ours = winnowVariantLessThan(ka, kb, cs, locale);
                    if (qt != ours)
                        qDebug() << "cs" << cs << "locale" << locale << ka << kb
                                 << "qt" << qt << "ours" << ours;
                    QCOMPARE(ours, qt);
                }
            }
        }
    }
}

void tst_variantless::sameOrderAsQt()
{
    QFETCH(QList<QVariant>, vals);
    QStandardItemModel m;
    fillModel(m, vals);
    QSortFilterProxyModel stock;
    KeyedProxy keyed;
    for (int r = 0; r < vals.size(); ++r) keyed.keys << m.index(r, 0).data();
    stock.setSourceModel(&m);
    keyed.setSourceModel(&m);

    /*  The same history for both, including re-sorting an already-sorted proxy, which
        is what Winnow does all day (every filter change re-sorts the current order). */
    const struct { Qt::SortOrder order; Qt::CaseSensitivity cs; bool locale; } steps[] = {
        {Qt::AscendingOrder,  Qt::CaseSensitive,   false},
        {Qt::DescendingOrder, Qt::CaseSensitive,   false},
        {Qt::AscendingOrder,  Qt::CaseInsensitive, false},
        {Qt::DescendingOrder, Qt::CaseInsensitive, true},
        {Qt::AscendingOrder,  Qt::CaseSensitive,   true},
        {Qt::DescendingOrder, Qt::CaseSensitive,   false},
    };
    for (const auto &st : steps) {
        for (QSortFilterProxyModel *p : {static_cast<QSortFilterProxyModel *>(&stock),
                                         static_cast<QSortFilterProxyModel *>(&keyed)}) {
            p->setSortCaseSensitivity(st.cs);
            p->setSortLocaleAware(st.locale);
            p->sort(0, st.order);
            p->invalidate();                    // SortFilter::filterChange's path
        }
        const QList<int> a = order(stock), b = order(keyed);
        if (a != b) qDebug() << "step" << st.order << st.cs << st.locale
                             << "\n  qt   " << a << "\n  keyed" << b;
        QCOMPARE(b, a);
    }
}

QTEST_MAIN(tst_variantless)
#include "tst_variantless.moc"
