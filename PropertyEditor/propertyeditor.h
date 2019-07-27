#ifndef PROPERTYEDITOR_H
#define PROPERTYEDITOR_H

#include <QtWidgets>
#include "Main/global.h"
#include "propertydelegate.h"

class PropertyEditor : public QTreeView
{
    Q_OBJECT
public:
    explicit PropertyEditor(QWidget *parent);
    QStandardItemModel *model;
    PropertyDelegate *propertyDelegate;
    const QStyleOptionViewItem *styleOptionViewItem;
    int indentation;
    void setSolo(bool isSolo);

protected:
    void mousePressEvent(QMouseEvent *event);

public slots:
    void editorWidgetToDisplay(QModelIndex idx, QWidget *editor);

private:
    QLinearGradient categoryBackground;
    bool isSolo;
};


#endif // PROPERTYEDITOR_H
