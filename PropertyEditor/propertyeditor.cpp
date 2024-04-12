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
    source, datamodel index, min value, max value, a stringlist for combos etc).

PropertyEditor subclass ie Preferences.  All the property items are defined and added to the
    treeview in addItems().  When an item value has been changed in an editor itemChanged(idx)
    is signaled and the variable is updated in Winnow and any necessary actions are executed.
    For example, if the thumbnail size is increased then the IconView function to do this is
    called.  Use setItemValue to access and change the value held in a custom widget.

Property Model
    row = unique property
    col 0 = caption describing property
    col 1 = widget to edit property value

    The values and propeties of each item in the model are stored by user role:
    UR_Name = Qt::UserRole + 1,         // unique name to identify the item
    UR_ItemIndex,                       // unique, consistent index for all items
    UR_SortOrder,                       // override appended order ie for effects, borders
    UR_DefaultValue,                    // when created or double click
    UR_SettingsPath,                    // settings path
    UR_Editor,                          // pointer to custom editor created
    UR_DelegateType,                    // type of custom widget
    UR_hasValue,                        // if no value then value column is empty
    UR_CaptionIsEditable,               // can edit caption
    UR_isIndent,                        // indent column 0 in QTreeView
    UR_isHeader,                        // header item in QTreeView
    UR_okToCollapseRoot,                // collapse root item when collapseAll
    UR_isDecoration,                    // show expand/collapse decoration
    UR_isBackgroundGradient,            // make the root rows dark gray gradiant
    UR_isHidden,                        // flag to hide/show row in tree
    UR_isEnabled,                       // flag to show disabled in tree
    UR_Source,                          // name of property/variable being edited = i.key
    UR_QModelIndex,                     // index from another model ie infoView->ok
    UR_Type,                            // the data type required by the delegate
    UR_Min,                             // validate minimum value
    UR_Max,                             // validate maximum value
    UR_Div,                             // Divide slider value by amount to get double from int
    UR_DivPx,                           // Slider singlestep = one pixel
    UR_FixedWidth,                      // fixed label width in custom widget
    UR_StringList,                      // list of items for comboBox
    UR_IconList,                        // list of icons for comboBox
    UR_IsBtn,                           // button to run a secondary widget or help
    UR_BtnText,                         // button text
    UR_Color                            // QColor for text in LineEdit, LabelEdit

Parameters to override as required when subclass:

    setIndentation
    setAlternatingRowColors
    resizeColumns
    isExpandRecursively
*/

QVariant PropertyModel::data(const QModelIndex &index, int role) const
{
    if (role == Qt::BackgroundRole) {
        qDebug() << "PropertyModel::data" << QStandardItemModel::data(index, role);
    }
    return QStandardItemModel::data(index, role);
}

PropertyEditor::PropertyEditor(QWidget *parent) : QTreeView(parent)
{
    if (G::isLogger) G::log("PropertyEditor::PropertyEditor");
    setRootIsDecorated(true);
    setAlternatingRowColors(true);
    setSelectionBehavior(QAbstractItemView::SelectRows);
    setSelectionMode(QAbstractItemView::SingleSelection);
    setEditTriggers(QAbstractItemView::AllEditTriggers);
    setSortingEnabled(true);
    setObjectName("EmbelProperties");

    indentation = 15;
    setIndentation(indentation);

    model = new QStandardItemModel;
    model->setColumnCount(3);
    model->setSortRole(UR_SortOrder);
    setModel(model);
    propertyDelegate = new PropertyDelegate(this);
    setItemDelegate(propertyDelegate);
    styleOptionViewItem = new QStyleOptionViewItem;
    propertyDelegate->isAlternatingRows = true;

    connect(this->propertyDelegate, &PropertyDelegate::itemChanged,
            this, &PropertyEditor::itemChange);

    connect(propertyDelegate, &PropertyDelegate::editorWidgetToDisplay,
            this, &PropertyEditor::editorWidgetToDisplay);

    connect(propertyDelegate, &PropertyDelegate::drawBranchesAgain, this, &PropertyEditor::drawBranches);

    connect(this, &PropertyEditor::fontSizeChange, propertyDelegate, &PropertyDelegate::fontSizeChanged);
}

PropertyEditor::~PropertyEditor()
{
    if (G::isLogger) G::log("PropertyEditor::~PropertyEditor");
    close(model->index(0,0,QModelIndex()));
    delete propertyDelegate;
    delete styleOptionViewItem;
    delete model;
}

void PropertyEditor::editorWidgetToDisplay(QModelIndex idx, QWidget *editor)
/*
    Sets the custom editor widget for the value column (column 2).
*/
{
    if (G::isLogger) G::log("PropertyEditor::editorWidgetToDisplay");
    setIndexWidget(idx, editor);
    emit propertyDelegate->closeEditor(editor);
}

void PropertyEditor::itemChange(QModelIndex)
// virtual function to be subclassed
{

}

void PropertyEditor::getIndexFromNameAndParent(QString name, QString parName, QModelIndex parent)
{
    for (int r = 0; r < model->rowCount(parent); ++r) {
        QModelIndex idx = model->index(r, CapColumn, parent);
        if (idx.parent().isValid()) {
            QString thisName = idx.data().toString();
            QString thisParName = idx.parent().data().toString();
//            qDebug() << "PropertyEditor::getIndexFromNameAndParent" << thisName << thisParName;
            if (thisName == name && thisParName == parName) {
                foundIdx = idx;
            }
        }
        // iterate children
        if (model->hasChildren(idx)) {
            getIndexFromNameAndParent(name, parName, idx);
        }
    }
}

void PropertyEditor::updateHiddenRows(QModelIndex parent)
{
    for (int r = 0; r < model->rowCount(parent); ++r) {
        QModelIndex idx = model->index(r, CapColumn, parent);
        if (model->data(idx, UR_isHidden).toBool()) {
            setRowHidden(r, parent, true);
            /*
            qDebug() << "PropertyEditor::updateHiddenRows"
                     << idx.parent().data().toString().leftJustified(20)
                     << idx.data().toString().leftJustified(20)
                     << "UR_isHidden = true";
//            */
        }
        else {
            setRowHidden(r, parent, false);
            /*
            qDebug() << "PropertyEditor::updateHiddenRows"
                     << idx.parent().data().toString().leftJustified(20)
                     << idx.data().toString().leftJustified(20)
                     << "UR_isHidden = false";
//                     */
        }
        // iterate children
        if (model->hasChildren(idx)) {
            updateHiddenRows(idx);
        }
    }
}

int PropertyEditor::uniqueItemIndex(QModelIndex parentIdx)
{
/*
Creates a unique number, starting at 100, for the row in the model for the given parent.  This
is used to find the item when it has been moved.  For example, if the item is one of a list of
borders which can contain texts, then the ItemIndex can be assigned to the text, and it can be
associated with the correct border even when the border is resorted.
*/
    if (G::isLogger) G::log("PropertyEditor::uniqueItemIndex");
    int index = 100;
    while (true) {
        bool matchFound = false;
//        for (int row = 0; row < model->rowCount(parentIdx); ++row) {
        for (int row = 0; row < model->rowCount(parentIdx); ++row) {
            QModelIndex idx = model->index(row, 0, parentIdx);
            // match found, keep trying
            if (idx.data(UR_ItemIndex) == index) {
                matchFound = true;
                break;
            }
        }
        if (!matchFound) return index;
        index++;
    }
}

QModelIndex PropertyEditor::getItemIndex(int itemIndex, QModelIndex /*parentIdx*/)
{
/*
    Returns the QModelIndex for the itemIndex. itemIndex is unique for every row in the
    model. It is assigned in addItem(). Use it to find the correct index when items have
    been sorted or deleted, and the associated settings and vectors have not been
    adjusted.
*/
    if (G::isLogger) G::log("PropertyEditor::getItemIndex");
    QModelIndex start = model->index(0,0,QModelIndex());
    QModelIndexList list = model->match(start, UR_ItemIndex, itemIndex, 1, Qt::MatchExactly | Qt::MatchRecursive);
    if (list.size() > 0) return list.at(0);
    else return QModelIndex();
}

QModelIndex PropertyEditor::findCaptionIndex(QString name)
{
    QModelIndex start = model->index(0,0,QModelIndex());
    QModelIndexList list = model->match(start, UR_Name, name, 1, Qt::MatchExactly | Qt::MatchRecursive);
    if (list.size() > 0) return list.at(0);
    else return QModelIndex();
}

QModelIndex PropertyEditor::findValueIndex(QString name)
{
    QModelIndex start = model->index(0,0,QModelIndex());
    QModelIndexList list = model->match(start, UR_Name, name, 1, Qt::MatchExactly | Qt::MatchRecursive);
    if (list.size() > 0) {
        return model->index(list.at(0).row(), 1, list.at(0).parent());
    }
    else return QModelIndex();
}

QVariant PropertyEditor::getItemValue(QString name, QModelIndex parent)
{
    getIndex(name, parent);
    QModelIndex idx = model->index(foundIdx.row(), ValColumn, foundIdx.parent());
    return idx.data();
}

bool PropertyEditor::getIndex(QString searchName, QModelIndex parent)
{
/*
    Searches column 0 of the model for the text searchName in the role UR_Name and
    returns the index. The role UR_Name is used as the display values could be duplicated
    in several rows in the model.
*/
    foundIdx = QModelIndex();
    for(int r = 0; r < model->rowCount(parent); ++r) {
        QModelIndex idx0 = model->index(r, 0, parent);
        QString name = model->data(idx0, UR_Name).toString();
        if (name == searchName) {
            foundIdx = idx0;
            /*
            qDebug() << "PropertyEditor::getIndex"
                     << "found" << idx0
                     << "name =" << name
                        ;
//                        */
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
    Adds a row to the properties tree (model). The necessary elements of the ItemInfo
    struct are supplied by the calling function. Each row has two columns: caption and
    value. The caption is a description of the value. The value is a propertyWidget
    custom editor (ComboBoxEditor, SliderEditor, LineEditor etc).
*/
    if (G::isLogger) G::log("PropertyEditor::addItem");
    int row;
    // capIdx defined in header file
    // valIdx defined in header file
    QModelIndex parIdx = QModelIndex();             // parent caption field
    QStandardItem *capItem = new QStandardItem;
    QStandardItem *valItem = new QStandardItem;
    QStandardItem *parItem = new QStandardItem;
    parItem = nullptr;
    itemIndex++;

    if (i.parIdx.isValid()) parIdx = i.parIdx;
    else parIdx = findCaptionIndex(i.parentName);
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
        capIdx = model->index(row, CapColumn, QModelIndex());
        valIdx = model->index(row, ValColumn, QModelIndex());
    }
    else {
        row = model->rowCount(parIdx);
        if (row) {
            parItem->insertRow(row, capItem);
        }
        else {
            parItem->setChild(row, CapColumn, capItem);
            parItem->setChild(row, ValColumn, valItem);
        }
        capIdx = model->index(row, CapColumn, parIdx);
        valIdx = model->index(row, ValColumn, parIdx);
    }

//    qDebug() << "PropertyEditor::addItem" << i.captionText;

    // caption
    model->setData(capIdx, i.hasValue, UR_hasValue);
    model->setData(capIdx, i.sortOrder, UR_SortOrder); // -1 if not sort
    model->setData(capIdx, i.captionIsEditable, UR_CaptionIsEditable);
    model->setData(capIdx, i.isIndent, UR_isIndent);
    model->setData(capIdx, i.isHeader, UR_isHeader);
    model->setData(valIdx, i.isHeader, UR_isHeader);
    model->setData(capIdx, i.okToCollapseRoot, UR_okToCollapseRoot);
    model->setData(valIdx, i.okToCollapseRoot, UR_okToCollapseRoot);
    model->setData(capIdx, i.isDecoration, UR_isDecoration);
    model->setData(valIdx, i.isDecoration, UR_isDecoration);
    model->setData(capIdx, i.decorateGradient, UR_isBackgroundGradient);
    model->setData(valIdx, i.decorateGradient, UR_isBackgroundGradient);
//    if (UR_isBackgroundGradient) model->setData(capIdx, QBrush(Qt::green), Qt::BackgroundRole);
    model->setData(capIdx, i.captionText);
    model->setData(capIdx, i.name, UR_Name);
    model->setData(capIdx, i.tooltip, Qt::ToolTipRole);
    model->setData(valIdx, i.tooltip, Qt::ToolTipRole);
    model->setData(capIdx, i.path, UR_SettingsPath);
    model->setData(valIdx, i.path, UR_SettingsPath);
    model->setData(capIdx, itemIndex, UR_ItemIndex);
    model->setData(capIdx, false, UR_isHidden);
    model->setData(valIdx, false, UR_isHidden);
    model->setData(capIdx, true, UR_isEnabled);
    model->setData(valIdx, true, UR_isEnabled);
    model->setData(capIdx, i.key, UR_Source);           // key = "effect" for sortOrder

    // if no value associated (header item or spacer etc) then we are done
    if (!i.hasValue) {
        clearItemInfo(i);
        return nullptr;
    }

    // value
    model->setData(valIdx, i.value, Qt::EditRole);
    model->setData(valIdx, i.defaultValue, UR_DefaultValue);
    model->setData(valIdx, i.delegateType, UR_DelegateType);
    model->setData(valIdx, itemIndex, UR_ItemIndex);
    model->setData(valIdx, i.key, UR_Source);
    model->setData(valIdx, i.type, UR_Type);
    model->setData(valIdx, i.min, UR_Min);
    model->setData(valIdx, i.max, UR_Max);
    model->setData(valIdx, i.div, UR_Div);
    model->setData(valIdx, i.step, UR_DivPx);
    model->setData(valIdx, i.fixedWidth, UR_FixedWidth);
    model->setData(valIdx, i.color, UR_Color);
    model->setData(valIdx, i.dropList, UR_StringList);
    model->setData(valIdx, i.dropIconList, UR_IconList);
    model->setData(valIdx, i.index, UR_QModelIndex);

    sourceIdx[i.key] = valIdx;      // map item key string to value index

    // reset item data struct
    clearItemInfo(i);

    QWidget *editor = propertyDelegate->createEditor(this, *styleOptionViewItem, valIdx);
    // save editor in data model for later use (see PropertyEditor::setItemValue)
    model->setData(valIdx, QVariant::fromValue(static_cast<void*>(editor)), UR_Editor);

    return editor;
}

void PropertyEditor::clearItemInfo(ItemInfo &i)
{
    if (G::isLogger) G::log("PropertyEditor::clearItemInfo");
    i.itemIndex = 0;                // all
    i.sortOrder = -1;               // all
    i.name = "";                    // all
    i.path = "";                    // all
    i.parentName = "";              // all
    i.isHeader = false;             // set true for header rows
    i.isDecoration = false;         // set true for headers that can expand
    i.isIndent = true;              // as required if not header with decoration
    i.decorateGradient = false;     // Generally root headers as reqd
    i.hasValue = true;              // all
    i.tooltip = "";                 // all
    i.captionText = "";             // all
    i.okToCollapseRoot = true;      // only root items
    i.captionIsEditable = false;    // all
    i.delegateType = DT_None;       // all
    i.defaultValue = "__Ignore__";
    i.value = 0;                    // except hdr
    i.key = "";                     // except hdr
    i.type = "";                    // except hdr
    i.min = 0;                      // DT_Spinbox, DT_Slider
    i.max = 0;                      // DT_Spinbox, DT_Slider
    i.div = 1;                      // DT_Slider (zero == int, nonzero = double)
    i.step = 1;                    // DT_Slider
    i.fixedWidth = 50;              // DT_Slider
    i.dropList.clear();             // DT_Combo
    i.dropIconList.clear();         // DT_Combo
    i.color = QColor(G::textShade,G::textShade,G::textShade).name();
    i.index = QModelIndex();
    i.parIdx = QModelIndex();
}

void PropertyEditor::setItemEnabled(QString name, bool state)
{
    if (G::isLogger) G::log("PropertyEditor::setItemEnabled");
    QModelIndex capIdx = findCaptionIndex(name);
    model->setData(capIdx, state, UR_isEnabled);
    model->dataChanged(capIdx,capIdx);
    QModelIndex idx = model->index(capIdx.row(), ValColumn, capIdx.parent());
    int type = model->data(idx, UR_DelegateType).toInt();
    if (type == DT_Label) {
        auto editor = static_cast<LabelEditor*>(idx.data(UR_Editor).value<void*>());
        editor->setEnabled(state);
    }
    if (type == DT_LineEdit) {
        auto editor = static_cast<LineEditor*>(idx.data(UR_Editor).value<void*>());
        editor->setEnabled(state);
    }
    if (type == DT_Spinbox) {
        auto editor = static_cast<SpinBoxEditor*>(idx.data(UR_Editor).value<void*>());
        editor->setEnabled(state);
    }
    if (type == DT_DoubleSpinbox) {
        auto editor = static_cast<DoubleSpinBoxEditor*>(idx.data(UR_Editor).value<void*>());
        editor->setEnabled(state);
    }
    if (type == DT_Checkbox) {
        auto editor = static_cast<CheckBoxEditor*>(idx.data(UR_Editor).value<void*>());
        editor->setEnabled(state);
    }
    if (type == DT_Combo) {
        auto editor = static_cast<ComboBoxEditor*>(idx.data(UR_Editor).value<void*>());
        editor->setEnabled(state);
    }
    if (type == DT_Slider) {
        auto editor = static_cast<SliderEditor*>(idx.data(UR_Editor).value<void*>());
        editor->setEnabled(state);
    }
    if (type == DT_Color) {
        auto editor = static_cast<ColorEditor*>(idx.data(UR_Editor).value<void*>());
        editor->setEnabled(state);
    }
}

void PropertyEditor::getItemInfo(QModelIndex &idx, ItemInfo &copy)
{
    if (G::isLogger) G::log("PropertyEditor::getItemInfo");
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
}

bool PropertyEditor::setItemValue(QString name, QVariant val)
{
    if (G::isLogger) G::log("PropertyEditor::setItemValue(QString name, QVariant val)", name);
    QModelIndex valIdx = findValueIndex(name);
    if (valIdx.isValid()) {
        setItemValue(valIdx, val);
        return true;
    }
    return false;
}

void PropertyEditor::setItemValue(QModelIndex idx, QVariant value)
{
/*

*/
    if (G::isLogger) G::log("PropertyEditor::(QModelIndex idx, int type, QVariant value)");
    if (value.toString() == "__Ignore__") return;
    int type = idx.data(UR_DelegateType).toInt();
    if (type == DT_Label) {
        auto editor = static_cast<LabelEditor*>(idx.data(UR_Editor).value<void*>());
        editor->setValue(value);
    }
    if (type == DT_LineEdit) {
        auto editor = static_cast<LineEditor*>(idx.data(UR_Editor).value<void*>());
        editor->setValue(value);
    }
    if (type == DT_Spinbox) {
        auto editor = static_cast<SpinBoxEditor*>(idx.data(UR_Editor).value<void*>());
        editor->setValue(value);
    }
    if (type == DT_DoubleSpinbox) {
        auto editor = static_cast<DoubleSpinBoxEditor*>(idx.data(UR_Editor).value<void*>());
        editor->setValue(value);
    }
    if (type == DT_Checkbox) {
        auto editor = static_cast<CheckBoxEditor*>(idx.data(UR_Editor).value<void*>());
        editor->setValue(value);
    }
    if (type == DT_Combo) {
        auto editor = static_cast<ComboBoxEditor*>(idx.data(UR_Editor).value<void*>());
        editor->setValue(value);
    }
    if (type == DT_Slider) {
        auto editor = static_cast<SliderEditor*>(idx.data(UR_Editor).value<void*>());
        editor->setValue(value);
    }
    if (type == DT_Color) {
        auto editor = static_cast<ColorEditor*>(idx.data(UR_Editor).value<void*>());
        editor->setValue(value);
    }
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
    qDebug() << "PropertyEditor::expandBranch" << text;
    collapseAll();
    getIndex(text);
    expandRecursively(foundIdx);
}

void PropertyEditor::mouseDoubleClickEvent(QMouseEvent *event)
{
/*
    Set the value to the default value
*/
    if (G::isLogger) G::log("PropertyEditor::mouseDoubleClickEvent");
    QModelIndex idx = indexAt(event->pos());
    idx = model->index(idx.row(), ValColumn, idx.parent());
    QVariant value = idx.data(UR_DefaultValue);
    if (idx.data(UR_DelegateType).toInt() == DT_Slider)
        value = idx.data(UR_DefaultValue).toDouble() * idx.data(UR_Div).toInt();
    setItemValue(idx, value);
}

void PropertyEditor::mousePressEvent(QMouseEvent *event)
/*
    Set the current index and expand/collapse when click anywhere on a row that has children.
*/
{
    // ignore right mouse clicks (reserved for context menu)
    if (event->button() == Qt::RightButton) return;

    QModelIndex idx = indexAt(event->pos());
    if (idx.column() != 0) idx = model->index(idx.row(), CapColumn, idx.parent());
    QStandardItem *item = model->itemFromIndex(idx);
    if (idx.isValid() && item->hasChildren()) {
        bool isRoot = idx.parent() == QModelIndex();
        bool wasExpanded = isExpanded(idx);
        if (isRoot && !wasExpanded && isSolo) collapseAllExcept();

        if (idx.data(UR_okToCollapseRoot).toBool()) {
            if (isExpandRecursively) {
                wasExpanded ? collapse(idx) : expandRecursively(idx);
            }
            else {
                wasExpanded ? collapse(idx) : expand(idx);
            }
        }
    }
}

void PropertyEditor::mouseReleaseEvent(QMouseEvent * /*event*/)
{
/*
    Do not pass on to QTreeView as this will enable QTreeView expanding and collapsing when the
    decoration is clicked.
*/
}

void PropertyEditor::resizeColumns()
{
    QFont fnt = this->font();
    int px = static_cast<int>(G::strFontSize.toInt() * G::ptToPx);
    if (px < 6) px = 6;
    fnt.setPixelSize(px);
    QFontMetrics fm(fnt);
    captionColumnWidth = fm.boundingRect(stringToFitCaptions).width();
    valueColumnWidth = fm.boundingRect(stringToFitValues).width();
    width = captionColumnWidth + valueColumnWidth + 3;
    /*
    qDebug() << "PropertyEditor::resizeColumns"
             << "captionColumnWidth =" << captionColumnWidth
             << "valueColumnWidth =" << valueColumnWidth
             << "width =" << width
             << "px =" << px
        ; //*/

    setColumnWidth(CapColumn, captionColumnWidth);
    hideColumn(OrdColumn);
}

void PropertyEditor::fontSizeChanged(int fontSize)
{
    // qDebug() << "PropertyEditor::fontSizeChanged =" << fontSize;
    if (!ignoreFontSizeChangeSignals) {
        emit propertyDelegate->fontSizeChange(fontSize);
    }
    resizeColumns();
    // this forces treeview to trigger sizehint for every item
    scheduleDelayedItemsLayout();
}

void PropertyEditor::setSolo(bool isSolo)
{
    this->isSolo = isSolo;
}

void PropertyEditor::setExpandRecursively(bool isExpandRecursively)
{
    this->isExpandRecursively = isExpandRecursively;
}

void PropertyEditor::collapseAllExcept()
{
/*
    Iterates through all root items in the property tree and collapses if the item
    okToCollapseRoot = true.  This is used in EmbelProperties to keep the Template header
    open all the time to show the current template.
*/
    for(int r = 0; r < model->rowCount(QModelIndex()); ++r) {
        QModelIndex idx = model->index(r, 0);
        if (idx.data(UR_okToCollapseRoot).toBool()) collapse(idx);
    }

}

void PropertyEditor::close(QModelIndex parent)
{
    if (G::isLogger) G::log("PropertyEditor::close");
    for (int r = 0; r < model->rowCount(parent); ++r) {
        QModelIndex capIdx = model->index(r, CapColumn, parent);
        QModelIndex valIdx = model->index(r, ValColumn, parent);
        if (valIdx.data(UR_DelegateType).toInt() == DT_Label) {
            auto editor = static_cast<LabelEditor*>(valIdx.data(UR_Editor).value<void*>());
            delete editor;
        }
        if (valIdx.data(UR_DelegateType).toInt() == DT_LineEdit) {
            auto editor = static_cast<LineEditor*>(valIdx.data(UR_Editor).value<void*>());
            delete editor;
        }
        if (valIdx.data(UR_DelegateType).toInt() == DT_Spinbox) {
            auto editor = static_cast<SpinBoxEditor*>(valIdx.data(UR_Editor).value<void*>());
            delete editor;
        }
        if (valIdx.data(UR_DelegateType).toInt() == DT_DoubleSpinbox) {
            auto editor = static_cast<DoubleSpinBoxEditor*>(valIdx.data(UR_Editor).value<void*>());
            delete editor;
        }
        if (valIdx.data(UR_DelegateType).toInt() == DT_Checkbox) {
            auto editor = static_cast<CheckBoxEditor*>(valIdx.data(UR_Editor).value<void*>());
            delete editor;
        }
        if (valIdx.data(UR_DelegateType).toInt() == DT_Combo) {
            auto editor = static_cast<ComboBoxEditor*>(valIdx.data(UR_Editor).value<void*>());
            delete editor;
        }
        if (valIdx.data(UR_DelegateType).toInt() == DT_Slider) {
            auto editor = static_cast<SliderEditor*>(valIdx.data(UR_Editor).value<void*>());
            delete editor;
        }
        if (valIdx.data(UR_DelegateType).toInt() == DT_Color) {
            auto editor = static_cast<ColorEditor*>(valIdx.data(UR_Editor).value<void*>());
            delete editor;
        }
        // iterate children
        if (model->hasChildren(capIdx)) {
            close(capIdx);
        }
    }
}

void PropertyEditor::diagnosticProperties(QModelIndex parent)
{
    if (G::isLogger) G::log("PropertyEditor::diagnosticProperties");
    qDebug() << "PropertyEditor::diagnosticProperties" << "PROPERTIES MODEL\n";
    // model
    for (int r = 0; r < model->rowCount(parent); ++r) {
        QModelIndex idx0 = model->index(r, CapColumn, parent);
        QModelIndex idx1 = model->index(r, ValColumn, parent);
        QString p = parent.data(UR_Name).toString();
        QString n = idx0.data(UR_Name).toString();
        QVariant v = idx1.data(Qt::EditRole);
        QString s = idx0.data(UR_Source).toString();
        qDebug().noquote() << "Caption index                      " << idx0;
        qDebug().noquote() << "Value index                        " << idx1;
        qDebug().noquote() << "Caption UR_Name                    " << idx0.data(UR_Name).toString();
        qDebug().noquote() << "Caption Parent                     " << parent.data(UR_Name).toString();
        qDebug().noquote() << "Caption UR_Source                  " << idx0.data(UR_Source).toString();
        qDebug().noquote() << "Caption UR_Path                    " << idx0.data(UR_SettingsPath).toString();
        qDebug().noquote() << "Caption UR_ItemIndex               " << idx0.data(UR_ItemIndex).toString();
        qDebug().noquote() << "Caption Qt::ToolTipRole            " << idx0.data(Qt::ToolTipRole).toString();
        qDebug().noquote() << "Caption UR_hasValue                " << idx0.data(UR_hasValue).toString();
        qDebug().noquote() << "Caption UR_SortOrder               " << idx0.data(UR_SortOrder).toString();
        qDebug().noquote() << "Caption UR_CaptionIsEditable       " << idx0.data(UR_CaptionIsEditable).toString();
        qDebug().noquote() << "Caption UR_isIndent                " << idx0.data(UR_isIndent).toString();
        qDebug().noquote() << "Caption UR_isHeader                " << idx0.data(UR_isHeader).toString();
        qDebug().noquote() << "Caption UR_isDecoration            " << idx0.data(UR_isDecoration).toString();
        qDebug().noquote() << "Caption UR_isBackgroundGradient    " << idx0.data(UR_isBackgroundGradient).toString();
        qDebug().noquote() << "Caption UR_isHidden                " << idx0.data(UR_isHidden).toString();

        qDebug().noquote() << "Value   Qt::EditRole               " << idx1.data(Qt::EditRole).toString();
        qDebug().noquote() << "Value   UR_DefaultValue            " << idx1.data(UR_DefaultValue).toString();
        qDebug().noquote() << "Value   UR_Source                  " << idx1.data(UR_Source).toString();
        qDebug().noquote() << "Value   Qt::ToolTipRole            " << idx1.data(Qt::ToolTipRole).toString();
        qDebug().noquote() << "Value   UR_Path                    " << idx1.data(UR_SettingsPath).toString();
        qDebug().noquote() << "Value   UR_ItemIndex               " << idx1.data(UR_ItemIndex).toString();
        qDebug().noquote() << "Value   UR_isHeader                " << idx1.data(UR_isHeader).toString();
        qDebug().noquote() << "Value   UR_isDecoration            " << idx1.data(UR_isDecoration).toString();
        qDebug().noquote() << "Value   UR_isBackgroundGradient    " << idx1.data(UR_isBackgroundGradient).toString();
        qDebug().noquote() << "Value   UR_isHidden                " << idx1.data(UR_isHidden).toString();
        qDebug().noquote() << "Value   UR_Type                    " << idx1.data(UR_Type).toString();
        qDebug().noquote() << "Value   UR_Min                     " << idx1.data(UR_Min).toString();
        qDebug().noquote() << "Value   UR_Max                     " << idx1.data(UR_Max).toString();
        qDebug().noquote() << "Value   UR_Div                     " << idx1.data(UR_Div).toString();
        qDebug().noquote() << "Value   UR_FixedWidth              " << idx1.data(UR_FixedWidth).toString();
        qDebug().noquote() << "Value   UR_Color                   " << idx1.data(UR_Color).toString();
        qDebug().noquote() << "Value   UR_StringList              " << idx1.data(UR_StringList).toString();
        qDebug().noquote() << "Value   UR_QModelIndex             " << idx1.data(UR_QModelIndex).toString();

        qDebug() << " ";

        // iterate children
        if (model->hasChildren(idx0)) {
            diagnosticProperties(idx0);
        }
    }
}
