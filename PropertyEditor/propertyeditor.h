#ifndef PROPERTYEDITOR_H
#define PROPERTYEDITOR_H

#include <QtWidgets>
#include "Main/global.h"
#include "Main/dockwidget.h"  // includes BarBtn
#include "propertydelegate.h"
#include "PropertyEditor/propertywidgets.h"

class SortProperties : public QSortFilterProxyModel
{
    Q_OBJECT

public:
    SortProperties(QObject *parent);

public slots:
    void sortChange();

protected:
    bool lessThan(const QModelIndex &left, const QModelIndex &right) const;
};

/* PropertyEditor Notes -----------------------------------------------------------------------


*/

class PropertyEditor : public QTreeView
{
    Q_OBJECT
public:
    explicit PropertyEditor(QWidget *parent);
//    QAbstractItemModel *model;
    QStandardItemModel *model;
    PropertyDelegate *propertyDelegate;
    SortProperties *proxy;
    const QStyleOptionViewItem *styleOptionViewItem;
    void setSolo(bool isSolo);
    void expandBranch(QString text);
    void resizeColumns(QString stringToFitCaptions, QString stringToFitValues);
    int indentation;
    int captionColumnWidth;
    int valueColumnWidth;
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
        QVariant value;
        QString key;
        QString type;
        int delegateType;
        QVariant min;
        QVariant max;
        int fixedWidth;
        QString color;
        QStringList dropList;
        QModelIndex parIdx;             // datamodel parent index for the item
        QModelIndex index;              // index for
        int insertRow;
        int itemIndex;
    };
    QWidget* addItem(ItemInfo &i); // abstract addItem
    void getItemInfo(QModelIndex &idx, ItemInfo &copy);
    void clearItemInfo(ItemInfo &i);
    void getIndexFromNameAndParent(QString name, QString parName, QModelIndex = QModelIndex());
    bool getIndex(QString caption, QModelIndex parent = QModelIndex());
    QModelIndex findIndex(QString name);
    void updateHiddenRows(QModelIndex parent);
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
