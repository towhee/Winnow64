#include "Develop/Properties/submasklist.h"
#include "Main/dockwidget.h"        // BarBtn
#include "Main/global.h"

#include <QAction>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPixmap>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>

/*
    SubmaskList (see submasklist.h): the mask's ordered submasks, each one selectable,
    toggleable, re-ordered and deletable. Rebuilt wholesale by setSubmasks().
*/

SubmaskList::SubmaskList(QWidget *parent) : QWidget(parent)
{
    if (G::isLogger) G::log("SubmaskList::SubmaskList");
    buildUi();
}

void SubmaskList::buildUi()
{
    QVBoxLayout *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    /* Header band: collapse arrow + "Submasks" + [+]. Translucent so paintEvent draws the
       property-header gradient behind it (same idiom as RawPanel / ScopeHeader). Its
       arrow and title are offset G::subHeaderIndent from the Edits bar's arrow, like the
       Mask band and the tree's sections: the bar above folds all of them away, so they
       read as its children. G::headerLeftInset is the bar's own inset from the panel
       edge, shared by every header in the dock. */
    headerBand = new QWidget(this);
    headerBand->setAttribute(Qt::WA_TranslucentBackground);
    headerBand->setCursor(Qt::PointingHandCursor);
    headerBand->installEventFilter(this);        // a header click toggles collapse
    /* Height and caption placement of a property-tree section header (see RawPanel), the
       same the Mask band uses, so the two sibling bands cannot drift apart. */
    headerBand->setFixedHeight(G::propertyRowHeight());
    QHBoxLayout *hb = new QHBoxLayout(headerBand);
    hb->setContentsMargins(G::headerLeftInset + G::subHeaderIndent, 0,
                           G::headerBtnRightInset, G::headerCaptionTrim);
    hb->setSpacing(0);

    collapseBtn = new BarBtn();
    collapseBtn->setToolTip("Hide or show the submask list");
    collapseBtn->setIconSize(QSize(9, 9));
    collapseBtn->setFixedSize(9, 16);
    collapseBtn->setStyleSheet("QToolButton { border: none; padding: 0;"
                               " background: transparent; }");
    connect(collapseBtn, &BarBtn::clicked, this, [this]{ toggleCollapsed(); });

    titleLabel = new QLabel(tr("Submasks"), headerBand);
    titleLabel->setStyleSheet(G::labelCss(G::header3Color, G::strFontSize.toInt()));

    addBtn = new BarBtn();
    addBtn->setToolTip("Add a submask to this mask (M)");
    /* new.png (the [+] plus), the same "add one of these" glyph the Embellish dock
       uses for its New Template / Border / Text buttons. */
    addBtn->setIcon(":/images/icon16/new.png", G::iconOpacity);
    addBtn->setIconSize(QSize(16, 16));
    connect(addBtn, &BarBtn::clicked, this, [this]{ emit addRequested(); });

    /* Band eye: show/hide EVERY submask's contribution in one click -- an A/B of the
       whole mask. It reads as shown while any submask is enabled. Like every other band
       and row in this panel, the trailing pair is eye then menu, menu last. */
    eyeBtn = new BarBtn();
    eyeBtn->setToolTip("Show or hide every submask's contribution");
    setEyeIcon(eyeBtn, true);
    eyeBtn->setIconSize(QSize(16, 16));
    connect(eyeBtn, &BarBtn::clicked, this, [this]{ toggleAllEnabled(); });

    menuBtn = new BarBtn();
    menuBtn->setToolTip("Submask list actions (add, show or hide all)");
    menuBtn->setIcon(":/images/icon16/ellipsis_vertical.png", G::iconOpacity);
    menuBtn->setIconSize(QSize(16, 16));
    connect(menuBtn, &BarBtn::clicked, this, [this]{ showListMenu(); });

    hb->addWidget(collapseBtn);
    hb->addSpacing(G::decorationTitleGap);
    hb->addWidget(titleLabel);
    hb->addStretch(1);
    hb->addWidget(addBtn);
    hb->addSpacing(G::headerBtnGap);
    hb->addWidget(eyeBtn);
    hb->addSpacing(G::headerBtnGap);
    hb->addWidget(menuBtn);
    outer->addWidget(headerBand);
    updateCollapseIcon();

    /* Rows container: one row widget per submask, rebuilt by setSubmasks. Translucent for
       the same reason ScopeHeader's is -- under the app stylesheet a plain QWidget
       fills its background opaquely and would paint over the containment rail the scope
       list draws behind its children. */
    rowsContainer = new QWidget(this);
    rowsContainer->setAttribute(Qt::WA_TranslucentBackground);
    rowsLayout = new QVBoxLayout(rowsContainer);
    rowsLayout->setContentsMargins(0, 0, 0, 0);
    rowsLayout->setSpacing(0);
    rowsContainer->setVisible(!collapsed);      // starts collapsed (see the header)
    outer->addWidget(rowsContainer);

    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
}

void SubmaskList::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    /* ONE flat surface (G::panelContentBg), band included -- no header gradient here,
       unlike every other band in the dock. The Submasks band and its rows are the inside
       of the Mask panel, not a section of their own: the Mask panel's own "Mask" band
       already says where this block starts, and a second gradient inside it read as a
       second panel. The band still carries its arrow, caption and buttons; only the fill
       behind it changed. */
    p.fillRect(rect(), G::panelContentBg());
    /* The BAND, though, continues the mask-level block's dock background above it
       (MaskPanel::paintEvent): the lighter surface starts at the submask rows, which are
       what the details below them belong to. */
    if (headerBand)
        p.fillRect(QRect(headerBand->mapTo(this, QPoint(0, 0)), headerBand->size()),
                   G::backgroundColor);
}

QString SubmaskList::opName(int op)
{
    return op == 1 ? tr("Subtract") : op == 2 ? tr("Intersect") : tr("Add");
}

QString SubmaskList::opGlyph(int op)
{
    /* One character that says what the submask DOES to the mask so far: it adds area,
       takes area away, or keeps only the overlap. */
    return op == 1 ? QString("-") : op == 2 ? QString("∩") : QString("+");
}

void SubmaskList::emitDeferred(std::function<void()> fn)
{
    QTimer::singleShot(0, this, [fn]{ fn(); });
}

void SubmaskList::setSubmasks(const QVector<SubmaskRowInfo> &rows, int selected)
{
    infos = rows;
    selectedIndex = (selected >= 0 && selected < rows.size()) ? selected : -1;
    rebuild();
    updateBandEyeIcon();
}

void SubmaskList::rebuild()
{
    while (QLayoutItem *it = rowsLayout->takeAt(0)) {
        if (QWidget *w = it->widget()) w->deleteLater();
        delete it;
    }
    for (int i = 0; i < infos.size(); ++i)
        rowsLayout->addWidget(makeRow(i, infos.at(i), i == selectedIndex));
    /* An empty mask still shows the header: its [+] is how the first submask gets made,
       so hiding the section on an empty mask would hide the only way out of it. */
}

QWidget *SubmaskList::makeRow(int index, const SubmaskRowInfo &r, bool selected)
{
    QWidget *row = new QWidget(rowsContainer);
    row->setProperty("submaskIndex", index);     // read back by the row-body click filter
    row->installEventFilter(this);
    if (selected) {
        /* Same treatment as the scope rows: an object-name selector (so the rule cannot
           leak onto the row's children) and a muted band while the panel is greyed. */
        row->setObjectName("submaskRow");
        row->setStyleSheet(QString("QWidget#submaskRow { background: %1; }"
                                   "QWidget#submaskRow:disabled { background: %2; }")
                               .arg(G::selectionColor.name(),
                                    G::dimmed(G::selectionColor).name()));
    }
    else {
        row->setAttribute(Qt::WA_TranslucentBackground);
    }

    QHBoxLayout *hb = new QHBoxLayout(row);
    /* Indented ONE level under the Submasks band: that band's arrow sits at
       G::headerLeftInset + G::subHeaderIndent, and its rows step the same
       G::subHeaderIndent further right, the way the tree's rows sit under their section
       header. */
    hb->setContentsMargins(G::headerLeftInset + 2 * G::subHeaderIndent, 1,
                           G::headerBtnRightInset, 1);
    hb->setSpacing(G::headerBtnGap);

    /* Expand/collapse arrow, the same 9px glyph every band in this dock uses: open (down)
       while this submask's settings are showing below it (MaskPanel's attribute block),
       closed (right) otherwise. A submask row IS an expander -- selecting it opens its
       settings and selecting it again closes them -- so the arrow just does what a click
       on the row body does, and says which state the row is in. */
    BarBtn *openBtn = new BarBtn();
    openBtn->setToolTip(selected ? tr("Close this submask's settings")
                                 : tr("Open this submask's settings"));
    openBtn->setIconSize(QSize(9, 9));
    openBtn->setFixedSize(9, 16);
    openBtn->setStyleSheet("QToolButton { border: none; padding: 0;"
                           " background: transparent; }");
    openBtn->setIcon(QIcon(QPixmap(selected ? ":/images/branch-open-winnow.png"
                                            : ":/images/branch-closed-winnow.png")));
    connect(openBtn, &BarBtn::clicked, this, [this, index]{
        emitDeferred([this, index]{ emit submaskSelected(index); });
    });
    hb->addWidget(openBtn);
    hb->addSpacing(G::decorationTitleGap);

    /* Op chip. The FIRST submask has nothing to combine with, so its op is inert and the
       chip is shown flat and disabled rather than hidden (the column stays aligned). */
    QPushButton *opBtn = new QPushButton(opGlyph(r.op), row);
    opBtn->setFixedSize(18, 18);
    opBtn->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    /* Flat on the row: the app stylesheet gives a QPushButton its own raised background,
       which read as a slab on the row's lifted surface. The glyph alone carries the op;
       a hover fill keeps it discoverable as something clickable. min-width: 0 defeats the
       global "QPushButton { min-width: 100px }" (widgetcss.cpp), which Qt applies as an
       EXPLICIT minimum and which would floor the dock's width. */
    opBtn->setStyleSheet(
        "QPushButton { min-width: 0; padding: 0; border: none; border-radius: 9px;"
                     " background: transparent; }"
        "QPushButton:hover:enabled { background: " + G::groupSeparatorColor().name() + "; }");
    if (index == 0) {
        opBtn->setEnabled(false);
        opBtn->setToolTip("The first submask starts the mask, so it always adds");
    }
    else {
        opBtn->setToolTip(QString("%1 -- click to cycle Add / Subtract / Intersect")
                              .arg(opName(r.op)));
        const int next = (r.op + 1) % 3;
        connect(opBtn, &QPushButton::clicked, this, [this, index, next]{
            emitDeferred([this, index, next]{ emit opChanged(index, next); });
        });
    }
    hb->addWidget(opBtn);

    QString caption = r.toolName;
    if (r.inverted) caption += tr(" (inverted)");
    QLabel *name = new QLabel(caption, row);
    name->setStyleSheet(G::labelCss(selected ? QColor(Qt::white)
                                    : r.enabled ? G::textColor
                                                : G::disabledColor,
                                    G::strFontSize.toInt()));
    name->setToolTip(r.pending  ? tr("Being built -- it joins the mask once your edits "
                                     "settle (Return lands it now, Esc discards it)")
                     : selected ? tr("Open -- click to close its settings")
                                : tr("Click to edit this submask again"));
    hb->addWidget(name);
    hb->addStretch(1);

    /* Show/hide this submask's contribution (MaskComponent::enabled). The renderer skips
       a disabled component, so this is a one-click A/B of what it is doing. */
    BarBtn *rowEye = new BarBtn();
    rowEye->setToolTip("Show or hide this submask's contribution to the mask");
    setEyeIcon(rowEye, r.enabled);
    rowEye->setIconSize(QSize(16, 16));
    const bool on = r.enabled;
    connect(rowEye, &BarBtn::clicked, this, [this, index, on]{
        emitDeferred([this, index, on]{ emit enabledToggled(index, !on); });
    });
    hb->addWidget(rowEye);

    BarBtn *rowMenu = new BarBtn();
    rowMenu->setToolTip("Submask actions (edit, op, invert, move, duplicate, delete)");
    rowMenu->setIcon(":/images/icon16/ellipsis_vertical.png", G::iconOpacity);
    rowMenu->setIconSize(QSize(16, 16));
    connect(rowMenu, &BarBtn::clicked, this, [this, index]{ showRowMenu(index); });
    hb->addWidget(rowMenu);

    return row;
}

bool SubmaskList::eventFilter(QObject *watched, QEvent *event)
{
    /* A click on the header band (arrow or caption) toggles collapse; MouseButtonDblClick
       is included because Qt sends it INSTEAD of the second press, which would otherwise
       leave the list in the opposite state. A click on a row body selects that submask;
       the op chip / eye / menu button consume their own clicks. */
    if ((event->type() == QEvent::MouseButtonPress ||
         event->type() == QEvent::MouseButtonDblClick) && watched == headerBand) {
        QMouseEvent *me = static_cast<QMouseEvent *>(event);
        if (me->button() == Qt::LeftButton) { toggleCollapsed(); return true; }
    }
    else if (event->type() == QEvent::MouseButtonRelease) {
        QWidget *w = qobject_cast<QWidget *>(watched);
        QMouseEvent *me = static_cast<QMouseEvent *>(event);
        if (w && me->button() == Qt::LeftButton) {
            const QVariant v = w->property("submaskIndex");
            if (v.isValid()) {
                const int index = v.toInt();
                emitDeferred([this, index]{ emit submaskSelected(index); });
                return true;
            }
        }
    }
    return QWidget::eventFilter(watched, event);
}

void SubmaskList::setEyeIcon(BarBtn *b, bool shown)
{
    if (!b) return;
    b->setIcon(shown ? ":/images/icon16/eye.png" : ":/images/icon16/eye_off.png",
               G::iconOpacity);
}

void SubmaskList::updateBandEyeIcon()
{
    /* Shown while ANY submask still contributes, so the band eye is "off" only when the
       whole mask is hidden -- which is exactly the state its click undoes. */
    bool any = false;
    for (const SubmaskRowInfo &r : infos) if (r.enabled) { any = true; break; }
    setEyeIcon(eyeBtn, infos.isEmpty() ? true : any);
}

void SubmaskList::toggleAllEnabled()
{
    /* One flag for the whole list: if anything is showing, hide everything; otherwise
       bring it all back. Emitted per row (the owner has no bulk slot) and deferred a tick
       like every other emission here, since each one rebuilds these rows. */
    bool any = false;
    for (const SubmaskRowInfo &r : infos) if (r.enabled) { any = true; break; }
    const bool on = !any;
    const int n = infos.size();
    emitDeferred([this, n, on]{
        for (int i = 0; i < n; ++i) emit enabledToggled(i, on);
    });
}

void SubmaskList::showListMenu()
{
    if (G::isLogger) G::log("SubmaskList::showListMenu");
    enum { Add = 1, ShowAll, HideAll };
    QMenu menu(this);
    menu.addAction(tr("Add submask\tM"))->setData(Add);
    menu.addSeparator();
    QAction *aShow = menu.addAction(tr("Show all submasks"));
    aShow->setData(ShowAll);
    QAction *aHide = menu.addAction(tr("Hide all submasks"));
    aHide->setData(HideAll);
    aShow->setEnabled(!infos.isEmpty());
    aHide->setEnabled(!infos.isEmpty());
    /* Last item, as on every band in the Develop dock: this list's own help page. */
    menu.addSeparator();
    QAction *aHelp = menu.addAction(tr("Submasks help"));

    QAction *chosen = menu.exec(QCursor::pos());
    if (chosen == aHelp) { emitDeferred([this]{ emit helpRequested(); }); return; }
    const int code = chosen ? chosen->data().toInt() : 0;
    if (code == 0) return;
    const int n = infos.size();
    emitDeferred([this, code, n]{
        if (code == Add) { emit addRequested(); return; }
        const bool on = (code == ShowAll);
        for (int i = 0; i < n; ++i) emit enabledToggled(i, on);
    });
}

void SubmaskList::showRowMenu(int index)
{
    if (G::isLogger) G::log("SubmaskList::showRowMenu");
    if (index < 0 || index >= infos.size()) return;
    const SubmaskRowInfo r = infos.at(index);

    /* Read the choice as an int code while the menu is alive, then fire on the next tick:
       every action rebuilds these rows, deleting the button whose click we are inside. */
    enum { Edit = 1, OpAdd, OpSubtract, OpIntersect, Invert, Up, Down, Dup, Del };
    QMenu menu(this);
    /* Selecting the submask that is ALREADY open closes it (DevelopProperties::
       reopenSubmask), so this item has to say so -- it emits the same signal the row
       click does, and an "Edit" that closed the settings would be a lie. Left ENABLED on
       a PENDING submask, which cannot actually be closed (it still needs its commit
       button): the handler explains that, and one route with one explanation beats a
       greyed item here and a working one on the row. */
    const bool isOpen = (index == selectedIndex);
    QAction *aEdit = menu.addAction(isOpen ? tr("Close %1 settings").arg(r.toolName)
                                           : tr("Edit %1").arg(r.toolName));
    aEdit->setData(Edit);
    menu.addSeparator();
    /* Op is meaningless on the first submask -- there is nothing under it to combine
       with -- so offer it only from the second one down. */
    if (index > 0) {
        const int codes[3] = {OpAdd, OpSubtract, OpIntersect};
        for (int op = 0; op < 3; ++op) {
            QAction *a = menu.addAction(opName(op));
            a->setData(codes[op]);
            a->setCheckable(true);
            a->setChecked(r.op == op);
        }
        menu.addSeparator();
    }
    QAction *inv = menu.addAction(tr("Invert"));
    inv->setData(Invert);
    inv->setCheckable(true);
    inv->setChecked(r.inverted);
    menu.addSeparator();
    menu.addAction(tr("Move up"))->setData(Up);
    menu.addAction(tr("Move down"))->setData(Down);
    menu.addAction(tr("Duplicate"))->setData(Dup);
    menu.addAction(tr("Delete"))->setData(Del);

    QAction *chosen = menu.exec(QCursor::pos());
    const int code = chosen ? chosen->data().toInt() : 0;
    if (code == 0) return;
    const int last = infos.size() - 1;

    emitDeferred([this, index, code, last]{
        switch (code) {
            case Edit:        emit submaskSelected(index);        break;
            case OpAdd:       emit opChanged(index, 0);           break;
            case OpSubtract:  emit opChanged(index, 1);           break;
            case OpIntersect: emit opChanged(index, 2);           break;
            case Invert:      emit invertRequested(index);        break;
            case Up:   if (index > 0)    emit moveRequested(index, index - 1);  break;
            case Down: if (index < last) emit moveRequested(index, index + 1);  break;
            case Dup:         emit duplicateRequested(index);     break;
            case Del:         emit deleteRequested(index);        break;
        }
    });
}

void SubmaskList::toggleCollapsed()
{
    setCollapsed(!collapsed);
}

void SubmaskList::setCollapsed(bool collapse)
{
    if (collapse == collapsed) return;
    collapsed = collapse;
    if (rowsContainer) rowsContainer->setVisible(!collapsed);
    updateCollapseIcon();
    /* The selected submask's settings live BELOW this widget (MaskPanel), so the section
       only reads as collapsed if they go away with the rows. */
    emit collapsedChanged(collapsed);
}

void SubmaskList::updateCollapseIcon()
{
    if (!collapseBtn) return;
    /* Open branch (down) when the rows show, closed (right) when hidden -- 9x9, matching
       the tree's branch arrows and the Raw panel's. */
    const QString path = collapsed ? ":/images/branch-closed-winnow.png"
                                   : ":/images/branch-open-winnow.png";
    collapseBtn->setIcon(QIcon(QPixmap(path)));
}
