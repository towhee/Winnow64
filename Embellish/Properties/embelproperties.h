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
    EmbelProperties(QWidget *parent, QSettings *setting);
    struct Border {
        QString name;
        QString caption;
        QString parent;
        int top;
        int left;
        int right;
        int bottom;
        QString tile;
        QString color;
        int opacity;
        QString style;
    } border;
    QVector<Border>b;

public slots:
    void itemChange(QModelIndex idx);

private:
    void readEmbel();           // template list, tile list, style list
    void readTemplate();
    void writeTemplate();
    void addBorders();
    void newBorder();
    void addBorder();
    void addItems();

    ItemInfo i;
};

#endif // EMBELPROPERTIES_H
