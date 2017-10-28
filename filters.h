#ifndef FILTERS_H
#define FILTERS_H

#include <QtWidgets>
#include "global.h"

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

#endif // FILTERS_H
