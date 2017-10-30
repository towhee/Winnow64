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
    bool isMatch = true;                   // overall match
    bool isCategoryUnchecked = true;
    QTreeWidgetItemIterator it(filters);
    while (*it) {
        if ((*it)->parent()) {
            /* There is a parent therefore not a top level item so this is one
            of the items to match ie rating = one. If the item has been checked
            then compare the checked filter item to the data in the
            dataModelColumn for the row. If it matches then set isMatch = true.
            If it does not match them isMatch is still false but the row could
            still be accepted if another item in the same category does match.
            */
//            QString itemName = (*it)->text(0);      // for debugging

            if ((*it)->checkState(0) != Qt::Unchecked) {
                isCategoryUnchecked = false;
                QModelIndex idx = sourceModel()->index(sourceRow, dataModelColumn, sourceParent);
                QVariant dataValue = idx.data(Qt::EditRole);
                QVariant filterValue = (*it)->data(1, Qt::EditRole);

//                qDebug() << itemCategory << itemName
//                         << "Comparing" << dataValue << filterValue << (dataValue == filterValue);

                if (dataValue == filterValue) isMatch = true;
            }
        }
        else {
            // top level item = category
            // check results of category items filter match
            if (isCategoryUnchecked) isMatch = true;
//            qDebug() << "Category" << itemCategory << isMatch;
            if (!isMatch) return false;   // no match in category

            /* prepare for category items filter match.  If no item is checked
            or one checked item matches the data then the row is okay to show
            */
            // the top level items contain reference to the data model column
            dataModelColumn = (*it)->data(0, G::ColumnRole).toInt();
            isCategoryUnchecked = true;
            isMatch = false;
//            itemCategory = (*it)->text(0);      // for debugging
        }
        ++it;
    }
    // check results of category items filter match for the last group
    if (isCategoryUnchecked) isMatch = true;
//    qDebug() << "After iteration  isMatch =" << isMatch;

    return isMatch;
}

void ThumbViewFilter::filterChanged(QTreeWidgetItem* x, int col)
{
    this->invalidateFilter();
}
