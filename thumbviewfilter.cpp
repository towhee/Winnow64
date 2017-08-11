#include "thumbviewfilter.h"
#include <QDebug>

ThumbViewFilter::ThumbViewFilter(QObject * parent)
{

}

bool ThumbViewFilter::filterAcceptRows(const QModelIndex &index) const
{
    qDebug() << "ThumbViewFilter::filterAcceptRows";
    return qvariant_cast<bool>(index.data(Qt::UserRole+4));
}
