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
