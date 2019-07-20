#ifndef PROPERTYEDITOR_H
#define PROPERTYEDITOR_H

#include <QtWidgets>
#include "Main/global.h"

#include "propertysliderwidget.h"

class PropertyEditor : public QTreeWidget
{
    Q_OBJECT
public:
    explicit PropertyEditor(QWidget *parent = nullptr);

    QTreeWidgetItem *general;
    QTreeWidgetItem *generalFontSize;

signals:

public slots:
    void editCheck(QTreeWidgetItem *item, int column);
};

#endif // PROPERTYEDITOR_H
