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
    void editorValueChange(QVariant v, QString source);

private:
    void addItems();
};

#endif // PREFERENCES_H
