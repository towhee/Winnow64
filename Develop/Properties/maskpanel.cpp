#include "Develop/Properties/maskpanel.h"
#include "Develop/Properties/maskeditor.h"
#include "Main/dockwidget.h"        // BarBtn
#include "Main/global.h"

#include <QAction>
#include <QCursor>
#include <QLabel>
#include <QLinearGradient>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPixmap>
#include <QPushButton>
#include <QApplication>
#include <QCursor>
#include <QHBoxLayout>
#include <QTimer>
#include <QVBoxLayout>

/*
    MaskPanel (see maskpanel.h): the active mask's editor, nested under its scope row --
    the submask list and the selected submask's settings. There is no commit button: a
    submask folds itself into the mask once its edits settle (see
    DevelopProperties::armMaskAutoCommit), so the panel is settings only.
*/

MaskPanel::MaskPanel(QWidget *parent) : QWidget(parent)
{
    if (G::isLogger) G::log("MaskPanel::MaskPanel");
    buildUi();
}

void MaskPanel::buildUi()
{
    QVBoxLayout *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    /* MASK-level settings (Edge, Halo) come FIRST, above the Submasks row: they act on
       the whole mask, so they read as a property of the mask itself rather than of
       anything in the list. Being outside the Submasks section also means collapsing that
       section leaves them up -- they are not part of what it collapses -- and it keeps
       the rows anchored directly under the scope row instead of shuffling as the list
       grows and submask settings appear below it. */
    buildMaskLevel(outer);

    /* The mask's contents: what the mask is made of, and every submask in it re-opens
       from here. */
    submaskList = new SubmaskList(this);
    connect(submaskList, &SubmaskList::collapsedChanged, this, [this]{
        syncAttrVisible();
    });
    outer->addWidget(submaskList);

    /* Settings + commit for the SELECTED submask. Wrapped so the whole block hides when
       nothing is selected (the list alone is then the panel). */
    attrWrap = new QWidget(this);
    /* Translucent, like levelWrap above: under the app stylesheet a plain QWidget fills
       its background opaquely, which would cover the panel's content background
       (G::panelContentBg, painted in paintEvent) with a dark slab. */
    attrWrap->setAttribute(Qt::WA_TranslucentBackground);
    QVBoxLayout *bl = new QVBoxLayout(attrWrap);
    bl->setContentsMargins(0, 2, 10, 6);
    bl->setSpacing(4);
    outer->addWidget(attrWrap);

    /* Header row: what a change to the settings below will AFFECT. A brush's attributes
       are baked per stroke, so the same slider reaches the submask's strokes when it is
       SELECTED, the next stroke while one is being ADDED, and the last stroke when Shift
       is held there -- with no way to tell which, and a slider that silently does nothing
       reads as broken. DevelopProperties keeps the text current, Shift included. */
    scopeLabel = new QLabel(attrWrap);
    scopeLabel->setWordWrap(true);
    /* Lined up with the submask settings' captions below it (see maskEditor). */
    scopeLabel->setContentsMargins(G::headerLeftInset + 2 * G::subHeaderIndent
                                       + G::subHeaderIndent / 2, 2, 0, 2);
    scopeLabel->setStyleSheet(G::labelCss(G::disabledColor,
                                          qMax(7, G::strFontSize.toInt() - 1)));
    scopeLabel->setVisible(false);
    bl->addWidget(scopeLabel);

    /* The tool's settings render here (Size/Feather/etc.), identical to the main tree.
       DevelopProperties populates + wires it. Full left margin (0) so its own caption
       column lines up with the tree below. */
    maskEditor = new MaskEditor(attrWrap);
    /* One step deeper than the mask-level rows (see buildMaskLevel): these settings
       belong to the SELECTED submask, whose row sits at G::headerLeftInset +
       2 * G::subHeaderIndent, so they start half a step in from that row's arrow.
       Indentation moves the caption column only -- the sliders stay put. */
    maskEditor->setIndentation(G::headerLeftInset + 2 * G::subHeaderIndent
                               + G::subHeaderIndent / 2);
    bl->addWidget(maskEditor);

    /* No button row under the settings. A submask used to need one to fold it into the
       mask, but nothing was ever buffered for it to flush -- every edit is written into
       the component and rendered as it is made -- so the submask now lands by itself a
       couple of seconds after the last edit (DevelopProperties::armMaskAutoCommit).
       Return finishes it early, Esc discards one that has not landed yet, and a landed
       submask is removed from its row's [:] menu.

       The overlay's APPEARANCE (colour + grayscale background) is not edited here
       either: it belongs to the veil, not to a submask, and lives on the Develop action
       row's tint button (left-click toggles the veil, right-click picks the colour /
       flips grayscale). The palette itself still lives with this class --
       overlayColours(). */

    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

    /* See syncHovered / eventFilter for why this is application-wide rather than this
       widget's own enterEvent/leaveEvent. */
    if (qApp) qApp->installEventFilter(this);
}

void MaskPanel::setTintButton(QWidget *btn)
{
/*
    Adopt the mask-overlay tint swatch onto the "Mask" band, at its right-hand end
    beside the band's [:].

    MW still owns it -- it builds it, wires "O" and the right-click colour/grayscale
    menu, and repaints the swatch -- because the veil's visibility and colour are
    application state, not the panel's. This only decides WHERE it sits, which is the
    panel's business: the swatch tints THIS mask, and it used to sit in the Develop
    action row at the top of the dock, among the Scope / Transform / Spot buttons, with
    nothing around it to say what it acted on.

    Consequence worth knowing: the band hides when the mask has no submasks
    (showMaskLevel), so on the Global scope -- where there is no mask and the swatch's
    own answer was a popup saying so -- the swatch is simply not there.
*/
    if (!btn || !levelBandLayout || levelTintSlot < 0) return;
    levelBandLayout->insertWidget(levelTintSlot, btn);
    levelBandLayout->insertSpacing(levelTintSlot + 1, G::headerBtnGap);
    /* The band toggles collapse on a click, but its eventFilter only watches levelBand
       itself, so a click on this child reaches the button. */
}

void MaskPanel::syncHovered()
{
    const bool now = isVisible()
                  && rect().contains(mapFromGlobal(QCursor::pos()));
    if (now == hovered) return;
    hovered = now;
    emit hoverChanged(hovered);
}

void MaskPanel::hideEvent(QHideEvent *e)
{
    /* The panel going away is a Leave that never arrives: the scope switched, the image
       changed, or the mask was deleted out from under the cursor. Without this the veil
       would stay engaged on a panel that is no longer on screen. */
    QWidget::hideEvent(e);
    syncHovered();
}

/*
    The "Mask" band and its two rows.

    The sliders used to sit bare between the scope row and the Submasks header, where they
    read as belonging to whichever of the two the eye landed on first -- and Edge appears
    twice in this panel, once here for the folded mask and once inside a submask's
    settings, with nothing on screen to tell them apart. A titled band says which level
    the rows below it act on, and gives them the same collapse + [:] affordances every
    other section in the dock has.
*/
void MaskPanel::buildMaskLevel(QVBoxLayout *outer)
{
    levelWrap = new QWidget(this);
    /* Translucent: paintEvent draws the header gradient on the PANEL, and under the app
       stylesheet a plain QWidget fills its background opaquely and would cover it. */
    levelWrap->setAttribute(Qt::WA_TranslucentBackground);
    QVBoxLayout *lw = new QVBoxLayout(levelWrap);
    lw->setContentsMargins(0, 4, 0, 0);        // 4 px clear of the scope row above
    lw->setSpacing(0);

    /* Header band: collapse arrow + "Mask" + [:], the same idiom as the Submasks band
       below it (SubmaskList::buildUi), so the two read as sibling sections. Both are
       folded away by the Edits bar's own arrow, so both start G::subHeaderIndent in from
       that bar's own arrow -- arrow and title -- reading as its children. The bar itself
       clears the panel edge by G::headerLeftInset, which every header here adds. */
    levelBand = new QWidget(levelWrap);
    levelBand->setAttribute(Qt::WA_TranslucentBackground);
    levelBand->setCursor(Qt::PointingHandCursor);
    levelBand->installEventFilter(this);       // a band click toggles collapse
    /* Height and caption placement of a property-tree section header (see RawPanel):
       the band is one tree row high and its content centres on the band less
       G::headerCaptionTrim at the bottom, matching PropertyDelegate's r4, so "Mask"
       lines up with the "Basic" / "Color" headers under it. */
    levelBand->setFixedHeight(G::propertyRowHeight());
    QHBoxLayout *hb = new QHBoxLayout(levelBand);
    hb->setContentsMargins(G::headerLeftInset + G::subHeaderIndent, 0,
                           G::headerBtnRightInset, G::headerCaptionTrim);
    hb->setSpacing(0);

    levelCollapseBtn = new BarBtn();
    levelCollapseBtn->setToolTip("Hide or show this mask: its Edge and Halo, its "
                                "submasks and their settings");
    levelCollapseBtn->setIconSize(QSize(9, 9));
    levelCollapseBtn->setFixedSize(9, 16);
    levelCollapseBtn->setStyleSheet("QToolButton { border: none; padding: 0;"
                                    " background: transparent; }");
    connect(levelCollapseBtn, &BarBtn::clicked, this, [this]{ toggleLevelCollapsed(); });

    levelTitle = new QLabel(tr("Mask"), levelBand);
    levelTitle->setToolTip("Settings for the FOLDED mask -- every submask combined.\n"
                           "The rows inside Submasks below act on one submask each.");
    levelTitle->setStyleSheet(G::labelCss(G::header2Color, G::strFontSize.toInt()));

    /* No band eye here, unlike Submasks: there is nothing to show or hide -- Edge and
       Halo reshape the mask rather than contributing to it, and both already have a
       neutral value (0) reachable from this menu. */
    levelMenuBtn = new BarBtn();
    levelMenuBtn->setToolTip("Mask actions (reset Edge and Halo)");
    levelMenuBtn->setIcon(":/images/icon16/ellipsis_vertical.png", G::iconOpacity);
    levelMenuBtn->setIconSize(QSize(16, 16));
    connect(levelMenuBtn, &BarBtn::clicked, this, [this]{ showLevelMenu(); });

    hb->addWidget(levelCollapseBtn);
    hb->addSpacing(G::decorationTitleGap);
    hb->addWidget(levelTitle);
    hb->addStretch(1);
    /* The overlay tint swatch is inserted HERE by setTintButton, ahead of the band's
       [:]. See setTintButton for why it lives on this band. */
    levelTintSlot = hb->count();
    hb->addWidget(levelMenuBtn);
    levelBandLayout = hb;
    lw->addWidget(levelBand);

    /* The rows themselves, indented under the band like the submask rows are. */
    levelBody = new QWidget(levelWrap);
    levelBody->setAttribute(Qt::WA_TranslucentBackground);
    QVBoxLayout *bl = new QVBoxLayout(levelBody);
    bl->setContentsMargins(0, 2, 10, 2);
    bl->setSpacing(0);
    maskLevelEditor = new MaskEditor(levelBody);
    /* Caption indent: the rows belong to the "Mask" band above them, so they start half
       an indent step in from that band's arrow (G::headerLeftInset + G::subHeaderIndent)
       -- under the middle of it -- the way the tree's rows sit under their section
       header. Set as the tree's indentation, which moves the CAPTION column only: the
       value column starts at the caption column's width, so the sliders and amounts do
       not move (they stay aligned with the tree's). */
    maskLevelEditor->setIndentation(G::headerLeftInset + G::subHeaderIndent
                                    + G::subHeaderIndent / 2);
    /* On the dock's own background, not the lighter subpanel surface: the mask-level
       block and the Submasks band under it are the frame around the SELECTED submask's
       details, and those details keep G::panelContentBg so they read as the thing being
       edited (see paintEvent and SubmaskList::paintEvent). */
    maskLevelEditor->setRowBackground(G::backgroundColor);
    bl->addWidget(maskLevelEditor);
    levelBody->setVisible(!levelCollapsed);
    lw->addWidget(levelBody);

    levelWrap->setVisible(false);              // no submasks yet: nothing to reshape
    outer->addWidget(levelWrap);
    updateLevelCollapseIcon();
}

void MaskPanel::paintEvent(QPaintEvent *)
{
    /* Header gradient behind the "Mask" band, drawn by the PANEL (the band itself is
       translucent) -- the same two shades SubmaskList paints behind its own band, so the
       two sections cannot drift apart. mapTo, not geometry(): the band is a grandchild
       here, where SubmaskList's is a direct child. */
    QPainter p(this);
    /* Subpanel content background (G::panelContentBg), the lift every Develop subpanel's
       contents carry. Painted before the early return below: the panel has content --
       the scope row and its buttons -- even when the Mask band is hidden. */
    p.fillRect(rect(), G::panelContentBg());
    if (!levelBand || !levelWrap || !levelWrap->isVisible()) return;
    /* The mask-level block (its band, its Edge/Halo rows and the gaps around them) sits
       on the dock background instead, continued behind the Submasks band by
       SubmaskList::paintEvent: everything above the selected submask's details frames
       them rather than belonging to them. */
    p.fillRect(QRect(levelWrap->mapTo(this, QPoint(0, 0)), levelWrap->size()),
               G::backgroundColor);
    const int a = G::backgroundShade + 5;
    const int b = G::backgroundShade - 15;
    const QRect r(levelBand->mapTo(this, QPoint(0, 0)), levelBand->size());
    QLinearGradient g(0, r.top(), 0, r.bottom());
    g.setColorAt(0, QColor(a, a, a));
    g.setColorAt(1, QColor(b, b, b));
    p.fillRect(r, g);
}

bool MaskPanel::eventFilter(QObject *watched, QEvent *event)
{
    /* HOVER, from the application-wide filter installed in buildUi: any Enter or Leave
       anywhere is a chance for the cursor to have crossed this panel's boundary,
       children included. Answered from the cursor position rather than from the event's
       receiver, so a rebuilt submask row cannot leave the state stale. Checked first and
       cheaply -- this filter sees every Enter/Leave in the app -- and never consumed. */
    if (event->type() == QEvent::Enter || event->type() == QEvent::Leave) {
        syncHovered();
        return QWidget::eventFilter(watched, event);
    }
    /* A click anywhere on the band (arrow or caption) toggles collapse. DblClick is
       included because Qt sends it INSTEAD of the second press, which would otherwise
       leave the section in the opposite state (same reason SubmaskList does). */
    if ((event->type() == QEvent::MouseButtonPress ||
         event->type() == QEvent::MouseButtonDblClick) && watched == levelBand) {
        QMouseEvent *me = static_cast<QMouseEvent *>(event);
        if (me->button() == Qt::LeftButton) { toggleLevelCollapsed(); return true; }
    }
    return QWidget::eventFilter(watched, event);
}

void MaskPanel::toggleLevelCollapsed()
{
    levelCollapsed = !levelCollapsed;
    syncLevelVisible();
    updateLevelCollapseIcon();
}

void MaskPanel::updateLevelCollapseIcon()
{
    if (!levelCollapseBtn) return;
    const QString path = levelCollapsed ? ":/images/branch-closed-winnow.png"
                                        : ":/images/branch-open-winnow.png";
    levelCollapseBtn->setIcon(QIcon(QPixmap(path)));
}

void MaskPanel::showLevelMenu()
{
    if (G::isLogger) G::log("MaskPanel::showLevelMenu");
    QMenu menu(this);
    QAction *aReset = menu.addAction(tr("Reset mask Edge and Halo"));
    /* Last item, as on every band in the Develop dock. Edge and Halo are documented on
       the submask page, which is the mask's page. */
    menu.addSeparator();
    QAction *aHelp = menu.addAction(tr("Mask help"));

    QAction *chosen = menu.exec(QCursor::pos());
    if (!chosen) return;
    /* Deferred: the handler rebuilds this panel's rows from under the click, exactly as
       SubmaskList's menu does. */
    if (chosen == aReset)
        QTimer::singleShot(0, this, [this]{ emit resetMaskLevelRequested(); });
    else if (chosen == aHelp)
        QTimer::singleShot(0, this, [this]{ emit helpRequested(); });
}

const QVector<QColor> &MaskPanel::overlayColours()
{
    /* The overlay speaks ONE colour and this is the choice: red first (the default),
       then colours far enough apart in hue to survive most subjects, ending in white
       for dark ones. Shared with the Mask band tint swatch's context menu. */
    static const QVector<QColor> colours = {
        QColor(220, 40, 40),   QColor(70, 200, 90),  QColor(60, 150, 255),
        QColor(240, 200, 40),  QColor(225, 70, 210), QColor(240, 240, 240)};
    return colours;
}

const QStringList &MaskPanel::overlayColourNames()
{
    static const QStringList names = {
        tr("Red"), tr("Green"), tr("Blue"), tr("Yellow"), tr("Magenta"), tr("White")};
    return names;
}

void MaskPanel::setPendingOp(int op)
{
    /* Modifiers are inert on the first submask -- there is nothing to subtract from or
       intersect with an empty mask -- and on a submask that has already landed, whose op
       is changed on its own row in the list. */
    if (editingExisting) return;
    if (firstMask) op = 0;
    if (op == pendingOp) return;
    pendingOp = op;
}

void MaskPanel::setEditingExisting(bool existing)
{
    editingExisting = existing;
    if (existing) pendingOp = 0;
}

void MaskPanel::showAttributes(bool show)
{
    attrShown = show;
    syncAttrVisible();
}

void MaskPanel::showMaskLevel(bool show)
{
    levelShown = show;
    syncLevelVisible();
}

void MaskPanel::syncLevelVisible()
{
    /* The "Mask" band is the panel's TOP band, so its arrow folds the WHOLE mask away:
       its own Edge/Halo rows, the Submasks section below them and the selected submask's
       settings. Anything left behind would read as belonging to the Edits bar rather than
       to the closed mask.

       Whether the band shows at all is a separate question: a mask with nothing to grow
       or shrink has no band (levelShown). With no band there is no arrow on screen, so
       nothing may stay folded either -- the Submasks section is then the only way to
       build the first submask. */
    if (!levelWrap) return;
    levelWrap->setVisible(levelShown);
    if (levelBody) levelBody->setVisible(!levelCollapsed);
    if (submaskList) submaskList->setVisible(!maskFolded());
    syncAttrVisible();              // the settings block folds with everything else
    update();                       // the band moved: repaint its gradient
}

/* True while the "Mask" band's arrow is closed AND that band is on screen to show it:
   everything below the band is folded away. */
bool MaskPanel::maskFolded() const
{
    return levelShown && levelCollapsed;
}

void MaskPanel::syncAttrVisible()
{
    /* The settings block belongs to a submask in the list above it, so a collapsed
       Submasks section takes it down too -- otherwise collapsing left the selected
       submask's settings (and the commit row) floating under a closed header. */
    if (!attrWrap) return;
    const bool collapsed = submaskList && submaskList->isCollapsed();
    attrWrap->setVisible(attrShown && !collapsed && !maskFolded());
}

void MaskPanel::setAttributeScope(const QString &text)
{
    if (!scopeLabel) return;
    if (scopeLabel->text() == text) return;      // called on every Shift press/release
    scopeLabel->setText(text);
    scopeLabel->setVisible(!text.isEmpty());
}

void MaskPanel::beginPending(bool first)
{
    firstMask = first;
    editingExisting = false;
    pendingOp = 0;                  // every submask opens as Add
    /* A new submask is being built: re-open the section, or its settings would be
       invisible (the [+] that starts one is on the header). */
    if (submaskList) submaskList->setCollapsed(false);
    showAttributes(true);
    setVisible(true);
}
