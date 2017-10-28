#ifndef THUMBVIEWFILTER_H
#define THUMBVIEWFILTER_H

//#include <QObject>
//#include <QSortFilterProxyModel>
#include <QtWidgets>
#include "global.h"
#include "filters.h"

class ThumbViewFilter : public QSortFilterProxyModel
{
    Q_OBJECT

public:
    ThumbViewFilter();
//    ThumbViewFilter(Filters *filterView);

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;
private:
//    Filters *filterView;
};

#endif // THUMBVIEWFILTER_H
