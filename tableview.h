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

protected:
    void mouseDoubleClickEvent(QMouseEvent *event);

private:
    ThumbView *thumbView;

private slots:
    void delaySelectCurrentThumb();

signals:
    void displayLoupe();
};

#endif // TABLEVIEW_H
