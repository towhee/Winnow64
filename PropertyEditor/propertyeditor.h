#ifndef PROPERTYEDITOR_H
#define PROPERTYEDITOR_H

#include <QtWidgets>
#include "Main/global.h"
#include "Main/dockwidget.h"  // includes BarBtn
#include "propertydelegate.h"

/* PropertyEditor Notes -----------------------------------------------------------------------


*/

class PropertyEditor : public QTreeView
{
    Q_OBJECT
public:
    explicit PropertyEditor(QWidget *parent);
    QStandardItemModel *model;
    PropertyDelegate *propertyDelegate;
    const QStyleOptionViewItem *styleOptionViewItem;
    void setSolo(bool isSolo);
    void expandBranch(QString text);
    void resizeColumns(QString stringToFitCaptions, QString stringToFitValues);
    int indentation;
    int captionColumnWidth;
    int valueColumnWidth;
    struct ItemInfo {
        QString name;
        QString parentName;
        bool decorateGradient = false;
        bool hasValue;
        QString tooltip;
        QString captionText;
        bool captionIsEditable;
        QVariant value;
        QString valueName;
        QString type;
        int delegateType;
        QVariant min;
        QVariant max;
        int fixedWidth;
        QString color;
        QStringList dropList;
        QModelIndex index;
    };
    QWidget* addItem(ItemInfo &i); // abstract addItem
    void clearItemInfo(ItemInfo &i);
    bool getIndex(QString caption, QModelIndex parent = QModelIndex());
    QModelIndex foundIdx;

    QMap<QString, QModelIndex> sourceIdx;

protected:
    void mousePressEvent(QMouseEvent *event) override;

public slots:
    void editorWidgetToDisplay(QModelIndex idx, QWidget *editor);
    virtual void itemChange(QModelIndex);
    virtual void selectionChange(const QItemSelection &selected, const QItemSelection &deselected);

private:
    QLinearGradient categoryBackground;
    bool isSolo;
};


#endif // PROPERTYEDITOR_H
