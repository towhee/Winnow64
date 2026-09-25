#ifndef VARIANTLESS_H
#define VARIANTLESS_H

#include <QVariant>
#include <QVector>
#include <algorithm>
#include <QString>
#include <QDate>
#include <QTime>
#include <QDateTime>

/*
    QAbstractItemModelPrivate::isVariantLessThan, Qt 6.11
    (Src/qtbase/src/corelib/itemmodels/qabstractitemmodel.cpp:760), the comparison
    QSortFilterProxyModel::lessThan applies to two data() values. It is private API, so
    it is copied rather than called: SortFilter::lessThan uses this on keys taken in one
    pass, and the proxy's order is only unchanged if the two agree exactly. If Qt changes
    theirs, change this -- tst_variantless compares the two orders directly, and
    --perfprobe prints "[PERF] sort check" after every keyed sort.
*/
inline bool winnowVariantLessThan(const QVariant &left, const QVariant &right,
                                  Qt::CaseSensitivity cs, bool isLocaleAware)
{
    if (left.userType() == QMetaType::UnknownType)
        return false;
    if (right.userType() == QMetaType::UnknownType)
        return true;
    switch (left.userType()) {
    case QMetaType::Int:        return left.toInt() < right.toInt();
    case QMetaType::UInt:       return left.toUInt() < right.toUInt();
    case QMetaType::LongLong:   return left.toLongLong() < right.toLongLong();
    case QMetaType::ULongLong:  return left.toULongLong() < right.toULongLong();
    case QMetaType::Float:      return left.toFloat() < right.toFloat();
    case QMetaType::Double:     return left.toDouble() < right.toDouble();
    case QMetaType::QChar:      return left.toChar() < right.toChar();
    case QMetaType::QDate:      return left.toDate() < right.toDate();
    case QMetaType::QTime:      return left.toTime() < right.toTime();
    case QMetaType::QDateTime:  return left.toDateTime() < right.toDateTime();
    case QMetaType::QString:
    default:
        if (isLocaleAware)
            return left.toString().localeAwareCompare(right.toString()) < 0;
        else
            return left.toString().compare(right.toString(), cs) < 0;
    }
}

/*
    SORT RANKS: each key's position in the sorted order, with EQUAL keys given EQUAL
    ranks, so that rank[a] < rank[b] exactly when winnowVariantLessThan(key[a], key[b]).
    A stable sort then produces the same order comparing ranks as comparing keys -- the
    comparator's answers are identical -- whatever order it starts from, and comparing two
    ints is far cheaper than two QVariant string conversions and a compare (~58 ms of a
    148,567-row filter clear, sampled). SortFilter caches the ranks per sort column and
    reuses them until that column's data changes.

    ONLY FOR A WELL-BEHAVED KEY SET. The equivalence needs a strict weak ordering, which
    isVariantLessThan is for keys of one type plus unset ones (unset sorts last and equal
    to itself) but is NOT guaranteed to be across mixed types, where the left key's type
    picks the comparison. So this returns false -- and the caller keeps comparing keys --
    unless every valid key has the same type.
*/
inline bool winnowSortRanks(const QVector<QVariant> &keys, Qt::CaseSensitivity cs,
                            bool isLocaleAware, QVector<int> &ranks)
{
    int type = QMetaType::UnknownType;
    for (const QVariant &k : keys) {
        const int t = k.userType();
        if (t == QMetaType::UnknownType) continue;
        if (type == QMetaType::UnknownType) type = t;
        else if (t != type) return false;
    }
    const int n = keys.size();
    QVector<int> order(n);
    for (int i = 0; i < n; ++i) order[i] = i;
    auto less = [&](int a, int b) {
        return winnowVariantLessThan(keys.at(a), keys.at(b), cs, isLocaleAware);
    };
    std::stable_sort(order.begin(), order.end(), less);
    ranks.resize(n);
    for (int i = 0; i < n; ++i) {
        const int cur = order.at(i);
        ranks[cur] = (i > 0 && !less(order.at(i - 1), cur)) ? ranks.at(order.at(i - 1)) : i;
    }
    return true;
}

#endif // VARIANTLESS_H
