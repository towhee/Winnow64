#include "thumbviewfilter.h"

ThumbViewFilter::ThumbViewFilter(QObject *parent, Filters *filters) : QSortFilterProxyModel(parent)
{
    this->filters = filters;
}

bool ThumbViewFilter::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const
{
    QModelIndex idxRating = sourceModel()->index(sourceRow, G::RatingColumn, sourceParent);
    QModelIndex idxLabel = sourceModel()->index(sourceRow, G::LabelColumn, sourceParent);
    QModelIndex idxPick = sourceModel()->index(sourceRow, G::PickedColumn, sourceParent);
    QModelIndex idxType = sourceModel()->index(sourceRow, G::TypeColumn, sourceParent);
    QModelIndex idxModel = sourceModel()->index(sourceRow, G::CameraModelColumn, sourceParent);
//    qDebug() << "ThumbViewFilter::filterAcceptRows  Rating =" << idxModel.data(Qt::EditRole);

//    return filters->picksFalse->checkState(0) == Qt::Checked && idxLabel.data(Qt::EditRole) == "False";

    return idxRating.data(Qt::EditRole) != 2;

//    return qvariant_cast<bool>(sourceParent.data(G::PickedRole));

//    return true;
}
