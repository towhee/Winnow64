#include "Develop/Properties/scopeheaderlab.h"
#include "Main/dockwidget.h"        // BarBtn
#include "Main/global.h"

#include <QComboBox>
#include <QLabel>
#include <QMenu>
#include <QAction>
#include <QActionGroup>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QPainter>
#include <QPixmap>
#include <QLinearGradient>
#include <QMouseEvent>
#include <QTimer>

/*
    EXPERIMENTAL ScopeHeaderLab (see scopeheaderlab.h). A vertical list of scope rows in a
    gradient-headed band, wired through the ScopeHeaderBase contract so DevelopProperties
    drives it unchanged. Edit freely; swap over scopeheader.* once it proves out.
*/

/* Scope captions (Global, Mask 1, ...) are yellow so the scope names read apart from the
   section headers and slider captions in the same panel. The ACTIVE scope takes the full
   colour; inactive rows take G::dimmed() of the same hue, keeping the active/inactive
   contrast the previous white/grey pair carried. */
static const QColor kScopeTextColor(Qt::yellow);

ScopeHeaderLab::ScopeHeaderLab(QWidget *parent) : ScopeHeaderBase(parent)
{
    if (G::isLogger) G::log("ScopeHeaderLab::ScopeHeaderLab");

    /* ellipsis_vertical.png: the vertical "kebab" glyph for the panel and per-row scope
       action menus. Drawn gray 176 like questionmark.png and dimmed by the same
       G::iconOpacity (via BarBtn::setIcon(path, opacity)) so it reads at the identical
       brightness as the "?" tip buttons above it. */

    QVBoxLayout *outer = new QVBoxLayout(this);
    /* 4 px of dock background ABOVE the band, so "Edits" is not welded to whatever ends
       directly above it. It goes on the layout rather than the band's own padding: the
       band is the click target for collapse and paintEvent fills band->geometry() with
       the header gradient, so padding it would grow both the hit area and the gradient
       instead of leaving a gap. */
    outer->setContentsMargins(0, 4, 0, 0);
    outer->setSpacing(0);

    /* Header band: "Scopes" caption + a trailing [v] panel menu. Transparent background
       so ScopeHeaderLab::paintEvent can draw the property-header gradient behind it. */
    headerBand = new QWidget(this);
    headerBand->setAttribute(Qt::WA_TranslucentBackground);
    headerBand->setCursor(Qt::PointingHandCursor);
    headerBand->installEventFilter(this);        // a header click toggles collapse
    QHBoxLayout *hb = new QHBoxLayout(headerBand);
    hb->setContentsMargins(0, 3, G::headerBtnRightInset, 3);
    hb->setSpacing(0);
    /* Collapse arrow (branch glyph) in an icon-wide button, matching the tree's section
       headers + the Raw panel: click it (or the caption) to hide/show the list. The
       caption follows G::decorationTitleGap clear of the arrow. */
    collapseBtn = new BarBtn();
    collapseBtn->setToolTip("Hide or show the scope list");
    collapseBtn->setIconSize(QSize(9, 9));
    collapseBtn->setFixedSize(9, 16);
    collapseBtn->setStyleSheet("QToolButton { border: none; padding: 0; background: transparent; }");
    connect(collapseBtn, &BarBtn::clicked, this, [this]{ toggleListCollapsed(); });
    titleLabel = new QLabel(tr("Edits"), headerBand);
    titleLabel->setStyleSheet(G::labelCss(G::header2Color, G::strFontSize.toInt()));
    /* Band eye: show/hide the ACTIVE scope's changes -- the same flag its row eye
       carries, so the two always read alike. Every band and row in this panel ends with
       the same pair, eye then menu. */
    bandEyeBtn = new BarBtn();
    bandEyeBtn->setToolTip("Show or hide the selected scope's changes");
    setEyeIcon(bandEyeBtn, true);
    bandEyeBtn->setIconSize(QSize(16, 16));
    connect(bandEyeBtn, &BarBtn::clicked, this, [this]{ toggleActiveEnabled(); });
    panelMenuBtn = new BarBtn();
    panelMenuBtn->setToolTip("Scope actions (new mask)");
    panelMenuBtn->setIcon(":/images/icon16/ellipsis_vertical.png", G::iconOpacity);
    panelMenuBtn->setIconSize(QSize(16, 16));
    connect(panelMenuBtn, &BarBtn::clicked, this, [this]{ showPanelMenu(); });
    hb->addWidget(collapseBtn);
    hb->addSpacing(G::decorationTitleGap);
    hb->addWidget(titleLabel);
    hb->addStretch(1);
    hb->addWidget(bandEyeBtn);
    hb->addSpacing(G::headerBtnGap);      // the gap every header/row's pair carries
    hb->addWidget(panelMenuBtn);
    outer->addWidget(headerBand);
    updateListCollapseIcon();

    /* MINIMAL layout: the one-line scope bar that stands in for the header band AND the
       rows. Built here so a layout flip is a show/hide, never a rebuild. */
    buildScopeBar(outer);

    /* Rows container: one row widget per scope, rebuilt by setScopeRows. Translucent for
       the same reason headerBand is: under the app stylesheet a plain QWidget fills its
       background opaquely, which painted over the containment rail this widget draws
       behind its children (probe-confirmed -- the rail was drawn but invisible). */
    rowsContainer = new QWidget(this);
    rowsContainer->setAttribute(Qt::WA_TranslucentBackground);
    rowsLayout = new QVBoxLayout(rowsContainer);
    /* No bottom margin: the tree's first section header butts straight onto the last
       scope row, so the scope list and the sections read as one uninterrupted block. */
    rowsLayout->setContentsMargins(0, 0, 0, 0);
    rowsLayout->setSpacing(0);
    outer->addWidget(rowsContainer);

    /* FLAT layout only (G::developEditsLayout): the band that heads the editor region
       below the list. It is painted in G::selectionColor with a white caption -- the SAME
       band the selected row carries -- so the eye pairs "the highlighted row up there"
       with "the sliders down here" by colour, which is the binding the flat layout gives
       up by separating them. The rail runs the same colour through both.
       Like every other band and row in this panel it ends with [eye] then [:], acting on
       the ACTIVE scope (the same targets the header band's pair uses). */
    editorBand = new QWidget(rowsContainer);
    editorBand->setObjectName("scopeRow");
    editorBand->setStyleSheet(QString("QWidget#scopeRow { background: %1; }"
                                      "QWidget#scopeRow:disabled { background: %2; }")
                                  .arg(G::selectionColor.name(),
                                       G::dimmed(G::selectionColor).name()));
    QHBoxLayout *eb = new QHBoxLayout(editorBand);
    eb->setContentsMargins(kDetailIndent, 2, G::headerBtnRightInset, 2);
    eb->setSpacing(G::headerBtnGap);
    editorBandLabel = new QLabel(editorBand);
    editorBandLabel->setStyleSheet(G::labelCss(kScopeTextColor, G::strFontSize.toInt()));
    editorEyeBtn = new BarBtn();
    editorEyeBtn->setToolTip("Show or hide the selected scope's changes");
    setEyeIcon(editorEyeBtn, true);
    editorEyeBtn->setIconSize(QSize(16, 16));
    connect(editorEyeBtn, &BarBtn::clicked, this, [this]{ toggleActiveEnabled(); });
    editorMenuBtn = new BarBtn();
    editorMenuBtn->setToolTip("Scope actions");
    editorMenuBtn->setIcon(":/images/icon16/ellipsis_vertical.png", G::iconOpacity);
    editorMenuBtn->setIconSize(QSize(16, 16));
    connect(editorMenuBtn, &BarBtn::clicked, this, [this]{
        showRowMenu(activeIndex, names.value(activeIndex), activeIndex == 0);
    });
    eb->addWidget(editorBandLabel);
    eb->addStretch(1);
    eb->addWidget(editorEyeBtn);
    eb->addWidget(editorMenuBtn);
    editorBand->hide();                  // rebuild() shows it when the layout is flat
    rowsLayout->addWidget(editorBand);   // rebuild() fixes the position

    applyLayoutMode();                   // show the bar or the header band, not both
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
}

void ScopeHeaderLab::buildScopeBar(QVBoxLayout *outer)
{
    /* "Scope: [Global v] [+] [eye] [:]" -- the whole scope control on one line. In the
       minimal layout this REPLACES the header band, so it carries the band's gradient
       (painted in paintEvent) and the trailing [eye] [:] pair, plus a [+] that promotes
       New mask out of the menu it is buried in elsewhere. The combo lists every scope;
       the pair acts on whichever it has selected.
       NO collapse arrow: this layout has no scope list to hide, and the panel headers
       below it (Submasks, Basic, Color, ...) are always visible. */
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
        selectRowDeferred(names.value(idx));
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
    barMenuBtn->setIcon(":/images/icon16/ellipsis_vertical.png", G::iconOpacity);
    barMenuBtn->setIconSize(QSize(16, 16));
    connect(barMenuBtn, &BarBtn::clicked, this, [this]{
        showRowMenu(activeIndex, names.value(activeIndex), activeIndex == 0);
    });

    hb->addWidget(barLabel);
    hb->addSpacing(G::headerBtnGap);
    hb->addWidget(scopeCombo, 1);
    hb->addSpacing(G::headerBtnGap);
    hb->addWidget(barAddBtn);
    hb->addSpacing(G::headerBtnGap);
    hb->addWidget(barEyeBtn);
    hb->addSpacing(G::headerBtnGap);
    hb->addWidget(barMenuBtn);
    scopeBar->hide();
    outer->addWidget(scopeBar);
}

void ScopeHeaderLab::updateScopeBar()
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

void ScopeHeaderLab::applyLayoutMode()
{
    /* Exactly one scope control is up: the minimal bar, or the header band + rows. */
    const bool minimal = (G::developEditsLayout == G::EditsLayout::Minimal);
    if (headerBand) headerBand->setVisible(!minimal);
    if (scopeBar)   scopeBar->setVisible(minimal);
    /* Minimal has no collapse control of its own (the header band, which carries the
       one arrow, is down), so a listCollapsed left over from another layout would hide
       the editor with no way to get it back. Expand on entry. */
    if (minimal && listCollapsed) {
        listCollapsed = false;
        if (rowsContainer) rowsContainer->show();
    }
    updateListCollapseIcon();
}

void ScopeHeaderLab::paintEvent(QPaintEvent *)
{
    /* The property-header gradient (backgroundShade +5 -> -15), behind whichever band is
       up -- the "Edits" header, or the minimal layout's scope bar, which stands in for
       it. Everything below sits on the dock background. */
    QWidget *band = (G::developEditsLayout == G::EditsLayout::Minimal) ? scopeBar
                                                                      : headerBand;
    if (!band || !band->isVisible()) return;
    QPainter p(this);
    const int a = G::backgroundShade + 5;
    const int b = G::backgroundShade - 15;
    const QRect r = band->geometry();
    QLinearGradient g(0, r.top(), 0, r.bottom());
    g.setColorAt(0, QColor(a, a, a));
    g.setColorAt(1, QColor(b, b, b));
    p.fillRect(r, g);

    /* Containment rail (G::scopeRailX/W): brackets the SELECTED scope only -- from the
       top of its row down past the details nested under it (the Mask panel and the
       adjustment tree), so the rail says "everything in here belongs to this scope". The
       unselected rows carry no rail: they are just names until they are picked.
       The rail is G::selectionColor, the same fill as the selected row's band, so the
       stretch crossing that row is invisible: the selection reads as a gap in the rail
       rather than as its origin. The nested tree paints its own segment over its
       viewport (DevelopProperties::paintEvent), at the same x and colour, so the two
       read as one line. */
    /* NO panel separator along the bottom edge (unlike the other Develop panels): the
       scope row and its nested details are ONE BLOCK, joined by the rail. */
    if (G::scopeRailW > 0) {
        /* Dimmed while the panel is greyed, in step with the active row's band (which
           carries the same :disabled colour) so the two still read as one line. */
        const QColor rail = isEnabled() ? G::selectionColor : G::dimmed(G::selectionColor);
        auto fill = [&p, &rail](const QRect &r) {
            if (!r.isEmpty())
                p.fillRect(G::scopeRailX, r.top(), G::scopeRailW, r.height(), rail);
        };
        if (G::developEditsLayout == G::EditsLayout::Flat) {
            /* Two pieces: the selected row, then the editor region under the list. The
               unselected rows between them carry no rail, so the break IS the statement
               -- these two blocks belong together and the rows in between do not. */
            fill(activeRowRect());
            fill(editorBlockRect());
        }
        else if (G::developEditsLayout == G::EditsLayout::Nested) {
            fill(activeBlockRect());
        }
        /* Minimal: NO rail. It exists to say which ROWS the editor belongs to, and there
           are no rows -- the bar and the editor are adjacent and alone. */
    }
}

QRect ScopeHeaderLab::activeBlockRect() const
{
    /* The selected scope's block in THIS widget's coordinates: its row, plus every detail
       nested under it. The rows live in rowsContainer, so their geometry is offset by the
       container's own position. Empty while the list is collapsed (no row to bracket). */
    if (!rowsContainer || !rowsContainer->isVisible() || !activeRow) return QRect();
    const int dy = rowsContainer->y();
    int top    = activeRow->y() + dy;
    int bottom = top + activeRow->height();
    for (QWidget *d : detailWrap) {
        if (!d || !d->isVisible()) continue;
        bottom = qMax(bottom, d->y() + dy + d->height());
    }
    return QRect(0, top, width(), bottom - top);
}

QRect ScopeHeaderLab::activeRowRect() const
{
    /* Flat layout: the selected row alone -- its editor is elsewhere. */
    if (!rowsContainer || !rowsContainer->isVisible() || !activeRow) return QRect();
    const int top = activeRow->y() + rowsContainer->y();
    return QRect(0, top, width(), activeRow->height());
}

QRect ScopeHeaderLab::editorBlockRect() const
{
    /* Flat layout: the editor region -- its band plus every visible detail under it.
       Empty while the list is collapsed (the whole rowsContainer goes with it) or before
       the band has been placed. */
    if (!rowsContainer || !rowsContainer->isVisible()) return QRect();
    if (!editorBand || !editorBand->isVisible()) return QRect();
    const int dy = rowsContainer->y();
    int top    = editorBand->y() + dy;
    int bottom = top + editorBand->height();
    for (QWidget *d : detailWrap) {
        if (!d || !d->isVisible()) continue;
        bottom = qMax(bottom, d->y() + dy + d->height());
    }
    return QRect(0, top, width(), bottom - top);
}

void ScopeHeaderLab::applyDetailIndent(int slot)
{
    /* The flat editor region starts at the panel edge (indent 0): there is no scope row
       directly above it to line up under, and its rail then continues the tree's at the
       same x. Nested keeps the caller's indent, which puts the detail under the scope
       NAME. */
    if (slot < 0 || slot >= RowDetailSlotCount || !detailWrap[slot]) return;
    if (QLayout *l = detailWrap[slot]->layout()) {
        const int indent = (G::developEditsLayout == G::EditsLayout::Nested)
                               ? detailIndent[slot] : 0;
        l->setContentsMargins(indent, 0, 0, 0);
    }
}

void ScopeHeaderLab::updateEditorBand()
{
    /* Flat only: name the scope the sliders below belong to, and mirror its eye. */
    if (!editorBand) return;
    /* Flat only: nested has the row above it saying the same thing, and minimal has the
       scope bar. */
    if (G::developEditsLayout != G::EditsLayout::Flat) { editorBand->hide(); return; }
    if (editorBandLabel)
        editorBandLabel->setText(tr("Editing: %1").arg(names.value(activeIndex)));
    setEyeIcon(editorEyeBtn, enabledStates.value(activeIndex, true));
    editorBand->show();
}

void ScopeHeaderLab::setScopes(const QStringList &n, int currentIndex)
{
    /* Legacy contract (dropdown path / default callers): all scopes enabled, 0 = Global. */
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

void ScopeHeaderLab::setScopeRows(const QVector<ScopeRowInfo> &rows, int active)
{
    activeIndex = (active >= 0 && active < rows.size()) ? active : 0;
    globalActive  = (activeIndex == 0);
    names.clear();
    enabledStates.clear();
    for (const ScopeRowInfo &r : rows) {
        names << r.name;
        enabledStates << r.enabled;
    }
    rebuild(rows, activeIndex);
    updateBandEyeIcon();
}

void ScopeHeaderLab::setRowDetail(QWidget *detail, int slot, int indent)
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

    /* Indent (slot's choice) so a NESTED detail reads as belonging to the scope above it:
       the MaskPanel lines up under the scope NAME (kDetailIndent), while the adjustment
       tree takes indent 0 so its containment rail continues this widget's at the same x.
       Remembered per slot: the flat layout overrides it with 0, and a live layout flip
       has to be able to put it back. */
    detailIndent[slot] = indent;
    detailWrap[slot] = new QWidget(rowsContainer);
    detailWrap[slot]->setAttribute(Qt::WA_TranslucentBackground);
    QVBoxLayout *dl = new QVBoxLayout(detailWrap[slot]);
    dl->setContentsMargins(indent, 0, 0, 0);
    dl->setSpacing(0);
    dl->addWidget(detail);
    applyDetailIndent(slot);
    rowsLayout->addWidget(detailWrap[slot]);      // rebuild() fixes the position
}

void ScopeHeaderLab::rebuild(const QVector<ScopeRowInfo> &rows, int active)
{
    /* Drop the old rows. deleteLater, not delete: a rebuild can be triggered from inside
       a row child's own signal (the show/hide checkbox emits scopeEnabledToggled inline),
       so the row must outlive the handler it is running in (its eye and menu emissions
       are deferred a tick for the same reason). But a widget taken out of a
       layout KEEPS its parent, geometry and visibility, so until the deferred delete runs
       it still paints over the rebuilt list -- that was the stale "Mask 1 selected" band
       and the orphan "Mask 2" row floating in the tree. hide() it as it comes out, and
       drop its scopeName so a stray click cannot re-select a dead row.
       The nested details are NOT ours to delete: takeAt removes them from the layout
       WITHOUT re-parenting, so they stay children of rowsContainer and are simply
       re-inserted below. Deliberately no setParent round trip for them -- re-parenting a
       QTreeView full of setIndexWidget editors on every scope switch is the expensive,
       flicker-prone path. */
    auto isDetail = [this](QWidget *w) {
        if (w && w == editorBand) return true;      // ours, and outlives every rebuild
        for (QWidget *d : detailWrap) if (d && d == w) return true;
        return false;
    };
    while (QLayoutItem *it = rowsLayout->takeAt(0)) {
        QWidget *w = it->widget();
        if (w && !isDetail(w)) {
            w->hide();
            w->setProperty("scopeName", QVariant());
            w->deleteLater();
        }
        delete it;
    }
    applyLayoutMode();                    // a live layout switch lands here first
    activeRow = nullptr;
    /* MINIMAL builds no rows at all -- the scope bar's combo IS the list -- so the editor
       is the only thing in the container. */
    const G::EditsLayout layout = G::developEditsLayout;
    if (layout != G::EditsLayout::Minimal) {
        for (int i = 0; i < rows.size(); ++i) {
            QWidget *row = makeRow(i, rows.at(i), i == active);
            if (i == active) activeRow = row;   // the rail brackets this row's block
            rowsLayout->addWidget(row);
        }
    }

    /* Re-insert the editor, in slot order. FLAT: after every row, headed by its band, so
       the scope list stays whole and the sliders keep the same height whatever is
       selected. NESTED: directly beneath the ACTIVE row, so the editor moves with the
       selection. MINIMAL: it is all there is. Their own visibility is the owner's
       business (DevelopProperties hides the MaskPanel on Global), so the wrappers always
       show: an empty wrapper is 0 high. */
    int pos = (layout == G::EditsLayout::Nested)
                  ? qBound(0, active + 1, rowsLayout->count())
                  : rowsLayout->count();
    if (editorBand) {
        /* Always kept in the layout, even where it is hidden (0 high), so a live layout
           flip is one show() rather than a re-parent. */
        rowsLayout->insertWidget(pos++, editorBand);
        updateEditorBand();               // shows it for Flat, hides it otherwise
    }
    for (int slot = 0; slot < RowDetailSlotCount; ++slot) {
        QWidget *d = detailWrap[slot];
        if (!d) continue;
        applyDetailIndent(slot);          // the layout decides the inset
        rowsLayout->insertWidget(pos++, d);
        d->show();
    }
    updateScopeBar();                     // minimal: refill the combo + sync its eye
}

QWidget *ScopeHeaderLab::makeRow(int index, const ScopeRowInfo &r, bool active)
{
    QWidget *row = new QWidget(rowsContainer);
    row->setProperty("scopeName", r.name);      // read back by the row-body click filter
    row->installEventFilter(this);
    /* Active row: a selectionColor band + white caption; others transparent -- and
       genuinely transparent, so the containment rail painted behind them shows through
       (see rowsContainer). The active row's band is the rail's own colour, so the rail
       simply merges into it. */
    if (active) {
        /* Object-name selector so the rule cannot leak onto the row's children, and a
           MUTED band when the panel is greyed -- the selection stays legible without
           looking live. */
        row->setObjectName("scopeRow");
        row->setStyleSheet(QString("QWidget#scopeRow { background: %1; }"
                                   "QWidget#scopeRow:disabled { background: %2; }")
                               .arg(G::selectionColor.name(),
                                    G::dimmed(G::selectionColor).name()));
    }
    else {
        row->setAttribute(Qt::WA_TranslucentBackground);
    }

    QHBoxLayout *hb = new QHBoxLayout(row);
    /* The caption starts at kDetailIndent, the same inset the details nested under this
       row use, so a scope name and its Mask panel / section headers share one left
       edge. */
    hb->setContentsMargins(kDetailIndent, 2, G::headerBtnRightInset, 2);
    hb->setSpacing(G::headerBtnGap);

    QLabel *name = new QLabel(r.name, row);
    name->setStyleSheet(G::labelCss(active ? kScopeTextColor : G::dimmed(kScopeTextColor),
                                    G::strFontSize.toInt()));
    hb->addWidget(name);
    hb->addStretch(1);

    /* Every scope -- Global included -- ends with the same trailing pair: an eye that
       toggles EditScope::enabled (the compositor skips a disabled scope, so Global off =>
       the image renders with no develop adjustments), then the actions menu. */
    BarBtn *eyeBtn = new BarBtn();
    eyeBtn->setToolTip("Show or hide this scope's changes");
    setEyeIcon(eyeBtn, r.enabled);
    eyeBtn->setIconSize(QSize(16, 16));
    const bool on = r.enabled;
    connect(eyeBtn, &BarBtn::clicked, this, [this, index, on]{
        /* Deferred: the toggle loops back through setScopeRows, which deletes this very
           button (the checkbox this replaced could emit inline only because the row
           survived on deleteLater). */
        QTimer::singleShot(0, this, [this, index, on]{
            emit scopeEnabledToggled(index, !on);
        });
    });
    hb->addWidget(eyeBtn);

    BarBtn *menuBtn = new BarBtn();
    menuBtn->setToolTip(r.isGlobal
                            ? "Global actions (new mask, reset)"
                            : "Scope actions (add to mask, reset, remove, rename)");
    menuBtn->setIcon(":/images/icon16/ellipsis_vertical.png", G::iconOpacity);
    menuBtn->setIconSize(QSize(16, 16));
    const QString nm = r.name;
    const bool isGlobal = r.isGlobal;
    connect(menuBtn, &BarBtn::clicked, this,
            [this, index, nm, isGlobal]{ showRowMenu(index, nm, isGlobal); });
    hb->addWidget(menuBtn);

    return row;
}

bool ScopeHeaderLab::eventFilter(QObject *watched, QEvent *event)
{
    /* A single left click anywhere on the header band (caption or empty area) toggles the
       list collapse; the arrow + eye + [v] buttons consume their own clicks. A click on a
       row body (incl. its name label, which ignores the press so it propagates up)
       selects that scope; eye / menu-button clicks are consumed by those widgets.
       MouseButtonDblClick is included because Qt sends it INSTEAD of the second press:
       without it a double click would toggle once (leaving the list in the opposite
       state) rather than toggling twice back to where it started. */
    if ((event->type() == QEvent::MouseButtonPress ||
         event->type() == QEvent::MouseButtonDblClick) && watched == headerBand) {
        QMouseEvent *me = static_cast<QMouseEvent *>(event);
        if (me->button() == Qt::LeftButton) { toggleListCollapsed(); return true; }
    }
    else if (event->type() == QEvent::MouseButtonRelease) {
        QWidget *w = qobject_cast<QWidget *>(watched);
        QMouseEvent *me = static_cast<QMouseEvent *>(event);
        if (w && me->button() == Qt::LeftButton) {
            const QString nm = w->property("scopeName").toString();
            if (!nm.isEmpty()) { selectRowDeferred(nm); return true; }
        }
    }
    return ScopeHeaderBase::eventFilter(watched, event);
}

void ScopeHeaderLab::selectRowDeferred(const QString &name)
{
    /* Defer a tick: scopeSelected loops back through DevelopProperties -> setScopeRows,
       which rebuilds these rows. Emitting inline would delete the row widget from inside
       its own event handler. */
    QTimer::singleShot(0, this, [this, name]{ emit scopeSelected(name); });
}

void ScopeHeaderLab::toggleListCollapsed()
{
    listCollapsed = !listCollapsed;
    if (rowsContainer) rowsContainer->setVisible(!listCollapsed);
    updateListCollapseIcon();
    /* No scope can be picked while the list is hidden -> fall back to Global (index 0). */
    if (listCollapsed) selectRowDeferred(names.value(0, "Global"));
}

void ScopeHeaderLab::updateListCollapseIcon()
{
    if (!collapseBtn) return;
    /* Open branch (down) when the list shows; closed (right) when hidden. Full opacity,
       9x9, matching the tree's branch arrows. */
    const QString path = listCollapsed ? ":/images/branch-closed-winnow.png"
                                       : ":/images/branch-open-winnow.png";
    collapseBtn->setIcon(QIcon(QPixmap(path)));
}

void ScopeHeaderLab::setEyeIcon(BarBtn *b, bool shown)
{
    if (!b) return;
    b->setIcon(shown ? ":/images/icon16/eye.png" : ":/images/icon16/eye_off.png",
               G::iconOpacity);
}

void ScopeHeaderLab::updateBandEyeIcon()
{
    setEyeIcon(bandEyeBtn, enabledStates.value(activeIndex, true));
}

void ScopeHeaderLab::toggleActiveEnabled()
{
    /* The band eye IS the active row's eye: flip the same flag, and let the owner push
       the new state back through setScopeRows (which re-syncs both icons). */
    const int index = activeIndex;
    const bool on = enabledStates.value(index, true);
    QTimer::singleShot(0, this,
                       [this, index, on]{ emit scopeEnabledToggled(index, !on); });
}

void ScopeHeaderLab::showPanelMenu()
{
    QMenu menu(this);
    connect(menu.addAction(tr("New mask\tN")), &QAction::triggered,
            this, [this]{ emit addScopeRequested(); });
    /* Wipes the whole recipe (every scope, the geometry and the spots), the history and
       the sidecar record, on every selected image. DevelopProperties confirms first. */
    connect(menu.addAction(tr("Reset all edits")), &QAction::triggered,
            this, [this]{ emit resetAllEditsRequested(); });
    addPanelActions(&menu);
    addHelpAction(&menu);
    menu.exec(QCursor::pos());
}

void ScopeHeaderLab::addHelpAction(QMenu *menu)
{
    /* "Edits help", last item in every menu this panel pops -- the same place each
       section band in the tree below carries its own help. */
    if (!menu) return;
    menu->addSeparator();
    connect(menu->addAction(tr("Edits help")), &QAction::triggered,
            this, [this]{ emit helpRequested(); });
}

void ScopeHeaderLab::addPanelActions(QMenu *menu)
{
    if (!menu) return;
    /* The Edits LAYOUT picker (G::developEditsLayout): compare the candidates live, on
       real edits. Session-scoped; the owner sets the flag and rebuilds both halves.
       Retire this submenu with the flag once the comparison settles. */
    menu->addSeparator();
    QMenu *lay = menu->addMenu(tr("Edits layout"));
    struct Opt { G::EditsLayout mode; const char *label; const char *tip; };
    static const Opt opts[] = {
        {G::EditsLayout::Flat,    QT_TR_NOOP("Flat"),
         "Scope list, with the editor below the whole list"},
        {G::EditsLayout::Nested,  QT_TR_NOOP("Nested"),
         "Scope list, with the editor under the selected scope"},
        {G::EditsLayout::Minimal, QT_TR_NOOP("Minimal"),
         "No list: one scope dropdown, the editor below it, no rail"},
    };
    QActionGroup *group = new QActionGroup(lay);
    group->setExclusive(true);
    for (const Opt &o : opts) {
        QAction *a = lay->addAction(tr(o.label));
        a->setCheckable(true);
        a->setChecked(G::developEditsLayout == o.mode);
        a->setToolTip(o.tip);
        group->addAction(a);
        const int mode = int(o.mode);
        connect(a, &QAction::triggered, this, [this, mode]{
            /* Deferred like every other emission here: the switch rebuilds the rows (or
               the bar) the menu is still standing on. */
            QTimer::singleShot(0, this, [this, mode]{ emit editsLayoutRequested(mode); });
        });
    }
}

void ScopeHeaderLab::showRowMenu(int index, const QString &name, bool isGlobal)
{
    if (G::isLogger) G::log("ScopeHeaderLab::showRowMenu");

    /* Per-scope actions act on the ACTIVE scope in DevelopProperties, so a row menu first
       selects its own row. Read the choice as an int code while the menu is alive, then
       fire everything on the next tick (the select rebuilds the rows, deleting this
       button -- must not happen inside its own click handler). */
    enum { AddMask = 1, Reset, Remove, Rename, NewScope };
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

    /* MINIMAL layout: there is no header band, so this bar [:] is the ONLY menu in the
       panel -- it has to carry "Reset all edits" and the layout picker as well, or the
       way back out of this layout would be unreachable. These actions fire through their
       own connect (data code 0), so the per-scope dispatch below simply ignores them. */
    if (G::developEditsLayout == G::EditsLayout::Minimal) {
        menu.addSeparator();
        connect(menu.addAction(tr("Reset all edits")), &QAction::triggered,
                this, [this]{ emit resetAllEditsRequested(); });
        addPanelActions(&menu);
    }
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

void ScopeHeaderLab::setCollapsed(bool c)
{
    /* State only -- the scope LIST always stays visible. In the dropdown header this hid
       the active scope's tree items; in this list panel that job belongs to
       DevelopProperties::setTreeCollapsed (the adjustments below), NOT the scope rows.
       DevelopProperties drives this from Collapse-all / Solo / section-expand, so hiding
       the rows here would make the whole scope list vanish. */
    collapsed = c;
}

QString ScopeHeaderLab::currentScopeName() const
{
    return (activeIndex >= 0 && activeIndex < names.size()) ? names.at(activeIndex) : QString();
}
