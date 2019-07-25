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

protected:
    void mousePressEvent(QMouseEvent *event);
//    void drawRow(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const;

signals:
//    void editorValueChanged(QVariant v, QString source, QModelIndex index);
//    void itemChanged(QStandardItem *item);

public slots:
//    void editorValueChange(QVariant v, QString source, QModelIndex index);
//    void itemChange(QStandardItem *item);
    void editorWidgetToDisplay(QModelIndex idx, QWidget *editor);

//    void editCheck(QTreeWidgetItem *item, int column);
private:
};


#endif // PROPERTYEDITOR_H
