#include "Develop/History/historyview.h"
#include "Develop/History/develophistory.h"
#include "Main/global.h"

#include <QFontMetrics>
#include <QMouseEvent>
#include <QPainter>
#include <QTimer>

/*
    HistoryView / HistoryDelegate (see historyview.h): the Develop History dock's list of
    edit actions. Newest first; hover previews a state, a click reverts to it.
*/

/* ------------------------------------------------------------------------------------
   HistoryDelegate
   ------------------------------------------------------------------------------------ */

HistoryDelegate::HistoryDelegate(QObject *parent) : QStyledItemDelegate(parent) {}

void HistoryDelegate::setHoveredRow(int row)
{
    hoveredRow = row;
}

void HistoryDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option,
                            const QModelIndex &index) const
{
    painter->save();

    const int state = index.data(StateRole).toInt();
    const QString action = index.data(Qt::DisplayRole).toString();
    const QString value  = index.data(ValueRole).toString();

    /* Background: the applied state reads as selected; a hovered row is lightened
       because hovering is live -- the loupe is showing that state. */
    if (state == Current) {
        painter->fillRect(option.rect, G::selectionColor);
    }
    else if (index.row() == hoveredRow) {
        const int b = G::backgroundShade + 20;
        painter->fillRect(option.rect, QColor(b, b, b));
    }

    QColor fg = G::textColor;
    QColor vg = G::header2Color;
    if (state == Future) {          // superseded: still listed until the next edit
        fg = G::disabledColor;
        vg = G::disabledColor;
    }
    else if (state == Current) {
        fg = Qt::white;
    }

    const QFontMetrics fm(option.font);
    QRect r = option.rect.adjusted(8, 0, -8, 0);

    /* Value first: it is short and must never be squeezed out, so the action elides
       into whatever is left. */
    int valW = 0;
    if (!value.isEmpty()) {
        valW = fm.horizontalAdvance(value) + 10;
        painter->setPen(vg);
        painter->drawText(r, Qt::AlignRight | Qt::AlignVCenter, value);
    }
    painter->setPen(fg);
    const QRect ar = r.adjusted(0, 0, -valW, 0);
    painter->drawText(ar, Qt::AlignLeft | Qt::AlignVCenter,
                      fm.elidedText(action, Qt::ElideRight, ar.width()));

    painter->restore();
}

QSize HistoryDelegate::sizeHint(const QStyleOptionViewItem &option,
                                const QModelIndex &index) const
{
    QSize s = QStyledItemDelegate::sizeHint(option, index);
    s.setHeight(QFontMetrics(option.font).height() + 8);
    return s;
}

/* ------------------------------------------------------------------------------------
   HistoryView
   ------------------------------------------------------------------------------------ */

HistoryView::HistoryView(QWidget *parent) : QListWidget(parent)
{
    if (G::isLogger) G::log("HistoryView::HistoryView");

    delegate = new HistoryDelegate(this);
    setItemDelegate(delegate);
    setSelectionMode(QAbstractItemView::NoSelection);   // position IS the selection
    setFocusPolicy(Qt::NoFocus);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setUniformItemSizes(true);
    setMouseTracking(true);

    /* Debounce the hover: a fast sweep down the list must not queue one develop render
       per row. Only a settled hover previews. */
    hoverTimer = new QTimer(this);
    hoverTimer->setSingleShot(true);
    connect(hoverTimer, &QTimer::timeout, this, [this]{
        const int i = entryForRow(hoverRow);
        if (i < 0) return;
        hoverEmitted = true;
        emit entryHovered(i);
    });
}

void HistoryView::bindHistory(DevelopHistory *h)
{
    history = h;
    if (!history) return;
    connect(history, &DevelopHistory::changed, this, [this](const QString &path){
        if (path.isEmpty() || path == imagePath) refresh();
    });
}

void HistoryView::setImage(const QString &path)
{
    imagePath = path;
    setHover(-1);
    refresh();
}

void HistoryView::refresh()
{
    clear();
    if (!history || imagePath.isEmpty()) return;

    const int n = history->count(imagePath);
    const int pos = history->pos(imagePath);
    /* Newest first (Lightroom order): row 0 is the last action, the baseline is last. */
    for (int i = n - 1; i >= 0; --i) {
        const HistoryEntry *e = history->at(imagePath, i);
        if (!e) continue;
        QString action = e->action;
        if (!e->scope.isEmpty()) action = e->scope + ": " + action;
        QListWidgetItem *item = new QListWidgetItem(action, this);
        item->setData(HistoryDelegate::ValueRole, e->value);
        item->setData(HistoryDelegate::EntryRole, i);
        item->setData(HistoryDelegate::StateRole,
                      i == pos ? HistoryDelegate::Current
                               : (i > pos ? HistoryDelegate::Future
                                          : HistoryDelegate::Past));
        item->setToolTip(e->value.isEmpty() ? action : action + "   " + e->value);
    }
    scrollToTop();          // the newest action is what the user just did
}

int HistoryView::entryForRow(int row) const
{
    const QListWidgetItem *it = item(row);
    if (!it) return -1;
    return it->data(HistoryDelegate::EntryRole).toInt();
}

void HistoryView::setHover(int row)
{
    if (row == hoverRow) return;
    hoverRow = row;
    delegate->setHoveredRow(row);
    viewport()->update();
    if (row < 0) hoverTimer->stop();
    else         hoverTimer->start(kHoverDelayMs);
}

void HistoryView::mouseMoveEvent(QMouseEvent *event)
{
    setHover(indexAt(event->pos()).row());
    QListWidget::mouseMoveEvent(event);
}

void HistoryView::leaveEvent(QEvent *event)
{
    setHover(-1);
    if (hoverEmitted) {
        hoverEmitted = false;
        emit hoverEnded();          // put the real state back on the loupe
    }
    QListWidget::leaveEvent(event);
}

void HistoryView::mouseReleaseEvent(QMouseEvent *event)
{
    const int i = entryForRow(indexAt(event->pos()).row());
    QListWidget::mouseReleaseEvent(event);
    if (i < 0 || event->button() != Qt::LeftButton) return;
    /* End the hover preview first: applying the entry makes it the real state. Deferred
       a tick because applying rebuilds the Develop tree from inside this handler. */
    hoverEmitted = false;
    QTimer::singleShot(0, this, [this, i]{ emit entryChosen(i); });
}
