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
#include <QHBoxLayout>
#include <QTimer>
#include <QVBoxLayout>

/*
    MaskPanel (see maskpanel.h): the active mask's editor, nested under its scope row --
    the submask list, the selected submask's settings, the overlay-colour row and ONE
    commit button whose label follows the held modifier (or reads "Done" when the submask
    being edited is already committed).
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
    scopeLabel->setContentsMargins(10, 2, 0, 2);
    scopeLabel->setStyleSheet(G::labelCss(G::disabledColor,
                                          qMax(7, G::strFontSize.toInt() - 1)));
    scopeLabel->setVisible(false);
    bl->addWidget(scopeLabel);

    /* The tool's settings render here (Size/Feather/etc.), identical to the main tree.
       DevelopProperties populates + wires it. Full left margin (0) so its own caption
       column lines up with the tree below. */
    maskEditor = new MaskEditor(attrWrap);
    bl->addWidget(maskEditor);

    /* Commit buttons live under the settings, inset like a normal control row. */
    QWidget *btnWrap = new QWidget(attrWrap);
    QVBoxLayout *bw = new QVBoxLayout(btnWrap);
    bw->setContentsMargins(10, 4, 0, 0);
    bw->setSpacing(4);
    bl->addWidget(btnWrap);

    /* The overlay's APPEARANCE (colour + grayscale background) is not edited here: it
       belongs to the veil, not to a submask, and it now lives on the Develop action row's
       tint button (left-click toggles the veil, right-click picks the colour / flips
       grayscale). The palette itself still lives with this class -- overlayColours(). */

    /* ONE commit button. Its label follows the op the overlay is previewing, so the words
       Subtract/Intersect stay visible instead of hiding behind undocumented keys. The
       [x] beside it discards a submask that is still being built; a re-opened submask has
       nothing to discard, so it is hidden there (see refreshCommitBtn). */
    QWidget *commitRow = new QWidget(btnWrap);
    QHBoxLayout *cl = new QHBoxLayout(commitRow);
    cl->setContentsMargins(0, 0, 0, 0);
    cl->setSpacing(6);
    commitBtn = new QPushButton(tr("Add and Commit"), commitRow);
    commitBtn->setToolTip("Commit this submask into the mask (Return)\n"
                          "Opt: subtract    Shift+Opt: intersect");
    connect(commitBtn, &QPushButton::clicked, this, [this]{ emit committed(); });
    /* The global stylesheet (widgetcss.cpp) sets "QPushButton { min-width: 100px }",
       which Qt applies as an EXPLICIT minimum width (~112px at this DPI) and which floors
       the whole develop dock's width (a size policy alone can't undo an explicit
       minimum). Override it here so the panel never widens the dock. */
    commitBtn->setStyleSheet("QPushButton { min-width: 0; }");
    commitBtn->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    cancelBtn = new BarBtn();
    cancelBtn->setToolTip("Cancel (Esc): discard this submask");
    cancelBtn->setIcon(":/images/icon16/close.png", G::iconOpacity);
    connect(cancelBtn, &BarBtn::clicked, this, [this]{ emit cancelled(); });
    cl->addWidget(commitBtn);
    cl->addWidget(cancelBtn);
    bw->addWidget(commitRow);

    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
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
       below it (SubmaskList::buildUi), so the two read as sibling sections. */
    levelBand = new QWidget(levelWrap);
    levelBand->setAttribute(Qt::WA_TranslucentBackground);
    levelBand->setCursor(Qt::PointingHandCursor);
    levelBand->installEventFilter(this);       // a band click toggles collapse
    QHBoxLayout *hb = new QHBoxLayout(levelBand);
    hb->setContentsMargins(0, 3, G::headerBtnRightInset, 3);
    hb->setSpacing(0);

    levelCollapseBtn = new BarBtn();
    levelCollapseBtn->setToolTip("Hide or show the mask's Edge and Halo");
    levelCollapseBtn->setIconSize(QSize(9, 9));
    levelCollapseBtn->setFixedSize(9, 16);
    levelCollapseBtn->setStyleSheet("QToolButton { border: none; padding: 0;"
                                    " background: transparent; }");
    connect(levelCollapseBtn, &BarBtn::clicked, this, [this]{ toggleLevelCollapsed(); });

    levelTitle = new QLabel(tr("Mask"), levelBand);
    levelTitle->setToolTip("Settings for the FOLDED mask -- every submask combined.\n"
                           "The rows inside Submasks below act on one submask each.");
    levelTitle->setStyleSheet(G::labelCss(G::header3Color, G::strFontSize.toInt()));

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
    hb->addWidget(levelMenuBtn);
    lw->addWidget(levelBand);

    /* The rows themselves, indented under the band like the submask rows are. */
    levelBody = new QWidget(levelWrap);
    levelBody->setAttribute(Qt::WA_TranslucentBackground);
    QVBoxLayout *bl = new QVBoxLayout(levelBody);
    bl->setContentsMargins(0, 2, 10, 2);
    bl->setSpacing(0);
    maskLevelEditor = new MaskEditor(levelBody);
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
    if (!levelBand || !levelWrap || !levelWrap->isVisible()) return;
    QPainter p(this);
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
       for dark ones. Shared with the Develop action-row tint button's context menu. */
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

void MaskPanel::refreshCommitBtn()
{
    if (!commitBtn) return;
    if (editingExisting) {
        /* Already folded into the mask: there is nothing to commit and nothing to throw
           away -- the button just closes the editing session. */
        commitBtn->setText(tr("Done"));
        commitBtn->setToolTip("Finish editing this submask (Return).\n"
                              "Its edits are already part of the mask.");
    }
    else {
        /* The label names the OPERATION and the ACT: with the render already live, a bare
           "Update" read as "refresh the view" rather than "fold this submask in". */
        commitBtn->setText(pendingOp == 1 ? tr("Subtract and Commit")
                         : pendingOp == 2 ? tr("Intersect and Commit")
                                          : tr("Add and Commit"));
        commitBtn->setToolTip("Commit this submask into the mask (Return)\n"
                              "Opt: subtract    Shift+Opt: intersect");
    }
    if (cancelBtn) cancelBtn->setVisible(!editingExisting);
}

void MaskPanel::setPendingOp(int op)
{
    /* Modifiers are inert on the first submask -- there is nothing to subtract from or
       intersect with an empty mask -- and on a submask that is already committed, whose
       op is changed on its own row in the list. */
    if (editingExisting) return;
    if (firstMask) op = 0;
    if (op == pendingOp) return;
    pendingOp = op;
    refreshCommitBtn();
}

void MaskPanel::setEditingExisting(bool existing)
{
    editingExisting = existing;
    if (existing) pendingOp = 0;
    refreshCommitBtn();
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
    /* NOT gated on the Submasks collapse, unlike the settings block: this section sits
       ABOVE that section's header, so it is not part of what the header collapses. Two
       things decide it: does the mask have anything to grow or shrink (the band goes
       entirely), and is this band collapsed (only its rows go). */
    if (!levelWrap) return;
    levelWrap->setVisible(levelShown);
    if (levelBody) levelBody->setVisible(!levelCollapsed);
    update();                       // the band moved: repaint its gradient
}

void MaskPanel::syncAttrVisible()
{
    /* The settings block belongs to a submask in the list above it, so a collapsed
       Submasks section takes it down too -- otherwise collapsing left the selected
       submask's settings (and the commit row) floating under a closed header. */
    if (!attrWrap) return;
    const bool collapsed = submaskList && submaskList->isCollapsed();
    attrWrap->setVisible(attrShown && !collapsed);
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
    pendingOp = 0;                  // every submask opens as Add ("Add and Commit")
    refreshCommitBtn();
    /* A new submask is being built: re-open the section, or its settings and the
       commit button would be invisible (the [+] that starts one is on the header). */
    if (submaskList) submaskList->setCollapsed(false);
    showAttributes(true);
    setVisible(true);
}
