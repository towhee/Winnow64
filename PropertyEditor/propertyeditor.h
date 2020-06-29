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
    bool getParent(QString caption, QModelIndex parent = QModelIndex());
    void setSolo(bool isSolo);
    int indentation;
    int captionColumnWidth;
    int valueColumnWidth;
    struct ItemInfo {   // abstract addItem
        QString name;
        QString parentName;
        bool hasValue;
        QString tooltip;
        QString captionText;
        bool captionIsEditable;
        QVariant value;
        QString valueName;
        QString type;
        int delegateType;
        int min;
        int max;
        int fixedWidth;
        QString color;
        QStringList dropList;
        QModelIndex index;
    };
    void addItem(ItemInfo &i); // abstract addItem

protected:
    void mousePressEvent(QMouseEvent *event);

public slots:
    void editorWidgetToDisplay(QModelIndex idx, QWidget *editor);

private:
//    QModelIndex getParent(QString caption, QModelIndex parent);
    QLinearGradient categoryBackground;
    bool isSolo;
    QModelIndex parIdx;
};


#endif // PROPERTYEDITOR_H
