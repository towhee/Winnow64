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

    /* Reset buttons along the bottom, flat text on the dialog background so they read
       as quiet commands, not as part of the settings above.  See MW::requestSettingsReset. */
    const QString flatBtnStyle =
        "QPushButton {background: transparent; border: none; padding: 0px; min-width: 0;"
            "color: " + G::secondaryTextColor.name() + ";}"
        "QPushButton:hover {color: " + G::textColor.name() + ";}";
    factoryResetBtn = new QPushButton("Factory reset", this);
    factoryResetBtn->setStyleSheet(flatBtnStyle);
    factoryResetBtn->setCursor(Qt::PointingHandCursor);
    factoryResetBtn->setToolTip(
        "Return every preference, workspace, panel layout and shortcut to the\n"
        "settings a new installation starts with, and restart Winnow.\n"
        "Keywords, develop presets, places, the catalog and caches are kept.");
    connect(factoryResetBtn, &QPushButton::clicked, this, [this]() {
        emit resetSettingsRequested("reset");
    });

    testFactoryBtn = new QPushButton("Test factory settings", this);
    testFactoryBtn->setStyleSheet(flatBtnStyle);
    testFactoryBtn->setCursor(Qt::PointingHandCursor);
    connect(testFactoryBtn, &QPushButton::clicked, this, [this]() {
        emit resetSettingsRequested(isFactoryTest ? "restore" : "test");
    });

    QHBoxLayout *btnLayout = new QHBoxLayout;
    btnLayout->setContentsMargins(12,8,12,10);
    btnLayout->addWidget(factoryResetBtn);
    btnLayout->addStretch();
    btnLayout->addWidget(testFactoryBtn);

    layout = new QVBoxLayout(this);
    layout->addWidget(pref);
    layout->addLayout(btnLayout);
    layout->setContentsMargins(0,0,0,0);
    layout->setSpacing(0);
    syncResetButtons(false);

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

    pref->resizeColumns();
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

void PreferencesDlg::resizeForFontChange()
{
/*
    When the global font size changes, PropertyEditor::resizeColumns recomputes
    the caption (column 1) and value (column 2) widths from the font metrics.
    The value column is the stretched last section, so its visible width is
    (dialog width - caption width); the caption:value proportion is only
    preserved when the dialog width equals the tree's natural width (caption +
    value). Resize the dialog to that width, scale the height by the same factor
    so the dialog keeps its proportions, and clamp both to the screen.
*/
    QScreen *scr = screen() ? screen() : QGuiApplication::primaryScreen();
    if (!scr) return;
    const QRect avail = scr->availableGeometry();

    int targetW = qMin(tree->width + 10, avail.width());    // +10 matches construction
    qreal factor = width() > 0 ? static_cast<qreal>(targetW) / width() : 1.0;
    int minH = qMin(600, avail.height());                   // 600 = construction floor
    int targetH = qBound(minH, qRound(height() * factor), avail.height());

    // Construction pinned the minimum width to the initial font's natural width,
    // which would otherwise block shrinking when the font gets smaller.
    setMinimumWidth(targetW);
    resize(targetW, targetH);

    // Nudge back into view if the resize pushed the dialog off-screen (it grows
    // from the top-left, so the right/bottom edge can spill past the screen).
    // Uses frameGeometry so window-manager decorations are accounted for.
    QRect fg = frameGeometry();
    QPoint p = pos();
    if (fg.right()  > avail.right())  p.rx() -= fg.right()  - avail.right();
    if (fg.bottom() > avail.bottom()) p.ry() -= fg.bottom() - avail.bottom();
    if (p.x() < avail.left()) p.setX(avail.left());
    if (p.y() < avail.top())  p.setY(avail.top());
    if (p != pos()) move(p);
}

void PreferencesDlg::syncResetButtons(bool testing)
{
    isFactoryTest = testing;
    factoryResetBtn->setVisible(!testing);
    if (testing) {
        testFactoryBtn->setText("Restore my settings");
        testFactoryBtn->setToolTip("End the factory settings test: restart Winnow with the\n"
                                   "settings it had before the test.  Changes made while\n"
                                   "testing are discarded.");
    }
    else {
        testFactoryBtn->setText("Test factory settings");
        testFactoryBtn->setToolTip("Restart Winnow with the settings a new user sees.  Your\n"
                                   "own settings are set aside; \"Restore my settings\"\n"
                                   "brings them back exactly as they are now.");
    }
    /* G::isRory is not a setting, so it is off during a test: Restore always shows. */
    testFactoryBtn->setVisible(testing || G::isRory);
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
