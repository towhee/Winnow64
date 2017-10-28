#ifndef THUMBVIEWFILTER_H
#define THUMBVIEWFILTER_H

#include <QSortFilterProxyModel>
//#include <QtWidgets>
#include <QDebug>
#include "global.h"
#include "filters.h"

class ThumbViewFilter : public QSortFilterProxyModel
{
    Q_OBJECT

public:
    ThumbViewFilter(QObject *parent, Filters *filters);
//    ThumbViewFilter(Filters *filterView);

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;
private:
    Filters *filters;
};

#endif // THUMBVIEWFILTER_H
