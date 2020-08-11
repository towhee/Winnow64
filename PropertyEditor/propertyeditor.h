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
    QModelIndex capIdx; // used to get index for added item when subclassing addItem
    struct ItemInfo {
        QString name;
        QString parentName;
        bool isHeader = false;
        bool isIndent = true;
        bool decorateGradient = false;
        bool hasValue;
        QString tooltip;
        QString captionText;
        bool isDecoration = true;       // show collapse/expand decoration in header
        bool captionIsEditable;
        QVariant value;
        QString key;
        QString type;
        int delegateType;
        QVariant min;
        QVariant max;
        int fixedWidth;
        QString color;
        QStringList dropList;
        QModelIndex index;
        int itemIndex;
    };
    QWidget* addItem(ItemInfo &i); // abstract addItem
    void clearItemInfo(ItemInfo &i);
    bool getIndex(QString caption, QModelIndex parent = QModelIndex());
    QModelIndex foundIdx;

    QMap<QString, QModelIndex> sourceIdx;

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

public slots:
    void editorWidgetToDisplay(QModelIndex idx, QWidget *editor);
    virtual void itemChange(QModelIndex);
//    /*virtual*/ void selectionChanged(const QItemSelection &selected, const QItemSelection &deselected) override;

private:
    QLinearGradient categoryBackground;
    bool isSolo;
};


#endif // PROPERTYEDITOR_H
