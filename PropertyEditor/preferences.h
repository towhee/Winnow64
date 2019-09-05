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
    int captionColumnWidth;
    int valueColumnWidth;

    // items we will want to modify
    QWidget *cacheSizePercentOfAvailable;
    QWidget *cacheSizeMB;
};

#endif // PREFERENCES_H
