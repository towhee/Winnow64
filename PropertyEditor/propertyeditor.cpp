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
//    QHeaderView *hdr = new QHeaderView(Qt::Horizontal);
//    setHeader(hdr);
//    hdr->setVisible(true);

    propertyDelegate = new PropertyDelegate(this);
    setItemDelegate(propertyDelegate);
    styleOptionViewItem = new QStyleOptionViewItem;

    connect(this->propertyDelegate, &PropertyDelegate::itemChanged,
            this, &PropertyEditor::itemChange);

    connect(propertyDelegate, &PropertyDelegate::editorWidgetToDisplay,
            this, &PropertyEditor::editorWidgetToDisplay);

    connect(propertyDelegate, &PropertyDelegate::drawBranchesAgain, this, &PropertyEditor::drawBranches);

//    connect(model, &QStandardItemModel::dataChanged, this, &PropertyEditor::)
//    connect(this, &QTreeView::selectionChanged,
}

void PropertyEditor::editorWidgetToDisplay(QModelIndex idx, QWidget *editor)
/*
Sets the custom editor widget for the value column (column 1).
*/
{
    setIndexWidget(idx, editor);
    emit propertyDelegate->closeEditor(editor);
}

void PropertyEditor::itemChange(QModelIndex)
// virtual function to be subclassed
{

}

void PropertyEditor::selectionChange(const QItemSelection &selected, const QItemSelection &deselected)
{
//    qDebug() << __FUNCTION__ << selected << deselected;
}

/*QVariant PropertyEditor::getValue(QString name)
{
    getIndex(name);
    QModelIndex valIdx = model->index(foundCatIdx.row(), 1, foundCatIdx.parent());
    qDebug() << __FUNCTION__
             << "name =" << foundCatIdx.data(UR_Name).toString()
             << "foundCatIdx ="  << foundCatIdx
             << "foundValIdx ="  << foundValIdx
             << "value =" << foundValIdx.data().toString();
    return valIdx.data();
}*/

bool PropertyEditor::getIndex(QString searchName, QModelIndex parent)
{
/*
Searches column 0 of the model for the text searchName in the role UR_Name and returns the
index.  The role UR_Name is used as the display values could be duplicated in several rows
in the model.
*/
    foundIdx = QModelIndex();
    for(int r = 0; r < model->rowCount(parent); ++r) {
        QModelIndex idx0 = model->index(r, 0, parent);
        QString name = model->data(idx0, UR_Name).toString();
        if (name == searchName) {
            foundIdx = idx0;
            return true;
        }
        // iterate children
        if (model->hasChildren(idx0)) {
            getIndex(searchName, idx0);
        }
    }
    return false;
}

QWidget*  PropertyEditor::addItem(ItemInfo &i)
{
/*
Adds a row to the properties tree (model).  The necessary elements of the ItemInfo struct are
supplied by the calling function.
*/
    int row;
    QModelIndex capIdx;                             // caption field
    QModelIndex valIdx;                             // value field
    QModelIndex parIdx = QModelIndex();             // parent caption field
    QStandardItem *catItem = new QStandardItem;
    QStandardItem *valItem = new QStandardItem;
    QStandardItem *parItem = new QStandardItem;
    parItem = nullptr;

    getIndex(i.parentName);
    parIdx = foundIdx;
    parItem = model->itemFromIndex(parIdx);
    /*
    qDebug() << "Item =" << i.name
             << "Searching for =" << i.parentName
             << "parIdx =" << parIdx
             << "parIdx name =" << model->data(parIdx, UR_Name).toString();
//    */
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

    if (capIdx == model->index(0,0,QModelIndex()))
        qDebug() << __FUNCTION__ << "root:" << i.name << i.captionText;

    // caption
    model->setData(capIdx, i.decorateGradient, UR_DecorateGradient);
    model->setData(valIdx, i.decorateGradient, UR_DecorateGradient);
    model->setData(capIdx, i.captionText);
    model->setData(capIdx, i.name, UR_Name);
    model->setData(capIdx, i.tooltip, Qt::ToolTipRole);
    model->setData(valIdx, i.tooltip, Qt::ToolTipRole);
    model->setData(capIdx, i.itemIndex, UR_ItemIndex);

    // if no value associated (header item or spacer etc) then we are done
    if (!i.hasValue) {
        clearItemInfo(i);
        return nullptr;
    }

    // value
    model->setData(valIdx, i.value, Qt::EditRole);
    model->setData(valIdx, i.delegateType, UR_DelegateType);
    model->setData(valIdx, i.itemIndex, UR_ItemIndex);
//    qDebug() << __FUNCTION__ << valIdx << i.name << i.itemIndex << valIdx.data(UR_ItemIndex);
    model->setData(valIdx, i.valueName, UR_Source);
    model->setData(valIdx, i.type, UR_Type);
    model->setData(valIdx, i.min, UR_Min);
    model->setData(valIdx, i.max, UR_Max);
    model->setData(valIdx, i.fixedWidth, UR_LabelFixedWidth);
    model->setData(valIdx, i.color, UR_Color);
    model->setData(valIdx, i.dropList, UR_StringList);
    model->setData(valIdx, i.index, UR_QModelIndex);
    sourceIdx[i.valueName] = valIdx;

    // reset data struct
    clearItemInfo(i);

    return propertyDelegate->createEditor(this, *styleOptionViewItem, valIdx);
}

void PropertyEditor::clearItemInfo(ItemInfo &i)
{
    i.itemIndex = 0;                // all
    i.name = "";                    // all
    i.parentName = "";              // all
    i.decorateGradient = false;     // Root headers as reqd
    i.hasValue = true;              // all
    i.tooltip = "";                 // all
    i.captionText = "";             // all
    i.captionIsEditable = false;    // all
    i.delegateType = DT_None;       // all
    i.value = 0;                    // except hdr
    i.valueName = "";               // except hdr
    i.type = "";                    // except hdr
    i.min = 0;                      // DT_Spinbox, DT_Slider
    i.max = 0;                      // DT_Spinbox, DT_Slider
    i.fixedWidth = 50;              // DT_Slider
    i.dropList.clear();             // DT_Combo
    i.index = QModelIndex();        // except hdr if connected to datamodel (ie InfoView fields to show)
}

void PropertyEditor::expandBranch(QString text)
{
/*
Expand one preferences tree branch.  This is used to open the preferences for a specific
item - for example, in the metadata panel context panel there is an option to show / hide
items, which is controlled by the "Metadata panel items" in preferences.

Note: Even though treeIndex will find any index only root branches work. If the text for a
secondary or tertiary branch is sent then all branches expand.
*/
    collapseAll();
    getIndex(text);
    expandRecursively(foundIdx);
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

void PropertyEditor::resizeColumns(QString stringToFitCaptions, QString stringToFitValues)
{
    QFont fnt = this->font();
    int px = static_cast<int>(G::fontSize.toInt() * G::ptToPx);
    fnt.setPixelSize(px);
    QFontMetrics fm(fnt);
    captionColumnWidth = fm.boundingRect(stringToFitCaptions).width();
    valueColumnWidth = fm.boundingRect(stringToFitValues).width();
    setColumnWidth(0, captionColumnWidth);
    setColumnWidth(1, valueColumnWidth);
}

void PropertyEditor::setSolo(bool isSolo)
{
    this->isSolo = isSolo;
}
