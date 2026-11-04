#ifndef PINNEDCOLUMNS_H
#define PINNEDCOLUMNS_H

#include <QHeaderView>
#include <QPainter>
#include <QToolTip>
#include <QHelpEvent>

/*
    PINNED COLUMNS -- a tree whose column 0 (the name) may be wider than the viewport,
    so it scrolls horizontally, while every other visible column (the counts) stays at
    the viewport's right edge. Used by FSTree (Folders) and Filters.

    Qt has no frozen columns, so the tree paints each row twice (its drawRow override):
    the row as laid out, clipped to the left of the strip, then the row translated by
    Geometry::delta so its pinned cells land in the strip, clipped to the strip. Its
    visualRect and indexAt overrides apply the same delta, so repaints, clicks and
    tooltips agree with what is drawn, and its scrollContentsBy repaints the whole
    viewport on a horizontal scroll, because Qt blits the viewport and would carry the
    strip with it.

    When column 0 fits, delta is 0 and nothing is changed.
*/
namespace PinnedColumns {

struct Geometry {
    int left = 0;       // viewport x of the pinned strip
    int delta = 0;      // added to a pinned column's laid-out x; 0 = nothing to pin
    bool isActive() const { return delta != 0; }
    bool isPinned(int column) const { return delta != 0 && column != 0; }
};

inline Geometry geometry(const QHeaderView *h, int viewportWidth)
{
/*
    The pinned columns are every visible column after column 0. Their strip sits where
    the layout puts them, or at the right edge if that is further left (column 0 is
    wider than the viewport and not scrolled fully right).
*/
    Geometry g;
    int first = -1;
    int width = 0;
    for (int v = 0; v < h->count(); ++v) {
        const int col = h->logicalIndex(v);
        if (col == 0 || h->isSectionHidden(col)) continue;
        if (first < 0) first = col;
        width += h->sectionSize(col);
    }
    if (first < 0) return g;
    const int laidOut = h->sectionViewportPosition(first);
    g.left = qMin(laidOut, viewportWidth - width);
    g.delta = g.left - laidOut;
    return g;
}

class Header : public QHeaderView
{
/*
    A tree header whose pinned sections stay above the pinned strip. Configured as
    QTreeView configures its own default header.
*/
public:
    explicit Header(QWidget *parent = nullptr) : QHeaderView(Qt::Horizontal, parent)
    {
        setSectionsMovable(true);
        setStretchLastSection(true);
        setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    }

protected:
    void paintEvent(QPaintEvent *event) override
    {
        QHeaderView::paintEvent(event);
        const Geometry g = PinnedColumns::geometry(this, viewport()->width());
        if (!g.isActive()) return;
        QPainter p(viewport());
        p.setClipRect(QRect(g.left, 0, viewport()->width() - g.left, viewport()->height()));
        for (int v = 0; v < count(); ++v) {
            const int col = logicalIndex(v);
            if (!g.isPinned(col) || isSectionHidden(col)) continue;
            const QRect r(sectionViewportPosition(col) + g.delta, 0,
                          sectionSize(col), viewport()->height());
            p.save();
            paintSection(&p, r, col);
            p.restore();
        }
    }

    bool viewportEvent(QEvent *event) override
    {
        // a section tooltip follows the section as drawn, not as laid out
        if (event->type() == QEvent::ToolTip) {
            QHelpEvent *he = static_cast<QHelpEvent *>(event);
            const Geometry g = PinnedColumns::geometry(this, viewport()->width());
            if (g.isActive() && he->pos().x() >= g.left) {
                const int col = logicalIndexAt(he->pos().x() - g.delta);
                const QVariant tip = model()
                    ? model()->headerData(col, orientation(), Qt::ToolTipRole) : QVariant();
                if (tip.isValid()) QToolTip::showText(he->globalPos(), tip.toString(), this);
                else QToolTip::hideText();
                return true;
            }
        }
        return QHeaderView::viewportEvent(event);
    }
};

} // namespace PinnedColumns

#endif // PINNEDCOLUMNS_H
