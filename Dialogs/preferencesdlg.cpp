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

PreferencesDlg::PreferencesDlg(QWidget *parent,
                               bool &isSolo,
                               Preferences *pref,
                               QString &css)
                             : QDialog(parent),
                               isSolo(isSolo)
{
    tree = pref;

    setWindowTitle("Winnow Preferences");

    QFont fnt = this->font();
    int px = static_cast<int>(G::strFontSize.toInt() * G::ptToPx);
    if (px < 6) px = 6;
    fnt.setPixelSize(px);
    QFontMetrics fm(fnt);
    int w0 = fm.boundingRect("==Incremental amount to load plus more room==").width();
    int w1 = fm.boundingRect("===Next / Previous Image plus===").width();
    int w = pref->width;
    setMinimumSize(w + 10, 600);
//    setMinimumSize(w0 + w1 + 10, 600);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    setStyleSheet(css);
    tree->setStyleSheet(css);
    tree->setHeaderHidden(true);

    layout = new QHBoxLayout(this);
    layout->addWidget(pref);
    layout->setContentsMargins(0,0,0,0);

    expandAllAction = new QAction(tr("Expand all"), this);
    expandAllAction->setShortcutVisibleInContextMenu(true);
//    expandAllAction->setShortcut(QKeySequence("Ctrl+>"));
    addAction(expandAllAction);
    connect(expandAllAction, &QAction::triggered, this, &PreferencesDlg::expand);

    collapseAllAction = new QAction(tr("Collapse all"), this);
    collapseAllAction->setShortcutVisibleInContextMenu(true);
//    collapseAllAction->setShortcut(QKeySequence("Ctrl+<"));
    addAction(collapseAllAction);
    connect(collapseAllAction, &QAction::triggered, this, &PreferencesDlg::collapse);

    soloAction = new QAction(tr(" Solo "), this);
    soloAction->setShortcutVisibleInContextMenu(true);
//    soloAction->setShortcut(QKeySequence("Ctrl+*"));
    soloAction->setCheckable(true);
    addAction(soloAction);
    soloAction->setChecked(isSolo);
    connect(soloAction, &QAction::triggered, this, &PreferencesDlg::solo);
    solo();

    setContextMenuPolicy(Qt::ActionsContextMenu);

//    pref->expandBranch("Metadata panel items");

    #ifdef Q_OS_WIN
        Win::setTitleBarColor(winId(), G::backgroundColor);
    #endif
}

PreferencesDlg::~PreferencesDlg()
{
    delete layout;
    delete expandAllAction;
    delete collapseAllAction;
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
