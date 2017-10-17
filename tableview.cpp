#include "tableview.h"

TableView::TableView(ThumbView *thumbView)
{
    this->thumbView = thumbView;
    setModel(thumbView->thumbViewFilter);
    setSortingEnabled(true);
    setAlternatingRowColors(true);
    horizontalHeader()->setStretchLastSection(true);
    horizontalHeader()->setFixedHeight(22);
    horizontalHeader()->setSortIndicatorShown(false);
    verticalHeader()->setVisible(false);
    setEditTriggers(QAbstractItemView::NoEditTriggers);
    setSelectionBehavior(QAbstractItemView::SelectRows);
    setTabKeyNavigation(false);
    setIconSize(QSize(24,24+12));   // no effect on thumbView scroll issue
    verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    verticalHeader()->setDefaultSectionSize(24);
    setSelectionModel(thumbView->thumbViewSelection);
}

void TableView::pressed(const QModelIndex &index)
{
    QTableView::pressed(index);
    qDebug() << "Table pressed at index" << index;
}
