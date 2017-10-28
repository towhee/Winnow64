#include "thumbviewfilter.h"

ThumbViewFilter::ThumbViewFilter()
{
//    this->filterView = filterView;
}

bool ThumbViewFilter::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const
{
    QModelIndex idxRating = sourceModel()->index(sourceRow, G::RatingColumn, sourceParent);
    QModelIndex idxLabel = sourceModel()->index(sourceRow, G::LabelColumn, sourceParent);
    QModelIndex idxPick = sourceModel()->index(sourceRow, G::PickedColumn, sourceParent);
    QModelIndex idxType = sourceModel()->index(sourceRow, G::TypeColumn, sourceParent);
    QModelIndex idxModel = sourceModel()->index(sourceRow, G::CameraModelColumn, sourceParent);
    qDebug() << "ThumbViewFilter::filterAcceptRows  Rating =" << idxRating.data(Qt::EditRole);

//    return filterView->picksTrue->checkState(0) == Qt::Checked;

//    return true;  //idxRating.data(Qt::EditRole) != 2;

//    return qvariant_cast<bool>(sourceParent.data(G::PickedRole));
}
