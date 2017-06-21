#include "thumbviewfilter.h"
#include <QDebug>

ThumbViewFilter::ThumbViewFilter(QObject * parent)
{

}

//bool thumbviewfilter::filterAcceptRows(int row, const QModelIndex &index,
//                      bool filterOnPicks) const
bool ThumbViewFilter::filterAcceptRows(int row, const QModelIndex &index) const
{
    qDebug() << "ThumbViewFilter::filterAcceptRows";
    return qvariant_cast<bool>(index.data(Qt::UserRole+4));
//    if (filterOnPicks) return qvariant_cast<bool>(index.data(Qt::UserRole+4));
//    else return true;
}
