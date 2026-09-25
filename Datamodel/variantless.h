#ifndef VARIANTLESS_H
#define VARIANTLESS_H

#include <QVariant>
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

#endif // VARIANTLESS_H
