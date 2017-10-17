#ifndef TABLEVIEW_H
#define TABLEVIEW_H

#include <QtWidgets>
#include "global.h"
#include "thumbview.h"

class TableView : public QTableView
{
    Q_OBJECT

public:
    TableView(ThumbView *thumbView);

private:
    ThumbView *thumbView;
    void pressed(const QModelIndex &index);
};

#endif // TABLEVIEW_H
