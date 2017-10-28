#ifndef FILTERVIEW_H
#define FILTERVIEW_H

#include <QtWidgets>
#include "global.h"
#include "thumbview.h"

class Filters : public QTreeWidget
{
    Q_OBJECT
public:
    Filters(QWidget *parent);
    QTreeWidgetItem *picksFalse;
    QTreeWidgetItem *picksTrue;
signals:

public slots:

private:
};

#endif // FILTERVIEW_H
