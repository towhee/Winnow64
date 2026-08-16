#include "Develop/Properties/submasklist.h"
#include "Main/dockwidget.h"        // BarBtn
#include "Main/global.h"

#include <QAction>
#include <QCheckBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLinearGradient>
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
       property-header gradient behind it (same idiom as RawPanel / ScopeHeaderLab). */
    headerBand = new QWidget(this);
    headerBand->setAttribute(Qt::WA_TranslucentBackground);
    headerBand->setCursor(Qt::PointingHandCursor);
    headerBand->installEventFilter(this);        // a header click toggles collapse
    QHBoxLayout *hb = new QHBoxLayout(headerBand);
    hb->setContentsMargins(0, 3, 6, 3);
    hb->setSpacing(0);

    collapseBtn = new BarBtn();
    collapseBtn->setToolTip("Hide or show the submask list");
    collapseBtn->setIconSize(QSize(9, 9));
    collapseBtn->setFixedSize(9, 16);
    collapseBtn->setStyleSheet("QToolButton { border: none; padding: 0;"
                               " background: transparent; }");
    connect(collapseBtn, &BarBtn::clicked, this, [this]{ toggleCollapsed(); });

    titleLabel = new QLabel(tr("Submasks"), headerBand);
    titleLabel->setStyleSheet(G::labelCss(G::textColor, G::strFontSize.toInt()));

    addBtn = new BarBtn();
    addBtn->setToolTip("Add a submask to this mask (M)");
    addBtn->setIcon(":/images/icon16/addMask.png", G::iconOpacity);
    addBtn->setIconSize(QSize(16, 16));
    connect(addBtn, &BarBtn::clicked, this, [this]{ emit addRequested(); });

    hb->addWidget(collapseBtn);
    hb->addSpacing(G::decorationTitleGap);
    hb->addWidget(titleLabel);
    hb->addStretch(1);
    hb->addWidget(addBtn);
    outer->addWidget(headerBand);
    updateCollapseIcon();

    /* Rows container: one row widget per submask, rebuilt by setSubmasks. Translucent for
       the same reason ScopeHeaderLab's is -- under the app stylesheet a plain QWidget
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
    if (!headerBand) return;
    QPainter p(this);
    const int a = G::backgroundShade + 5;
    const int b = G::backgroundShade - 15;
    const QRect r = headerBand->geometry();
    QLinearGradient g(0, r.top(), 0, r.bottom());
    g.setColorAt(0, QColor(a, a, a));
    g.setColorAt(1, QColor(b, b, b));
    p.fillRect(r, g);
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
    hb->setContentsMargins(10, 1, 6, 1);
    hb->setSpacing(6);

    /* Show/hide this submask's contribution (MaskComponent::enabled). The renderer skips
       a disabled component, so this is a one-click A/B of what it is doing. */
    QCheckBox *cb = new QCheckBox(row);
    cb->setChecked(r.enabled);
    cb->setToolTip("Show or hide this submask's contribution to the mask");
    connect(cb, &QCheckBox::toggled, this, [this, index](bool on){
        emitDeferred([this, index, on]{ emit enabledToggled(index, on); });
    });
    hb->addWidget(cb);

    /* Op chip. The FIRST submask has nothing to combine with, so its op is inert and the
       chip is shown flat and disabled rather than hidden (the column stays aligned). */
    QPushButton *opBtn = new QPushButton(opGlyph(r.op), row);
    opBtn->setFixedSize(18, 18);
    opBtn->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    /* min-width: 0 defeats the global "QPushButton { min-width: 100px }" (widgetcss.cpp),
       which Qt applies as an EXPLICIT minimum and which would floor the dock's width. */
    opBtn->setStyleSheet("QPushButton { min-width: 0; padding: 0; border-radius: 9px; }");
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
    name->setToolTip(r.pending ? tr("Being built -- commit or cancel it")
                               : tr("Click to edit this submask again"));
    hb->addWidget(name);
    hb->addStretch(1);

    BarBtn *menuBtn = new BarBtn();
    menuBtn->setToolTip("Submask actions (edit, op, invert, move, duplicate, delete)");
    menuBtn->setIcon(":/images/icon16/ellipsis_vertical.png", G::iconOpacity);
    menuBtn->setIconSize(QSize(16, 16));
    connect(menuBtn, &BarBtn::clicked, this, [this, index]{ showRowMenu(index); });
    hb->addWidget(menuBtn);

    return row;
}

bool SubmaskList::eventFilter(QObject *watched, QEvent *event)
{
    /* A click on the header band (arrow or caption) toggles collapse; MouseButtonDblClick
       is included because Qt sends it INSTEAD of the second press, which would otherwise
       leave the list in the opposite state. A click on a row body selects that submask;
       the checkbox / op chip / menu button consume their own clicks. */
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

void SubmaskList::showRowMenu(int index)
{
    if (G::isLogger) G::log("SubmaskList::showRowMenu");
    if (index < 0 || index >= infos.size()) return;
    const SubmaskRowInfo r = infos.at(index);

    /* Read the choice as an int code while the menu is alive, then fire on the next tick:
       every action rebuilds these rows, deleting the button whose click we are inside. */
    enum { Edit = 1, OpAdd, OpSubtract, OpIntersect, Invert, Up, Down, Dup, Del };
    QMenu menu(this);
    menu.addAction(tr("Edit %1").arg(r.toolName))->setData(Edit);
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
