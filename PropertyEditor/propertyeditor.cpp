#include "propertyeditor.h"
#include "Main/mainwindow.h"
#include "Main/global.h"
#include <QDebug>

/*
The property editor has four components:

PropertyEditor - A subclass of QTreeView, with a data model that holds the property
    information.

PropertyDelegate - Creates the editor widgets, sets the editor and model data and does some
    custom painting to each row in the treeview.

PropertyWidgets - A collection of custom widget editors (slider, combobox etc) that are
    inserted into column 1 in the treeview (PropertyEditor) by PropertyDelegate.  The editors
    are built based on information passed in the datamodel UserRole such as the DelegateType
    (Spinbox, Checkbox, Combo, Slider, PlusMinusBtns etc).  Other information includes the
    source, datamodel index, mim value, max value, a stringlist for combos etc).

PropertyEditor subclass ie Preferences.  All the property items are defined and added to the
    treeview in addItems().  When an item value has been changed in an editor itemChanged(idx)
    is signaled and the variable is updated in Winnow and any necessary actions are executed.
    For example, if the thumbnail size is increased then the IconView function to do this is
    called.
*/

PropertyEditor::PropertyEditor(QWidget *parent) : QTreeView(parent)
{

    setRootIsDecorated(true);
    setAlternatingRowColors(true);
    setSelectionBehavior(QAbstractItemView::SelectRows);
    setEditTriggers(QAbstractItemView::AllEditTriggers);
    indentation = 15;
    setIndentation(indentation);  

    model = new QStandardItemModel;
    setModel(model);
    // abstract
    model->setColumnCount(2);
//    model->setHorizontalHeaderItem(0, new QStandardItem(QString("Field")));
//    model->setHorizontalHeaderItem(1, new QStandardItem(QString("Value")));

    propertyDelegate = new PropertyDelegate(this);
    setItemDelegate(propertyDelegate);
    styleOptionViewItem = new QStyleOptionViewItem;

    connect(propertyDelegate, &PropertyDelegate::editorWidgetToDisplay,
            this, &PropertyEditor::editorWidgetToDisplay);

    connect(propertyDelegate, &PropertyDelegate::drawBranchesAgain, this, &PropertyEditor::drawBranches);
}

void PropertyEditor::editorWidgetToDisplay(QModelIndex idx, QWidget *editor)
/*
Sets the custom editor widget for the value column (column 1).  The
*/
{
    setIndexWidget(idx, editor);
    emit propertyDelegate->closeEditor(editor);
}

//QStandardItem* PropertyEditor::getParent(QString caption, QModelIndex parent)
bool PropertyEditor::getParent(QString caption, QModelIndex parent)
{
    for(int r = 0; r < model->rowCount(parent); ++r) {
        QModelIndex idx = model->index(r, 0, parent);
        QString text = model->data(idx, UR_Name).toString();
        bool isMatch = text == caption;
        if (isMatch) {
            parIdx = idx;
            return true;
        }
        // iterate children
        if (model->hasChildren(idx)) {
            getParent(caption, idx);
        }
    }
    return false;
}

// abstract addItem
void PropertyEditor::addItem(ItemInfo &i)
{
    int row;
    QModelIndex capIdx;
    QModelIndex valIdx;
    parIdx = QModelIndex();
    QStandardItem *catItem = new QStandardItem;
    QStandardItem *valItem = new QStandardItem;
    QStandardItem *parItem = new QStandardItem;
    parItem = nullptr;
    /*
    QList<QStandardItem *>search;
    if (model->rowCount()) {
        search = model->findItems(i.parentName);
        qDebug() << "Item =" << i.captionText
                 << "Searching for =" << i.parentName
                 << "search =" << search
                 << "search length =" << search.length();
        if (search.length()) {
            parItem = search.at(0);
            parIdx = parItem->index();
        }
    } */
    getParent(i.parentName);
    parItem = model->itemFromIndex(parIdx);

    if (parItem == nullptr) {
        // First item = root
        row = model->rowCount();
        if (row == 0) model->insertRow(0, QModelIndex());
        else model->appendRow(catItem);
        capIdx = model->index(row, 0, QModelIndex());
        valIdx = model->index(row, 1, QModelIndex());
    }
    else {
    // row for next child to add
        row = parItem->rowCount();
        parItem->setChild(row, 0, catItem);
        parItem->setChild(row, 1, valItem);
        capIdx = catItem->index();
        valIdx = valItem->index();
    }

    // caption
    model->setData(capIdx, i.captionText);
    model->setData(capIdx, i.name, UR_Name);
    model->setData(capIdx, i.captionText);
    model->setData(capIdx, i.tooltip, Qt::ToolTipRole);
    model->setData(valIdx, i.tooltip, Qt::ToolTipRole);

    // if no value associated (header item or spacer etc) then we are done
    if (!i.hasValue) return;

    // value
    model->setData(valIdx, i.value, Qt::EditRole);
    model->setData(valIdx, i.delegateType, UR_DelegateType);
    model->setData(valIdx, i.valueName, UR_Source);
    model->setData(valIdx, i.type, UR_Type);
    model->setData(valIdx, i.min, UR_Min);
    model->setData(valIdx, i.max, UR_Max);
    model->setData(valIdx, i.fixedWidth, UR_LabelFixedWidth);
    model->setData(valIdx, i.color, UR_Color);
    model->setData(valIdx, i.dropList, UR_StringList);
    model->setData(valIdx, i.index, UR_QModelIndex);

    propertyDelegate->createEditor(this, *styleOptionViewItem, valIdx);
}

void PropertyEditor::mousePressEvent(QMouseEvent *event)
/*
Set the current index and expand/collapse when click anywhere on a row that has children.
Do not pass on to QTreeView as this will enable QTreeView expanding and collapsing when the
decoration is clicked.
*/
{
    // ignore right mouse clicks for context menu
    if (event->button() == Qt::RightButton) return;

    QModelIndex idx = indexAt(event->pos());
    if (idx.column() != 0) idx = model->index(idx.row(), 0, idx.parent());
    QStandardItem *item = model->itemFromIndex(idx);
    if (idx.isValid() && item->hasChildren()) {
        bool isRoot = idx.parent() == QModelIndex();
        bool wasExpanded = isExpanded(idx);
        if (isRoot) {
            if (isSolo) collapseAll();
            wasExpanded ? collapse(idx) : expandRecursively(idx);
        }
        else {
            isExpanded(idx) ? collapse(idx) : expand(idx);
        }
    }
}


void PropertyEditor::setSolo(bool isSolo)
{
    this->isSolo = isSolo;
}
