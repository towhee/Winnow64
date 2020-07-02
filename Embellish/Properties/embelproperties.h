#ifndef EMBELPROPERTIES_H
#define EMBELPROPERTIES_H

#include <QtWidgets>
#include "Main/global.h"
#include "PropertyEditor/propertyeditor.h"
#include "PropertyEditor/propertywidgets.h"

class EmbelProperties : public PropertyEditor
{
    Q_OBJECT
public:
    EmbelProperties(QWidget *parent);

public slots:
    void itemChange(QModelIndex idx);

private:
    void addItems();
};

#endif // EMBELPROPERTIES_H
