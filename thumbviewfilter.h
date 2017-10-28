#ifndef THUMBVIEWFILTER_H
#define THUMBVIEWFILTER_H

#include <QObject>
#include <QSortFilterProxyModel>
#include "filterview.h"

class ThumbViewFilter : public QSortFilterProxyModel
{
    Q_OBJECT

public:
    ThumbViewFilter(FilterView *filterView, QObject *parent = 0);

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;
private:
    FilterView *filterView;
};

#endif // THUMBVIEWFILTER_H
