#include "preferencesdlg.h"
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

PreferencesDlg::PreferencesDlg(QWidget *parent, bool &isSolo, Preferences *pref, QString &css)
                             : QDialog(parent),
                               isSolo(isSolo)
{
    tree = pref;

    setWindowTitle("Winnow Preferences");
    setMinimumSize(470, 500);
    QHBoxLayout* layout = new QHBoxLayout(this);
    layout->addWidget(pref);
    layout->setContentsMargins(0,0,0,0);
    setStyleSheet(css);
    setStyleSheet("QTreeView {border: none}");

    QAction *expandAllAction = new QAction(tr("Expand all"), this);
    expandAllAction->setShortcutVisibleInContextMenu(true);
    expandAllAction->setShortcut(QKeySequence("Ctrl+>"));
    addAction(expandAllAction);
    connect(expandAllAction, &QAction::triggered, this, &PreferencesDlg::expand);

    QAction *collapseAllAction = new QAction(tr("Collapse all"), this);
    collapseAllAction->setShortcutVisibleInContextMenu(true);
    collapseAllAction->setShortcut(QKeySequence("Ctrl+<"));
    addAction(collapseAllAction);
    connect(collapseAllAction, &QAction::triggered, this, &PreferencesDlg::collapse);

    soloAction = new QAction(tr(" Solo "), this);
    soloAction->setShortcutVisibleInContextMenu(true);
    soloAction->setShortcut(QKeySequence("Ctrl+*"));
    soloAction->setCheckable(true);
    addAction(soloAction);
    soloAction->setChecked(isSolo);
    connect(soloAction, &QAction::triggered, this, &PreferencesDlg::solo);

    setContextMenuPolicy(Qt::ActionsContextMenu);
}

void PreferencesDlg::expand()
{
    tree->expandAll();
}

void PreferencesDlg::collapse()
{
    tree->collapseAll();
}

void PreferencesDlg::solo()
{
    tree->setSolo(soloAction->isChecked());
    isSolo = soloAction->isChecked();
}
