#include "thumbviewfilter.h"
#include <QDebug>

ThumbViewFilter::ThumbViewFilter(FilterView *filterView)
{
    this->filterView = filterView;
}

bool ThumbViewFilter::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const
{
    QModelIndex idxRating = sourceModel()->index(sourceRow, 8, sourceParent);
    qDebug() << "ThumbViewFilter::filterAcceptRows  Rating =" << idxRating.data(Qt::EditRole);

    return filterView->picksTrue->checkState(0) == Qt::Checked;

//    return true;  //idxRating.data(Qt::EditRole) != 2;

//    return qvariant_cast<bool>(sourceParent.data(Qt::UserRole + 4));
}
