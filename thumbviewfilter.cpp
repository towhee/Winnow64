#include "thumbviewfilter.h"

ThumbViewFilter::ThumbViewFilter(QObject *parent, Filters *filters) : QSortFilterProxyModel(parent)
{
    this->filters = filters;
}

bool ThumbViewFilter::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const
{
//    return true;
    static int counter = 0;
    counter++;
//    qDebug() << "Filtering" << counter;
    QString itemCategory = "Just starting";

    int dataModelColumn;
    bool isMatch;                   // overall match
    bool isCategoryUnchecked = true;
    QTreeWidgetItemIterator it(filters);
    while (*it) {
        if ((*it)->parent()) {
            // item to compare
            // debug info
            QString itemName = (*it)->text(0);

            if ((*it)->checkState(0) != Qt::Unchecked) {
                isCategoryUnchecked = false;
                QModelIndex idx = sourceModel()->index(sourceRow, dataModelColumn, sourceParent);
                QVariant dataValue = idx.data(Qt::EditRole);
                QVariant filterValue = (*it)->data(1, Qt::EditRole);

                qDebug() << itemCategory << itemName
                         << "Comparing" << dataValue << filterValue << (dataValue == filterValue);

                if (dataValue == filterValue) isMatch = true;
            }
        }
        else {
            // top level item = category
            // check results of category items filter match
            if (isCategoryUnchecked) isMatch = true;
            if (!isMatch) return isMatch;   // no match in category

            // prepare for category items filter match
            dataModelColumn = (*it)->data(0, G::ColumnRole).toInt();
            isCategoryUnchecked = true;
            isMatch = false;
            itemCategory = (*it)->text(0);
        }
        ++it;
    }

    return true;

//    QModelIndex idxRating = sourceModel()->index(sourceRow, G::RatingColumn, sourceParent);
//    QModelIndex idxLabel = sourceModel()->index(sourceRow, G::LabelColumn, sourceParent);
//    QModelIndex idxPick = sourceModel()->index(sourceRow, G::PickedColumn, sourceParent);
//    QModelIndex idxType = sourceModel()->index(sourceRow, G::TypeColumn, sourceParent);
//    QModelIndex idxModel = sourceModel()->index(sourceRow, G::CameraModelColumn, sourceParent);

//    int rating = idxRating.data(Qt::EditRole).toInt();
//    bool isRating = false;

//    foreach (QStandardItem item, filters->ratings) {
//        qDebug() << "filter item  ischecked" << item.checkState() << "Value" << item.data(Qt::EditRole);
//        if (item.checkState()) {
//            if(item.data(Qt::EditRole) == rating) isRating = true;
//            if (isRating) break;
//        }
//    }
//    isMatch = isRating;

//    return isMatch;

//    qDebug() << "ThumbViewFilter::filterAcceptRows  Rating =" << idxModel.data(Qt::EditRole);

//    return filters->picksFalse->checkState(0) == Qt::Checked && idxLabel.data(Qt::EditRole) == "False";

//    return idxRating.data(Qt::EditRole) != 2;

//    return qvariant_cast<bool>(sourceParent.data(G::PickedRole));

    return true;
}

void ThumbViewFilter::filterChanged(QTreeWidgetItem* x, int col)
{
    this->invalidateFilter();
}
