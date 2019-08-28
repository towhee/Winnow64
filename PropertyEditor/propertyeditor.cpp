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
*/
{
    if (event->button() == Qt::RightButton) return;
    QTreeView::mousePressEvent(event);
    if (event->modifiers() == Qt::NoModifier) {
        QModelIndex idx = indexAt(event->pos());
        if (idx.isValid()) {
            // setCurrentIndex for the value cell (col 1) if user clicked on the caption cell (col 0)
            QModelIndex idxVal = model->index(idx.row(), 1, idx.parent());
            setCurrentIndex(idxVal);

            /* try to expand / collapse if click on any part of the row.  This will occur
            automatically if the user clicks on the decoration, so those clicks have to be
            ignored.  For root items the Qt decoration has been covered by our painting the
            gradient across the row, so any clicks after the indentation amount should be
            acted upon.  For non-root rows, the decoration is after the indentation amount
            and is 14 pixels wide (I believe this is hard coded in Qt).
            */
            QPoint p = event->pos();
//            qDebug() << __FUNCTION__ << p.x();
            if (idx.column() != 0) idx = model->index(idx.row(), 0, idx.parent());
            // if root item any click after indentation should
            if (idx.parent() == QModelIndex()) {
                // root item
                bool wasExpanded = false;
                if (p.x() >= indentation) {
                    if (isSolo) {
                        wasExpanded = isExpanded(idx);
                        collapseAll();
                    }
                    if (wasExpanded) collapse(idx);
                    else {
                        expandRecursively(idx);
                    }
                }
                if (p.x() <= indentation) {
                    if (isSolo) {
                        wasExpanded = !isExpanded(idx);
                        collapseAll();
                    }
                    if (wasExpanded) collapse(idx);
                    else {
                        expandRecursively(idx);
                    }
                }
            }
            else if (p.x() < indentation || p.x() > indentation + 14)
                    isExpanded(idx) ? collapse(idx) : expand(idx);
        }
    }
}

void PropertyEditor::setSolo(bool isSolo)
{
    this->isSolo = isSolo;
}
