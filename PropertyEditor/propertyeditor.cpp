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
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    setRootIsDecorated(true);
    setAlternatingRowColors(true);
    setSelectionBehavior(QAbstractItemView::SelectRows);
    setEditTriggers(QAbstractItemView::AllEditTriggers);
    setSelectionBehavior(QAbstractItemView::SelectRows);
    setSelectionMode(QAbstractItemView::SingleSelection);
    setSortingEnabled(true);

    indentation = 15;
    setIndentation(indentation);  

    model = new QStandardItemModel;
    model->setColumnCount(3);
//    hideColumn(1);
//    setColumnWidth(2, 0);
//    setColumnHidden(2, true);
//    proxy = new SortProperties(this);
//    proxy->setSourceModel(model);
//    setSortingEnabled(true);
    setModel(model);
//    setModel(proxy);

//    QHeaderView *hdr = new QHeaderView(Qt::Horizontal);
//    setHeader(hdr);
//    hdr->setVisible(true);

    propertyDelegate = new PropertyDelegate(this);
    setItemDelegate(propertyDelegate);
    styleOptionViewItem = new QStyleOptionViewItem;
    propertyDelegate->isAlternatingRows = true;

    connect(this->propertyDelegate, &PropertyDelegate::itemChanged,
            this, &PropertyEditor::itemChange);

    connect(propertyDelegate, &PropertyDelegate::editorWidgetToDisplay,
            this, &PropertyEditor::editorWidgetToDisplay);

    connect(propertyDelegate, &PropertyDelegate::drawBranchesAgain, this, &PropertyEditor::drawBranches);
}

void PropertyEditor::editorWidgetToDisplay(QModelIndex idx, QWidget *editor)
/*
Sets the custom editor widget for the value column (column 2).
*/
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    setIndexWidget(idx, editor);
    emit propertyDelegate->closeEditor(editor);
}

void PropertyEditor::itemChange(QModelIndex)
// virtual function to be subclassed
{

}

void PropertyEditor::updateHiddenRows(QModelIndex parent)
{
    for (int r = 0; r < model->rowCount(parent); ++r) {
        QModelIndex idx = model->index(r, CapColumn, parent);
        if (model->data(idx, UR_isHidden).toBool())
            setRowHidden(r, parent, true);
        else
            setRowHidden(r, parent, false);
        // iterate children
        if (model->hasChildren(idx)) {
            updateHiddenRows(idx);
        }
    }
}

QModelIndex PropertyEditor::findIndex(QString name)
{
    QModelIndex start = model->index(0,0,QModelIndex());
    QModelIndexList list = model->match(start, UR_Name, name, 1, Qt::MatchExactly | Qt::MatchRecursive);
    if (list.size() > 0) return list.at(0);
    else return QModelIndex();
}

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
        qDebug() << __FUNCTION__ << searchName << name;
        if (name == searchName) {
            foundIdx = idx0;
            qDebug() << __FUNCTION__ << "found" << idx0;

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
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    int row;
//   capIdx;                                        // caption field defined in header file
//   valIdx;                                        // value field defined in header
    QModelIndex parIdx = QModelIndex();             // parent caption field
    QStandardItem *capItem = new QStandardItem;
    QStandardItem *valItem = new QStandardItem;
    QStandardItem *ordItem = new QStandardItem;
    QStandardItem *parItem = new QStandardItem;
    parItem = nullptr;

    if (i.parIdx.isValid()) parIdx = i.parIdx;
    else parIdx = findIndex(i.parentName);
    parItem = model->itemFromIndex(parIdx);
    /*
    qDebug()
            << "Parent =" << i.parentName
             << "Item =" << i.name;
//             << "parIdx name =" << model->data(parIdx, UR_Name).toString();
//    */
    if (parItem == nullptr) {
        // First item = root
        row = model->rowCount();
        model->insertRow(row, QModelIndex());
//        if (row == 0) model->insertRow(0, QModelIndex());
//        else model->appendRow(capItem);
        capIdx = model->index(row, CapColumn, QModelIndex());
        ordIdx = model->index(row, OrdColumn, QModelIndex());
        valIdx = model->index(row, ValColumn, QModelIndex());
    }
    else {
        // row for next child to add
        if (i.insertRow == -1) row = model->rowCount(parIdx);
        else row = i.insertRow;
        if (row) {
            parItem->insertRow(row, capItem);
        }
        else {
            parItem->setChild(row, CapColumn, capItem);
            parItem->setChild(row, ValColumn, valItem);
            parItem->setChild(row, OrdColumn, ordItem);
        }
        capIdx = model->index(row, CapColumn, parIdx);
        ordIdx = model->index(row, OrdColumn, parIdx);
        valIdx = model->index(row, ValColumn, parIdx);
    }

    // caption
    model->setData(capIdx, i.hasValue, UR_hasValue);
    model->setData(capIdx, i.sortOrder, UR_SortOrder); // -1 if not sort
    if (i.sortOrder != -1) model->setData(ordIdx, i.sortOrder);
    model->setData(capIdx, i.captionIsEditable, UR_CaptionIsEditable);
    model->setData(capIdx, i.isIndent, UR_isIndent);
    model->setData(capIdx, i.isHeader, UR_isHeader);
    model->setData(valIdx, i.isHeader, UR_isHeader);
    model->setData(capIdx, i.isDecoration, UR_isDecoration);
    model->setData(valIdx, i.isDecoration, UR_isDecoration);
    model->setData(capIdx, i.decorateGradient, UR_isBackgroundGradient);
    model->setData(valIdx, i.decorateGradient, UR_isBackgroundGradient);
    model->setData(capIdx, i.captionText);
    model->setData(capIdx, i.name, UR_Name);
    model->setData(capIdx, i.tooltip, Qt::ToolTipRole);
    model->setData(valIdx, i.tooltip, Qt::ToolTipRole);
    model->setData(capIdx, i.path, UR_Path);
    model->setData(valIdx, i.path, UR_Path);
    model->setData(capIdx, i.itemIndex, UR_ItemIndex);
    model->setData(capIdx, false, UR_isHidden);
    model->setData(valIdx, false, UR_isHidden);
    model->setData(capIdx, i.key, UR_Source);           // key = "effect" for sortOrder

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
    model->setData(valIdx, i.key, UR_Source);
    model->setData(valIdx, i.type, UR_Type);
    model->setData(valIdx, i.min, UR_Min);
    model->setData(valIdx, i.max, UR_Max);
    model->setData(valIdx, i.fixedWidth, UR_FixedWidth);
    model->setData(valIdx, i.color, UR_Color);
    model->setData(valIdx, i.dropList, UR_StringList);
    model->setData(valIdx, i.index, UR_QModelIndex);

    sourceIdx[i.key] = valIdx;      // map item key string to value index

    /*
    qDebug() << __FUNCTION__
             << "i.name =" << i.name
                ;
//    */

    // reset item data struct
    clearItemInfo(i);

    return propertyDelegate->createEditor(this, *styleOptionViewItem, valIdx);
}

void PropertyEditor::clearItemInfo(ItemInfo &i)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    i.itemIndex = 0;                // all
    i.sortOrder = -1;               // all
    i.name = "";                    // all
    i.parentName = "";              // all
    i.isHeader = false;             // set true for header rows
    i.isDecoration = false;         // set true for headers that can expand
    i.isIndent = true;              // as required if not header with decoration
    i.decorateGradient = false;     // Generally root headers as reqd
    i.hasValue = true;              // all
    i.tooltip = "";                 // all
    i.captionText = "";             // all
    i.captionIsEditable = false;    // all
    i.delegateType = DT_None;       // all
    i.value = 0;                    // except hdr
    i.key = "";               // except hdr
    i.type = "";                    // except hdr
    i.min = 0;                      // DT_Spinbox, DT_Slider
    i.max = 0;                      // DT_Spinbox, DT_Slider
    i.fixedWidth = 50;              // DT_Slider
    i.dropList.clear();             // DT_Combo
    i.color = QColor(G::textShade,G::textShade,G::textShade).name();
    i.index = QModelIndex();
    i.parIdx = QModelIndex();
    i.insertRow = -1;               // if -1 then append, else insert at this row
}

void PropertyEditor::getItemInfo(QModelIndex &idx, ItemInfo &copy)
{
    {
    #ifdef ISDEBUG
    G::track(__FUNCTION__);
    #endif
    }
    capIdx = idx;
    valIdx = model->index(idx.row(), ValColumn, idx.parent());
    copy.itemIndex = model->data(capIdx, UR_ItemIndex).toInt();
    copy.name = model->data(capIdx, UR_Name).toString();
    copy.parentName = model->data(capIdx.parent(), UR_Name).toString();
    copy.isHeader = model->data(capIdx, UR_isHeader).toBool();
    copy.isDecoration = model->data(capIdx, UR_isDecoration).toBool();
    copy.isIndent = model->data(capIdx, UR_isIndent).toBool();
    copy.decorateGradient = model->data(capIdx, UR_isBackgroundGradient).toBool();
    copy.hasValue = model->data(capIdx, UR_hasValue).toBool();
    copy.tooltip = model->data(capIdx.parent(), Qt::ToolTipRole).toString();
    copy.captionText = model->data(capIdx).toString();
    copy.captionIsEditable = model->data(capIdx, UR_CaptionIsEditable).toBool();
    copy.delegateType = model->data(valIdx, UR_DelegateType).toInt();
    copy.value = model->data(valIdx);
    copy.key = model->data(capIdx.parent(), UR_Source).toString();
    copy.type = model->data(valIdx, UR_Type).toString();
    copy.min = model->data(valIdx, UR_Min).toInt();
    copy.max = model->data(valIdx, UR_Max).toInt();
    copy.fixedWidth = model->data(valIdx, UR_FixedWidth).toInt();
    copy.dropList = model->data(valIdx, UR_StringList).toStringList();
    copy.color = model->data(valIdx, UR_Color).toString();
    copy.index = capIdx;
    copy.parIdx = capIdx.parent();
    copy.insertRow = -1;
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
    qDebug() << __FUNCTION__ << text;
    collapseAll();
    getIndex(text);
    expandRecursively(foundIdx);
}

void PropertyEditor::mousePressEvent(QMouseEvent *event)
/*
Set the current index and expand/collapse when click anywhere on a row that has children.
*/
{
    // ignore right mouse clicks for context menu
    if (event->button() == Qt::RightButton) return;

    QModelIndex idx = indexAt(event->pos());
//    QModelIndex idx = proxy->mapToSource(indexAt(event->pos()));
    if (idx.column() != 0) idx = model->index(idx.row(), CapColumn, idx.parent());
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

void PropertyEditor::mouseReleaseEvent(QMouseEvent */*event*/)
{
/*
    Do not pass on to QTreeView as this will enable QTreeView expanding and collapsing when the
    decoration is clicked.
*/
}

void PropertyEditor::resizeColumns(QString stringToFitCaptions, QString stringToFitValues)
{
    QFont fnt = this->font();
    int px = static_cast<int>(G::fontSize.toInt() * G::ptToPx);
    fnt.setPixelSize(px);
    QFontMetrics fm(fnt);
    captionColumnWidth = fm.boundingRect(stringToFitCaptions).width();
    valueColumnWidth = fm.boundingRect(stringToFitValues).width();
    setColumnWidth(CapColumn, captionColumnWidth);
    setColumnWidth(OrdColumn, 25);
    setColumnWidth(ValColumn, valueColumnWidth);
//    hideColumn(OrdColumn);

}

void PropertyEditor::setSolo(bool isSolo)
{
    this->isSolo = isSolo;
}

// --------------------------------------------------------------------------------------------
// SortProperties Class used to custom sort
// --------------------------------------------------------------------------------------------

SortProperties::SortProperties(QObject *parent) : QSortFilterProxyModel(parent)
{
}

bool SortProperties::lessThan(const QModelIndex &left, const QModelIndex &right) const
{
    return false;
    QString leftSource = left.data(UR_Source).toString();
    QString rightSource = right.data(UR_Source).toString();
    QString leftName = left.data().toString();
    QString rightName = right.data().toString();
    QString leftParent = left.parent().data().toString();
    QString rightParent = right.parent().data().toString();
    int leftSortOrder = left.data(UR_SortOrder).toInt();
    int rightSortOrder = right.data(UR_SortOrder).toInt();
    bool less = left.data(UR_SortOrder) > right.data(UR_SortOrder);
    bool test = leftName < rightName;

    qDebug().noquote()
             << "LEFT "
             << left
             << "P:" << leftParent.leftJustified(22)
             << "N:" << leftName.leftJustified(22)
             << "S:" << leftSource.leftJustified(20)
                ;
//             << "RIGHT "
//             << "N:" << rightName.leftJustified(22)
//             << "P:" << rightParent.leftJustified(22)
//             << "S:" << rightSource.leftJustified(20)
//             << leftSortOrder
//             << rightSortOrder
//             << "less =" << less
//             << "test =" << test
//                ;

    if (leftSource == "effect"  && rightSource == "effect") {
        return less;
    }
    return false;
}

void SortProperties::sortChange()
{
    invalidate();
}
