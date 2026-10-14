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

    /* Rows container: one row widget per submask, rebuilt by setSubmasks. Translucent for
       the same reason ScopeHeader's is -- under the app stylesheet a plain QWidget
       fills its background opaquely and would paint over the containment rail the scope
       list draws behind its children. */
    rowsContainer = new QWidget(this);
    rowsContainer->setAttribute(Qt::WA_TranslucentBackground);
    rowsLayout = new QVBoxLayout(rowsContainer);
    rowsLayout->setContentsMargins(0, 0, 0, 0);
    rowsLayout->setSpacing(0);
    outer->addWidget(rowsContainer);

    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
}

void SubmaskList::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    /* ONE flat surface (G::panelContentBg): the rows are the inside of the Mask panel,
       whose own "Mask" band already says where this block starts. */
    p.fillRect(rect(), G::panelContentBg);
    /* Separator between the Edge/Halo rows above and the submask rows, in the gap
       setTopRule opens above them, drawn as PropertyDelegate draws the tree's group
       dividers: centred in the gap, inset from both sides. */
    if (topRule && rowsContainer) {
        const int gap = rowsContainer->mapTo(this, QPoint(0, 0)).y();
        const int w = G::groupSeparatorWidth;
        const int inset = G::groupSeparatorInset;
        if (gap > 0)
            p.fillRect(inset, (gap - w) / 2, width() - 2 * inset, w, G::groupSeparatorColor);
    }
}

void SubmaskList::setTopRule(bool on)
{
    if (topRule == on) return;
    topRule = on;
    /* The gap the separator sits in is the outer layout's top margin. */
    if (layout()) layout()->setContentsMargins(0, on ? G::groupSeparatorHeight : 0, 0, 0);
    update();
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
}

void SubmaskList::rebuild()
{
    while (QLayoutItem *it = rowsLayout->takeAt(0)) {
        if (QWidget *w = it->widget()) w->deleteLater();
        delete it;
    }
    for (int i = 0; i < infos.size(); ++i)
        rowsLayout->addWidget(makeRow(i, infos.at(i), i == selectedIndex));
    /* See rowPad(). Measured from a built row, so it follows the font size. */
    pad = 0;
    if (QLayoutItem *first = rowsLayout->itemAt(0))
        pad = qMax(0, (G::propertyRowHeight() - first->sizeHint().height()) / 2);
    rowsLayout->setContentsMargins(0, pad, 0, 0);
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
    /* Arrow at G::headerLeftInset + G::subHeaderIndent: where the old "Submasks" band's
       arrow sat, which is also the Mask band's arrow column, so each submask reads as a
       section of the mask. */
    hb->setContentsMargins(G::headerLeftInset + G::subHeaderIndent, 1,
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

    /* Caption ink, shared by the name and the op chip after it. */
    const QColor ink = selected ? QColor(Qt::white)
                     : r.enabled ? G::textColor : G::disabledColor;
    QString caption = r.toolName;
    if (r.inverted) caption += tr(" (inverted)");
    QLabel *name = new QLabel(caption, row);
    name->setStyleSheet(G::labelCss(ink, G::strFontSize.toInt()));
    QString nameTip = r.pending  ? tr("Being built -- it joins the mask once your edits "
                                      "settle (Return lands it now, Esc discards it)")
                      : selected ? tr("Open -- click to close its settings")
                                 : tr("Click to edit this submask again");
    /* The first submask's op chip is inert (see below) and carries no tooltip of its
       own, so say why here rather than nowhere. */
    if (index == 0)
        nameTip += tr("\n\nThe first submask starts the mask, so it always adds.");
    name->setToolTip(nameTip);
    hb->addWidget(name);

    /* Op chip, AFTER the name -- "Brush Submask (-)" -- so the captions start in one
       column and read as names first (2026-10-14; it used to lead the row). The FIRST
       submask has nothing to combine with, so its op is inert -- but the chip is still
       shown, so the "(+)" still says what that submask does. */
    QPushButton *opBtn = new QPushButton("(" + opGlyph(r.op) + ")", row);
    opBtn->setFixedHeight(18);
    opBtn->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    /* Flat on the row: the app stylesheet gives a QPushButton its own raised background,
       which read as a slab on the row's lifted surface. The glyph alone carries the op;
       a hover fill keeps it discoverable as something clickable. min-width: 0 defeats the
       global "QPushButton { min-width: 100px }" (widgetcss.cpp), which Qt applies as an
       EXPLICIT minimum and which would floor the dock's width. Same ink and size as the
       name beside it (greying with it too), so "(-)" reads as part of the caption. */
    opBtn->setStyleSheet(
        "QPushButton { min-width: 0; padding: 0 2px; border: none; border-radius: 9px;"
                     " background: transparent; color: " + ink.name() + ";"
                     " font-size: " + G::strFontSize + "pt; }"
        "QPushButton:disabled { color: " + G::disabledColor.name() + "; }"
        "QPushButton:hover:enabled { background: " + G::groupSeparatorColor.name() + "; }");
    if (index == 0) {
        /* Inert, but NOT setEnabled(false): that greys the glyph, and a dim "+" beside
           the crisp ones below it reads as "this submask is disabled" or as a fault,
           when all it means is that the first submask cannot combine with anything. The
           chip is made transparent to the mouse instead -- it cannot be clicked or
           hovered, it keeps the same ink as every other row, and the click falls through
           to the row, which opens the submask like a click anywhere else on it. */
        opBtn->setAttribute(Qt::WA_TransparentForMouseEvents, true);
        opBtn->setFocusPolicy(Qt::NoFocus);
        /* Its own tooltip would never show -- the chip sees no mouse events now -- so the
           explanation moves onto the row's name. */
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
    /* A click on a row body selects that submask; the op chip / eye / menu button
       consume their own clicks. */
    if (event->type() == QEvent::MouseButtonRelease) {
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

bool SubmaskList::anyEnabled() const
{
    for (const SubmaskRowInfo &r : infos) if (r.enabled) return true;
    return false;
}

void SubmaskList::setAllEnabled(bool on)
{
    /* Emitted per row (the owner has no bulk slot) and deferred a tick like every other
       emission here, since each one rebuilds these rows. */
    const int n = infos.size();
    emitDeferred([this, n, on]{
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
