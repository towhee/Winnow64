#include "Develop/Properties/scopeheaderlab.h"
#include "Main/dockwidget.h"        // BarBtn
#include "Main/global.h"

#include <QLabel>
#include <QCheckBox>
#include <QMenu>
#include <QAction>
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

ScopeHeaderLab::ScopeHeaderLab(QWidget *parent) : ScopeHeaderBase(parent)
{
    if (G::isLogger) G::log("ScopeHeaderLab::ScopeHeaderLab");

    /* ellipsis_vertical.png: the vertical "kebab" glyph for the panel and per-row scope
       action menus. Drawn gray 176 like questionmark.png and dimmed by the same
       G::iconOpacity (via BarBtn::setIcon(path, opacity)) so it reads at the identical
       brightness as the "?" tip buttons above it. */

    QVBoxLayout *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    /* Header band: "Scopes" caption + a trailing [v] panel menu. Transparent background
       so ScopeHeaderLab::paintEvent can draw the property-header gradient behind it. */
    headerBand = new QWidget(this);
    headerBand->setAttribute(Qt::WA_TranslucentBackground);
    headerBand->setCursor(Qt::PointingHandCursor);
    headerBand->installEventFilter(this);        // a header click toggles collapse
    QHBoxLayout *hb = new QHBoxLayout(headerBand);
    hb->setContentsMargins(0, 3, 6, 3);
    hb->setSpacing(0);
    /* Collapse arrow (branch glyph) in a gutter-width button, matching the tree's section
       headers + the Raw panel: click it (or the caption) to hide/show the list. */
    collapseBtn = new BarBtn();
    collapseBtn->setToolTip("Hide or show the scope list");
    collapseBtn->setIconSize(QSize(9, 9));
    collapseBtn->setFixedSize(10, 16);
    collapseBtn->setStyleSheet("QToolButton { border: none; padding: 0; background: transparent; }");
    connect(collapseBtn, &BarBtn::clicked, this, [this]{ toggleListCollapsed(); });
    titleLabel = new QLabel(tr("Scope"), headerBand);
    titleLabel->setStyleSheet(QString("color: %1; font-size: %2pt; background: transparent;")
                                  .arg(G::header2Color.name()).arg(G::strFontSize.toInt()));
    panelMenuBtn = new BarBtn();
    panelMenuBtn->setToolTip("Scope actions (new mask)");
    panelMenuBtn->setIcon(":/images/icon16/ellipsis_vertical.png", G::iconOpacity);
    panelMenuBtn->setIconSize(QSize(16, 16));
    connect(panelMenuBtn, &BarBtn::clicked, this, [this]{ showPanelMenu(); });
    hb->addWidget(collapseBtn);
    hb->addWidget(titleLabel);
    hb->addStretch(1);
    hb->addWidget(panelMenuBtn);
    outer->addWidget(headerBand);
    updateListCollapseIcon();

    /* Rows container: one row widget per scope, rebuilt by setScopeRows. */
    rowsContainer = new QWidget(this);
    rowsLayout = new QVBoxLayout(rowsContainer);
    rowsLayout->setContentsMargins(0, 0, 0, 2);
    rowsLayout->setSpacing(0);
    outer->addWidget(rowsContainer);

    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
}

void ScopeHeaderLab::paintEvent(QPaintEvent *)
{
    /* The property-header gradient (backgroundShade +5 -> -15), behind the header band
       only; the rows below sit on the dock background. */
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
    for (const ScopeRowInfo &r : rows) names << r.name;
    rebuild(rows, activeIndex);
}

void ScopeHeaderLab::rebuild(const QVector<ScopeRowInfo> &rows, int active)
{
    /* Drop the old rows (widgets own their children; deleteLater is unnecessary here as
       nothing captures them beyond this rebuild). */
    while (QLayoutItem *it = rowsLayout->takeAt(0)) {
        if (QWidget *w = it->widget()) w->deleteLater();
        delete it;
    }
    for (int i = 0; i < rows.size(); ++i)
        rowsLayout->addWidget(makeRow(i, rows.at(i), i == active));
}

QWidget *ScopeHeaderLab::makeRow(int index, const ScopeRowInfo &r, bool active)
{
    QWidget *row = new QWidget(rowsContainer);
    row->setProperty("scopeName", r.name);      // read back by the row-body click filter
    row->installEventFilter(this);
    /* Active row: a selectionColor band + white caption; others transparent. */
    if (active)
        row->setStyleSheet(QString("background: %1;").arg(G::selectionColor.name()));

    QHBoxLayout *hb = new QHBoxLayout(row);
    hb->setContentsMargins(10, 2, 6, 2);
    hb->setSpacing(6);

    /* Every scope -- Global included -- gets a show/hide checkbox that toggles
       EditScope::enabled; the compositor skips a disabled scope (Global off => the image
       renders with no develop adjustments). */
    QCheckBox *cb = new QCheckBox(row);
    cb->setChecked(r.enabled);
    cb->setToolTip("Show or hide this scope's edits");
    connect(cb, &QCheckBox::toggled, this,
            [this, index](bool on){ emit scopeEnabledToggled(index, on); });
    hb->addWidget(cb);

    QLabel *name = new QLabel(r.name, row);
    name->setStyleSheet(QString("color: %1; font-size: %2pt; background: transparent;")
                            .arg((active ? QColor(Qt::white) : G::textColor).name())
                            .arg(G::strFontSize.toInt()));
    hb->addWidget(name);
    hb->addStretch(1);

    if (!r.isGlobal) {
        BarBtn *menuBtn = new BarBtn();
        menuBtn->setToolTip("Scope actions (add to mask, reset, remove, rename)");
        menuBtn->setIcon(":/images/icon16/ellipsis_vertical.png", G::iconOpacity);
        menuBtn->setIconSize(QSize(16, 16));
        const QString nm = r.name;
        connect(menuBtn, &BarBtn::clicked, this,
                [this, index, nm]{ showRowMenu(index, nm); });
        hb->addWidget(menuBtn);
    }

    return row;
}

bool ScopeHeaderLab::eventFilter(QObject *watched, QEvent *event)
{
    /* A single left click anywhere on the header band (caption or empty area) toggles the
       list collapse; the arrow + [v] buttons consume their own clicks. A click on a row
       body (incl. its name label, which ignores the press so it propagates up) selects
       that scope; checkbox / menu-button clicks are consumed by those widgets. */
    if (event->type() == QEvent::MouseButtonPress && watched == headerBand) {
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

void ScopeHeaderLab::showPanelMenu()
{
    QMenu menu(this);
    connect(menu.addAction(tr("New mask\tN")), &QAction::triggered,
            this, [this]{ emit addScopeRequested(); });
    menu.exec(QCursor::pos());
}

void ScopeHeaderLab::showRowMenu(int index, const QString &name)
{
    if (G::isLogger) G::log("ScopeHeaderLab::showRowMenu");

    /* Per-scope actions act on the ACTIVE scope in DevelopProperties, so a row menu first
       selects its own row. Read the choice as an int code while the menu is alive, then
       fire everything on the next tick (the select rebuilds the rows, deleting this
       button -- must not happen inside its own click handler). */
    enum { AddMask = 1, Reset, Remove, Rename };
    QMenu menu(this);
    QAction *aMask   = menu.addAction(tr("Add to %1\tM").arg(name));
    aMask->setData(AddMask);
    menu.addSeparator();
    menu.addAction(tr("Reset %1").arg(name))->setData(Reset);
    menu.addAction(tr("Remove %1").arg(name))->setData(Remove);
    menu.addAction(tr("Rename %1").arg(name))->setData(Rename);

    QAction *chosen = menu.exec(QCursor::pos());
    const int code = chosen ? chosen->data().toInt() : 0;
    if (code == 0) return;

    QTimer::singleShot(0, this, [this, name, code]{
        emit scopeSelected(name);            // make this the active scope first
        switch (code) {
            case AddMask: emit addMaskRequested();     break;
            case Reset:   emit resetScopeRequested();  break;
            case Remove:  emit removeScopeRequested();  break;
            case Rename:  emit renameRequested();      break;
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
