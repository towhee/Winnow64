#include "Develop/Properties/scopeheader.h"
#include "Main/dockwidget.h"        // BarBtn
#include "Main/global.h"

#include <QComboBox>
#include <QLabel>
#include <QMenu>
#include <QAction>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QPainter>
#include <QPixmap>
#include <QLinearGradient>
#include <QTimer>

/*
    The Develop dock's scope control -- see scopeheader.h for the shape and the wiring.
*/

/* The scope caption (Global, Mask 1, ...) is yellow so the thing the panel below is
   editing reads apart from the section headers and slider captions under it. */
static const QColor kScopeTextColor(Qt::yellow);

ScopeHeader::ScopeHeader(QWidget *parent) : QWidget(parent)
{
    if (G::isLogger) G::log("ScopeHeader::ScopeHeader");

    QVBoxLayout *outer = new QVBoxLayout(this);
    /* 4 px of dock background ABOVE the bar, so "Edits" is not welded to whatever ends
       directly above it. It goes on the layout rather than the bar's own padding:
       paintEvent fills scopeBar->geometry() with the header gradient, so padding the bar
       would grow the gradient instead of leaving a gap. */
    outer->setContentsMargins(0, 4, 0, 0);
    outer->setSpacing(0);

    buildScopeBar(outer);

    /* Host for the detail widgets, rebuilt by nobody: the details outlive every refresh.
       Translucent for the same reason the bar is -- under the app stylesheet a plain
       QWidget fills its background opaquely, painting over the dock background. */
    rowsContainer = new QWidget(this);
    rowsContainer->setAttribute(Qt::WA_TranslucentBackground);
    rowsLayout = new QVBoxLayout(rowsContainer);
    /* No bottom margin: the tree's first section header butts straight onto the bar's
       block, so the scope control and the sections read as one uninterrupted block. */
    rowsLayout->setContentsMargins(0, 0, 0, 0);
    rowsLayout->setSpacing(0);
    outer->addWidget(rowsContainer);

    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
}

void ScopeHeader::buildScopeBar(QVBoxLayout *outer)
{
    /* "Edits [Global v] [+] [eye] [:]" -- the whole scope control on one line. It stands
       in for a panel header band, so it carries the band's gradient (painted in
       paintEvent) and the trailing [eye] [:] pair, plus a [+] that promotes New mask out
       of the menu it would otherwise be buried in. The combo lists every scope; the pair
       acts on whichever it has selected.
       NO collapse arrow: there is no scope list to hide, and the panel headers below
       (Submasks, Basic, Color, ...) are always visible. */
    scopeBar = new QWidget(this);
    scopeBar->setAttribute(Qt::WA_TranslucentBackground);
    QHBoxLayout *hb = new QHBoxLayout(scopeBar);
    hb->setContentsMargins(0, 3, G::headerBtnRightInset, 3);
    hb->setSpacing(0);

    QLabel *barLabel = new QLabel(tr("Edits"), scopeBar);
    barLabel->setStyleSheet(G::labelCss(G::header2Color, G::strFontSize.toInt()));

    scopeCombo = new QComboBox(scopeBar);
    scopeCombo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    scopeCombo->setToolTip("The scope the settings below belong to:\n"
                           "Global, or one of this image's masks");
    /* The scope name (Global, Subject, Mask 1 ...) is yellow so the thing the panel below
       is editing reads apart from the section headers and slider captions under it. Only
       the text colour is set: every other QComboBox property still comes from
       WidgetCSS::comboBox(), and the disabled rule is repeated here because a widget's
       own stylesheet outranks the application one. */
    scopeCombo->setStyleSheet("QComboBox { color: " + kScopeTextColor.name() + "; }"
                              "QComboBox:disabled { color: " + G::disabledColor.name() + "; }");
    /* activated (not currentIndexChanged): only a USER pick selects a scope. The refill
       in updateScopeBar is blocked as well, but activated never fires for it anyway. */
    connect(scopeCombo, QOverload<int>::of(&QComboBox::activated), this, [this](int idx){
        selectScopeDeferred(names.value(idx));
    });

    barAddBtn = new BarBtn();
    barAddBtn->setToolTip("New mask");
    /* new.png: the same [+] glyph SubmaskList's band uses for "add a submask", so the
       two "add" affordances in the Develop dock read alike. */
    barAddBtn->setIcon(":/images/icon16/new.png", G::iconOpacity);
    barAddBtn->setIconSize(QSize(16, 16));
    connect(barAddBtn, &BarBtn::clicked, this, [this]{
        QTimer::singleShot(0, this, [this]{ emit addScopeRequested(); });
    });

    barEyeBtn = new BarBtn();
    barEyeBtn->setToolTip("Show or hide the selected scope's changes");
    setEyeIcon(barEyeBtn, true);
    barEyeBtn->setIconSize(QSize(16, 16));
    connect(barEyeBtn, &BarBtn::clicked, this, [this]{ toggleActiveEnabled(); });

    barMenuBtn = new BarBtn();
    barMenuBtn->setToolTip("Scope and panel actions");
    /* ellipsis_vertical.png: the vertical "kebab" glyph the whole dock uses for its
       action menus, dimmed by G::iconOpacity so it reads at the same brightness as the
       "?" tip buttons above it. */
    barMenuBtn->setIcon(":/images/icon16/ellipsis_vertical.png", G::iconOpacity);
    barMenuBtn->setIconSize(QSize(16, 16));
    connect(barMenuBtn, &BarBtn::clicked, this, [this]{ showScopeMenu(); });

    hb->addWidget(barLabel);
    hb->addSpacing(G::headerBtnGap);
    hb->addWidget(scopeCombo, 1);
    hb->addSpacing(G::headerBtnGap);
    hb->addWidget(barAddBtn);
    hb->addSpacing(G::headerBtnGap);
    hb->addWidget(barEyeBtn);
    hb->addSpacing(G::headerBtnGap);
    hb->addWidget(barMenuBtn);
    outer->addWidget(scopeBar);
}

void ScopeHeader::updateScopeBar()
{
    /* Refill from the last setScopeRows. Blocked: setCurrentIndex must not read back as a
       user pick. */
    if (!scopeCombo) return;
    const QSignalBlocker block(scopeCombo);
    scopeCombo->clear();
    scopeCombo->addItems(names);
    scopeCombo->setCurrentIndex(activeIndex);
    setEyeIcon(barEyeBtn, enabledStates.value(activeIndex, true));
}

void ScopeHeader::paintEvent(QPaintEvent *)
{
    /* The property-header gradient (backgroundShade +5 -> -15) behind the scope bar,
       which stands in for a panel header band. Everything below sits on the dock
       background.
       NO containment rail and NO panel separator: the bar and the editor below it are
       ONE BLOCK -- there is no list of scopes for a rail to pick the editor's owner out
       of, and a rule under the bar would cut the block in two. */
    if (!scopeBar || !scopeBar->isVisible()) return;
    QPainter p(this);
    const int a = G::backgroundShade + 5;
    const int b = G::backgroundShade - 15;
    const QRect r = scopeBar->geometry();
    QLinearGradient g(0, r.top(), 0, r.bottom());
    g.setColorAt(0, QColor(a, a, a));
    g.setColorAt(1, QColor(b, b, b));
    p.fillRect(r, g);
}

void ScopeHeader::setScopes(const QStringList &n, int currentIndex)
{
    /* Convenience contract (default callers): all scopes enabled, 0 = Global. */
    QVector<ScopeRowInfo> rows;
    rows.reserve(n.size());
    for (int i = 0; i < n.size(); ++i) {
        ScopeRowInfo r;
        r.name = n.at(i);
        r.isGlobal = (i == 0);
        r.enabled = true;
        rows.append(r);
    }
    setScopeRows(rows, currentIndex);
}

void ScopeHeader::setScopeRows(const QVector<ScopeRowInfo> &rows, int active)
{
    activeIndex = (active >= 0 && active < rows.size()) ? active : 0;
    globalActive  = (activeIndex == 0);
    names.clear();
    enabledStates.clear();
    for (const ScopeRowInfo &r : rows) {
        names << r.name;
        enabledStates << r.enabled;
    }
    updateScopeBar();
}

void ScopeHeader::setRowDetail(QWidget *detail, int slot)
{
    if (slot < 0 || slot >= RowDetailSlotCount) return;
    if (rowDetail[slot] == detail) return;
    if (detailWrap[slot]) {
        /* Hand any previous detail back before the wrapper goes: re-parent it to
           rowsContainer, NOT nullptr -- a parentless QTreeView would briefly become a
           top-level window and flash on screen. */
        if (rowDetail[slot]) rowDetail[slot]->setParent(rowsContainer);
        delete detailWrap[slot];
        detailWrap[slot] = nullptr;
    }
    rowDetail[slot] = detail;
    if (!detail) return;

    /* No indent: the details start at the panel edge, so the adjustment tree's left edge
       -- and any rule it paints -- lines up with this widget's. The visible nesting comes
       from the section-header indent inside the tree. The wrapper is kept even so: it
       owns the slot's position in the layout, and detaching a detail must not disturb
       the other slot. */
    detailWrap[slot] = new QWidget(rowsContainer);
    detailWrap[slot]->setAttribute(Qt::WA_TranslucentBackground);
    QVBoxLayout *dl = new QVBoxLayout(detailWrap[slot]);
    dl->setContentsMargins(0, 0, 0, 0);
    dl->setSpacing(0);
    dl->addWidget(detail);
    /* Slot order IS layout order: MaskDetail (0) is bound before EditsDetail (1), and a
       re-bind reuses the position its predecessor held. */
    rowsLayout->insertWidget(qMin(slot, rowsLayout->count()), detailWrap[slot]);
    detailWrap[slot]->show();
}

void ScopeHeader::selectScopeDeferred(const QString &name)
{
    /* Defer a tick: scopeSelected loops back through DevelopProperties -> setScopeRows,
       which refills the combo this pick came from. */
    QTimer::singleShot(0, this, [this, name]{ emit scopeSelected(name); });
}

void ScopeHeader::setEyeIcon(BarBtn *b, bool shown)
{
    if (!b) return;
    b->setIcon(shown ? ":/images/icon16/eye.png" : ":/images/icon16/eye_off.png",
               G::iconOpacity);
}

void ScopeHeader::toggleActiveEnabled()
{
    /* Flip the ACTIVE scope's enabled flag and let the owner push the new state back
       through setScopeRows (which re-syncs the icon). Deferred like every other emission
       here: the handler loops back through setScopeRows. */
    const int index = activeIndex;
    const bool on = enabledStates.value(index, true);
    QTimer::singleShot(0, this,
                       [this, index, on]{ emit scopeEnabledToggled(index, !on); });
}

void ScopeHeader::addHelpAction(QMenu *menu)
{
    /* "Edits help", last item in the menu -- the same place each section band in the tree
       below carries its own help. */
    if (!menu) return;
    menu->addSeparator();
    connect(menu->addAction(tr("Edits help")), &QAction::triggered,
            this, [this]{ emit helpRequested(); });
}

void ScopeHeader::showScopeMenu()
{
    if (G::isLogger) G::log("ScopeHeader::showScopeMenu");

    /* The bar's [:] is the ONLY menu in the panel, so it carries BOTH the active scope's
       actions and the panel-wide ones ("Reset all edits"). Read the choice as an int code
       while the menu is alive, then fire everything on the next tick (a select refills
       the combo this menu hangs off -- must not happen inside its own click handler). */
    enum { AddMask = 1, Reset, Remove, Rename, NewScope };
    const int index = activeIndex;
    const QString name = names.value(index);
    const bool isGlobal = (index == 0);

    QMenu menu(this);
    if (isGlobal) {
        /* Global applies to the whole image: it has no mask, cannot be renamed and cannot
           be deleted, so its menu carries only what it can actually do. */
        menu.addAction(tr("New mask\tN"))->setData(NewScope);
        menu.addSeparator();
        menu.addAction(tr("Reset %1").arg(name))->setData(Reset);
    }
    else {
        QAction *aMask   = menu.addAction(tr("Modify %1 mask\tM").arg(name));
        aMask->setData(AddMask);
        menu.addSeparator();
        menu.addAction(tr("Reset %1").arg(name))->setData(Reset);
        menu.addAction(tr("Rename %1").arg(name))->setData(Rename);
        menu.addSeparator();
        menu.addAction(tr("Delete %1").arg(name))->setData(Remove);
    }

    /* Wipes the whole recipe (every scope, the geometry and the spots), the history and
       the sidecar record, on every selected image. DevelopProperties confirms first.
       It fires through its own connect (no data code), so the per-scope dispatch below
       simply ignores it. */
    menu.addSeparator();
    connect(menu.addAction(tr("Reset all edits")), &QAction::triggered,
            this, [this]{ emit resetAllEditsRequested(); });
    addHelpAction(&menu);

    QAction *chosen = menu.exec(QCursor::pos());
    const int code = chosen ? chosen->data().toInt() : 0;
    if (code == 0) return;

    QTimer::singleShot(0, this, [this, name, code]{
        emit scopeSelected(name);            // make this the active scope first
        /* The select can be REFUSED -- a submask still pending on the scope being left
           prompts, and "Cancel" keeps the old scope active (DevelopProperties re-asserts
           the rows, so currentScopeName reports the truth). Running the action anyway
           would apply it to the wrong scope: "Remove Mask 2" would remove Mask 1. */
        if (currentScopeName() != name) return;
        switch (code) {
            case AddMask:  emit addMaskRequested();     break;
            case Reset:    emit resetScopeRequested();  break;
            case Remove:   emit removeScopeRequested();  break;
            case Rename:   emit renameRequested();      break;
            case NewScope: emit addScopeRequested();    break;
        }
    });
}

void ScopeHeader::setCollapsed(bool c)
{
    /* State only -- the scope bar always stays visible. Hiding the adjustments below
       belongs to DevelopProperties::setTreeCollapsed; DevelopProperties drives this from
       Collapse-all / Solo / section-expand, so hiding anything here would take the scope
       control down with them. */
    collapsed = c;
}

QString ScopeHeader::currentScopeName() const
{
    return (activeIndex >= 0 && activeIndex < names.size()) ? names.at(activeIndex) : QString();
}
