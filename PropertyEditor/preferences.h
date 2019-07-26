#ifndef PREFERENCES_H
#define PREFERENCES_H

#include <QtWidgets>
#include "Main/global.h"
#include "propertyeditor.h"

class Preferences : public PropertyEditor
{
    Q_OBJECT
public:
    Preferences(QWidget *parent);

public slots:
    void itemChange(QModelIndex idx);

private:
    void addItems();
    int captionColumnWidth;
    int valueColumnWidth;
};

#endif // PREFERENCES_H
