#ifndef PREFERENCES_H
#define PREFERENCES_H

#include <QtWidgets>
#include "Main/global.h"
#include "propertyeditor.h"
#include "propertywidgets.h"

class Preferences : public PropertyEditor
{
    Q_OBJECT
public:
    Preferences(QWidget *parent);

public slots:
    void itemChange(QModelIndex idx);

private:
    void addItems();

    /* items we will want to modify dynamically in itemChange (returned from addItem)
       ie static_cast<LabelEditor*>(availMBMsgWidget)->setValue(availMBMsg);    */
    QWidget *availMBMsgWidget;
};

#endif // PREFERENCES_H
