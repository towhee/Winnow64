#ifndef PROPERTYEDITOR_H
#define PROPERTYEDITOR_H

#include <QtWidgets>
#include "Main/dockwidget.h"  // includes BarBtn
#include "propertydelegate.h"
#include "PropertyEditor/propertywidgets.h"


/* PropertyEditor Notes -----------------------------------------------------------------------


*/

class PropertyModel : public QStandardItemModel
{
    Q_OBJECT
public:
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
};

class PropertyEditor : public QTreeView
{
    Q_OBJECT
public:
    explicit PropertyEditor(QWidget *parent);
    ~PropertyEditor() override;
    QStandardItemModel *model;
    PropertyDelegate *propertyDelegate;
//    SortProperties *proxy;
    const QStyleOptionViewItem *styleOptionViewItem;
    void setSolo(bool isSolo);
    void setExpandRecursively(bool isExpandRecursively);
    void expandBranch(QString text);
    int indentation;
    QString stringToFitCaptions;
    QString stringToFitValues;
    int width;
    int captionColumnWidth;
    int valueColumnWidth;
    bool ignoreFontSizeChangeSignals = false;
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
        bool okToCollapseRoot = true;   // force root stay expanded (ie Templates in EmbelProperties)
        bool isDecoration = true;       // show collapse/expand decoration in header
        bool captionIsEditable;
        QVariant defaultValue;          // when created and when double click caption
        QVariant value;
        QString key;
        QString type;
        int delegateType;
        int min;
        int max;
        int div;                        // used to convert slider int amount to double ie / 100
        int step;                      // amount to adjust slider one pixel
        int fixedWidth;                 // width of control (ie lineedit in slider)
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
    bool setItemValue(QString name, QVariant val);
    void setItemValue(QModelIndex idx, QVariant value);
    void getItemInfo(QModelIndex &idx, ItemInfo &copy);
    void clearItemInfo(ItemInfo &i);
    void setItemEnabled(QString name, bool state);
    void getIndexFromNameAndParent(QString name, QString parName, QModelIndex = QModelIndex());
    bool getIndex(QString caption, QModelIndex parent = QModelIndex());
    QModelIndex findCaptionIndex(QString name);
    QModelIndex findValueIndex(QString name);
    int uniqueItemIndex(QModelIndex parentIdx = QModelIndex());
    QModelIndex getItemIndex(int itemIndex, QModelIndex parentIdx = QModelIndex());
    void updateHiddenRows(QModelIndex parent);
    QModelIndex foundIdx;
    void close(QModelIndex parent);
    void diagnosticProperties(QModelIndex parent);

    QMap<QString, QModelIndex> sourceIdx;

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;

signals:
    void fontSizeChange(int fontSize);

public slots:
    void editorWidgetToDisplay(QModelIndex idx, QWidget *editor);
    virtual void itemChange(QModelIndex);
    void fontSizeChanged(int fontSize);
    void resizeColumns(/*QString stringToFitCaptions, QString stringToFitValues*/);
//    /*virtual*/ void selectionChanged(const QItemSelection &selected, const QItemSelection &deselected) override;

private:
    void collapseAllExcept();
    int itemIndex = -1;
    QLinearGradient categoryBackground;
    bool isSolo;
    bool isExpandRecursively;
};


#endif // PROPERTYEDITOR_H
