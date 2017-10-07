#ifndef TABLEVIEW_H
#define TABLEVIEW_H

#include <QDialog>
#include "thumbview.h"

namespace Ui {
class TableView;
}

class TableView : public QDialog
{
    Q_OBJECT

public:
    explicit TableView(ThumbView *thumbView, QWidget *parent = 0);
    ~TableView();

private:
    Ui::TableView *ui;
};

#endif // TABLEVIEW_H
