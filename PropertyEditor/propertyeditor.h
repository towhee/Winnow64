#ifndef PROPERTYEDITOR_H
#define PROPERTYEDITOR_H

#include <QtWidgets>
#include "Main/global.h"
#include "Main/dockwidget.h"  // includes BarBtn
#include "propertydelegate.h"
#include "PropertyEditor/propertywidgets.h"


/* PropertyEditor Notes -----------------------------------------------------------------------


*/

class PropertyModel : public QStandardItemModel
{
    Q_OBJECT
public:
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const;
};

class PropertyEditor : public QTreeView
{
    Q_OBJECT
public:
    explicit PropertyEditor(QWidget *parent);
    QStandardItemModel *model;
    PropertyDelegate *propertyDelegate;
//    SortProperties *proxy;
    const QStyleOptionViewItem *styleOptionViewItem;
    void setSolo(bool isSolo);
    void expandBranch(QString text);
    void resizeColumns(QString stringToFitCaptions, QString stringToFitValues);
    int indentation;
    int captionColumnWidth;
    int valueColumnWidth;
    bool isExpandRecursively = true;
    QModelIndex capIdx; // used to get index for added item when subclassing addItem
    QModelIndex valIdx; // used to hide/show rows etc
    QModelIndex ordIdx; // used to order rows etc

    struct ItemInfo {
        QString name;
        int sortOrder;
        QString parentName;
        QString path;                   // the path in settings
        bool isHeader = false;
        bool isIndent = true;
        bool decorateGradient = false;
        bool hasValue;
        QString tooltip;
        QString captionText;
        bool isDecoration = true;       // show collapse/expand decoration in header
        bool captionIsEditable;
        QVariant defaultValue;          // when created and when double click caption
        QVariant value;
        QString key;
        QString type;
        int delegateType;
        QVariant min;
        QVariant max;
        QVariant div;                   // used to convert slider int amount to double ie / 100
        int fixedWidth;
        QString color;
        QStringList dropList;
        QStringList dropIconList;       // file location of icons
        QModelIndex parIdx;             // datamodel parent index for the item
        QModelIndex index;
        int itemIndex;                  // index for item (use when many items ie borders)
        int assocIndex;                 // used for anchorObject
    };
    QWidget* addItem(ItemInfo &i); // abstract addItem
    void copyTemplate(QString name);
    QVariant getItemValue(QString name, QModelIndex parent);
    void setItemValue(QModelIndex idx, int type, QVariant value);
    void getItemInfo(QModelIndex &idx, ItemInfo &copy);
    void clearItemInfo(ItemInfo &i);
    void getIndexFromNameAndParent(QString name, QString parName, QModelIndex = QModelIndex());
    bool getIndex(QString caption, QModelIndex parent = QModelIndex());
    QModelIndex findIndex(QString name);
    int uniqueItemIndex(QModelIndex parentIdx = QModelIndex());
    QModelIndex getItemIndex(int itemIndex, QModelIndex parentIdx = QModelIndex());
    void updateHiddenRows(QModelIndex parent);
    QModelIndex foundIdx;
    void diagnosticProperties(QModelIndex parent);

    QMap<QString, QModelIndex> sourceIdx;

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

public slots:
    void editorWidgetToDisplay(QModelIndex idx, QWidget *editor);
    virtual void itemChange(QModelIndex);
//    /*virtual*/ void selectionChanged(const QItemSelection &selected, const QItemSelection &deselected) override;

private:
    int itemIndex = -1;
    QLinearGradient categoryBackground;
    bool isSolo;
};


#endif // PROPERTYEDITOR_H
