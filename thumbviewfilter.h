#ifndef THUMBVIEWFILTER_H
#define THUMBVIEWFILTER_H

#include <QObject>
#include <QSortFilterProxyModel>

class ThumbViewFilter : public QSortFilterProxyModel
{
    Q_OBJECT

public:
    explicit ThumbViewFilter(QObject * parent = 0);
protected:
    bool filterAcceptRows(const QModelIndex &index) const;
};

#endif // THUMBVIEWFILTER_H
