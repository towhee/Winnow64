#include "progress.h"
#include <QPainter>
#include <QPaintEvent>
#include <QMouseEvent>
#include <QFontMetrics>

/*
    See progress.h for the overall design.

    Each row owns a QPixmap (Row::bar) that is the backing store for its bar
    column.  The incremental update functions paint into that pixmap and then
    call update(), so paintEvent only has to blit the pixmaps and draw the text
    labels.  The container background is painted directly in paintEvent, so a
    background change is just setBackgroundColor() + update().
*/

Progress::Progress(QWidget *parent) : QWidget(parent)
{
    setObjectName("Progress");

    /* Palette ported from ProgressBar so colors match the previous look. */
    int b = G::backgroundShade + 20;

    // cursor color
    int cg = 120;
    int cr = cg * 0.67;
    int cb = cr;

    // metaRead color
    int mBrightness = 100;
    int mr = mBrightness * 0.87;
    int mg = mBrightness * 0.17;
    int mb = mBrightness * 0.44;

    // imageCache color
    int ig = cg * 0.8;
    int ir = ig * 0.67;
    int ib = ir;

    // target color
    int t = G::backgroundShade + 20;

    bgColor            = QColor(b, b, b);
    cursorColor        = QColor(cr, cg, cb);
    targetColor        = QColor(t, t, t);
    metaReadCacheColor = QColor(mr, mg, mb);
    imageCacheColor    = QColor(ir, ig, ib);

    icBarHeight = 8;
    rebuildGradients();

    /* Build the three rows that mirror the previous ProgressBar strips. */
    idImageCache = addRow("ImageCache", icBarHeight, imageCacheColor);
    idMetaRead   = addRow("MetaRead", 4, metaReadCacheColor);
    idFocusStack = addRow("FocusStack", 2, QColor(Qt::darkYellow));
    idRawDenoise = addRow("FocusStack", 2, QColor(G::darkorange));

    /* All rows start hidden (Row::active defaults false) and are only shown while
       active, so at startup (no folder selected) the container is empty. The
       update functions show their row; the MetaRead/FocusStack clear functions
       hide theirs again. This lets the container collapse to fit just the live
       rows. */
    setRowText(idImageCache, "Image Cache");
    setRowText(idMetaRead, "Metadata");
    setRowText(idFocusStack, "Focus Stack");
    setRowText(idRawDenoise, "Raw Denoise");
}

/* ------------------------------------------------------------------ helpers */

int Progress::rowIndex(int id) const
{
    if (id < 0 || id >= rows.size()) return -1;
    return id;
}

int Progress::effectiveTextColWidth() const
{
    if (textColWidth > 0) return textColWidth;   // explicit override
    // auto-fit: width of the widest visible, non-empty label
    int w = 0;
    QFont f = font();
    for (const Row &r : rows) {
        if (!r.visible || r.text.isEmpty()) continue;
        int fs = r.fontSize > 0 ? r.fontSize : textFontSize;
        if (fs > 0) f.setPointSize(fs);
        w = qMax(w, QFontMetrics(f).horizontalAdvance(r.text));
    }
    return w;
}

int Progress::barX() const
{
    int tw = effectiveTextColWidth();
    return tw > 0 ? tw + textGap : 0;
}

int Progress::barWidth() const
{
    int w = containerWidth - barX();
    return w > 1 ? w : 1;
}

int Progress::barNudge() const
{
    /* Pixels to nudge the bar down relative to the status text so it aligns with
       the text baseline. The text sits lower on Windows, so it needs more nudge. */
#ifdef Q_OS_WIN
    return 2;
#else
    return 0;
#endif
}

int Progress::singleProgressItemNudge() const
{
    /* When only one progress row is visible the container is short, so shift the
       whole row down to vertically center it against the MetaRead/ImageCache
       activity labels to the right of the progress in the status bar. */
#ifdef Q_OS_WIN
    return 1;
#else
    return 1;
#endif
}

void Progress::rebuildRowPixmap(Row &r)
{
    r.bar = QPixmap(barWidth(), r.barHeight);
    fillBarBackground(r);
}

void Progress::fillBarBackground(Row &r)
{
    /* Fill the bar's empty area with the bg gradient (not a flat color) so the
       progress painted over it remains visible against the background. The
       gradient is sized to this row's bar height so it spans the full bar. */
    QPainter pnt(&r.bar);
    pnt.fillRect(r.bar.rect(), getGradient(bgColor, r.barHeight));
    pnt.end();
}

void Progress::rebuildGradients()
{
    bgGradient          = getGradient(bgColor, icBarHeight);
    cursorGradient      = getGradient(cursorColor, icBarHeight);
    targetColorGradient = getGradient(targetColor, icBarHeight);
    imageCacheGradient  = getGradient(imageCacheColor, icBarHeight);
}

QLinearGradient Progress::getGradient(QColor c1, int barHeight)
{
    QLinearGradient grad;
    grad.setStart(0, 0);
    grad.setFinalStop(0, barHeight);
    int r, g, b;
    c1.red()   >= 50 ? r = c1.red()   - 50 : r = 0;
    c1.green() >= 50 ? g = c1.green() - 50 : g = 0;
    c1.blue()  >= 50 ? b = c1.blue()  - 50 : b = 0;
    QColor c2(r, g, b);
    grad.setColorAt(0, c1);
    grad.setColorAt(1, c2);
    return grad;
}

void Progress::relayout()
{
    /* Lay the rows out block-relative (starting at 0). paintEvent centers the
       whole block within the widget's actual height, so the top and bottom
       margins are always equal regardless of how the status bar sizes us. */
    int y = 0;
    bool first = true;
    QFont f = font();
    for (Row &r : rows) {
        if (!r.visible) continue;
        if (!first) y += vGap;          // gap between rows only, not above the first
        first = false;
        int fs = r.fontSize > 0 ? r.fontSize : textFontSize;
        if (fs > 0) f.setPointSize(fs);
        int textHt = r.text.isEmpty() ? 0 : QFontMetrics(f).height();
        r.top = y;
        r.height = qMax(textHt, r.barHeight);
        y += r.height;
    }
    rowsBlockHeight = y;
    /* Reserve topPad+botPad so the status bar grows enough to leave the margins
       visible when Progress is the tallest widget (fills the bar). */
    int total = topPad + y + botPad;
    setFixedHeight(total);
    updateGeometry();
    emit heightChanged(total);
}

/* -------------------------------------------------------------- row config */

int Progress::addRow(const QString &name, int barHeight, const QColor &color)
{
    Row r;
    r.name = name;
    r.fontSize = 0;
    r.barHeight = barHeight;
    r.barColor = color;
    rebuildRowPixmap(r);
    rows.append(r);
    relayout();
    return rows.size() - 1;
}

void Progress::setRowText(int id, const QString &text)
{
    int i = rowIndex(id);
    if (i < 0) return;
    if (rows[i].text == text) return;
    rows[i].text = text;
    /* Adding/removing a label changes the text-column width, which changes the
       bar width, so the bar pixmaps must be resized. */
    for (Row &r : rows) rebuildRowPixmap(r);
    relayout();
    update();
}

void Progress::showRow(int id, bool visible)
{
    int i = rowIndex(id);
    if (i < 0) return;
    rows[i].active = visible;
    applyRowVisibility(i);
}

void Progress::setRowEnabled(int id, bool enabled)
{
    int i = rowIndex(id);
    if (i < 0) return;
    rows[i].enabled = enabled;
    applyRowVisibility(i);
}

void Progress::setCacheRowsEnabled(bool enabled)
{
    setRowEnabled(idImageCache, enabled);
    setRowEnabled(idMetaRead, enabled);
}

void Progress::applyRowVisibility(int i)
{
    bool eff = rows[i].active && rows[i].enabled;
    if (rows[i].visible == eff) return;
    rows[i].visible = eff;
    relayout();
    update();
}

void Progress::setContainerWidth(int w)
{
    if (w < 1) w = 1;
    setFixedWidth(w);                    // always assert the fixed width
    if (w == containerWidth) return;     // pixmaps already sized for this width
    containerWidth = w;
    for (Row &r : rows) rebuildRowPixmap(r);
    update();
}

void Progress::setTextFontSize(int pt)
{
    textFontSize = pt;
    relayout();
    update();
}

void Progress::setTextColumnWidth(int w)
{
    if (w < 0) w = 0;
    if (w == textColWidth) return;
    textColWidth = w;
    for (Row &r : rows) rebuildRowPixmap(r);
    update();
}

void Progress::setBackgroundColor(const QColor &bg)
{
    bgColor = bg;
    rebuildGradients();
    /* Refill the bars so their empty background tracks the container. Any live
       cache progress is repainted by MW's next ImageCache::updateStatus. */
    for (Row &r : rows) rebuildRowPixmap(r);
    update();
}

/* ---------------------------------------------------------------- geometry */

int Progress::preferredHeight() const
{
    int y = topPad;
    bool first = true;
    QFont f = font();
    for (const Row &r : rows) {
        if (!r.visible) continue;
        if (!first) y += vGap;
        first = false;
        int fs = r.fontSize > 0 ? r.fontSize : textFontSize;
        if (fs > 0) f.setPointSize(fs);
        int textHt = r.text.isEmpty() ? 0 : QFontMetrics(f).height();
        y += qMax(textHt, r.barHeight);
    }
    return y + botPad;
}

QSize Progress::sizeHint() const
{
    return QSize(containerWidth, preferredHeight());
}

/* --------------------------------------------------------------- painting */

void Progress::paintEvent(QPaintEvent * /*event*/)
{
    QPainter p(this);
    p.fillRect(rect(), bgColor);

    /* Center the rows block within the actual widget height so the top and
       bottom margins are equal whatever height the status bar gives us. */
    int yOff = (height() - rowsBlockHeight) / 2;
    if (yOff < 0) yOff = 0;

    /* When only one row is visible, nudge the whole row (text + bar) down to
       vertically align with the MetaRead/ImageCache activity labels to the right
       of the progress in the status bar. */
    int visibleCount = 0;
    for (const Row &r : rows) if (r.visible) ++visibleCount;
    if (visibleCount == 1) yOff += singleProgressItemNudge();

    QFont f = font();
    for (const Row &r : rows) {
        if (!r.visible) continue;
        // bar column (bar centered vertically within its row, nudged down 1px
        // relative to the text so it aligns with the status text baseline)
        int barY = r.top + yOff + (r.height - r.barHeight) / 2 + barNudge();
        p.drawPixmap(barX(), barY, r.bar);
        // text column
        int tw = effectiveTextColWidth();
        if (!r.text.isEmpty() && tw > 0) {
            int fs = r.fontSize > 0 ? r.fontSize : textFontSize;
            if (fs > 0) f.setPointSize(fs);
            p.setFont(f);
            p.setPen(Qt::lightGray);
            QRect textRect(0, r.top + yOff, tw, r.height);
            p.drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft, r.text);
        }
    }
}

void Progress::mousePressEvent(QMouseEvent * /*event*/)
{
    emit clicked();
}

/* ----------------------------------------------------- ImageCache row paint */

void Progress::clearImageCacheProgress()
{
    int i = rowIndex(idImageCache);
    if (i < 0) return;
    fillBarBackground(rows[i]);
    update();
}

void Progress::updateImageCacheProgress(int fromItem, int toItem, int items,
                                        QLinearGradient gradient)
{
    int i = rowIndex(idImageCache);
    if (i < 0 || items <= 0) return;
    showRow(idImageCache, true);    // reveal once there is real cache progress
    Row &r = rows[i];
    QPainter pnt(&r.bar);
    int bw = barWidth();
    float itemWidth = static_cast<float>(bw) / items;
    int pxStart, pxWidth;

    // to and from can be mixed depending on direction of travel
    if (fromItem < toItem) {
        pxWidth = qRound((toItem - fromItem) * itemWidth) + itemWidth + 1;
        pxStart = qRound(fromItem * itemWidth);
        if (pxStart + pxWidth > bw) pxWidth = bw - pxStart;
    }
    else {
        pxWidth = qRound((fromItem - toItem) * itemWidth) + itemWidth + 1;
        pxStart = qRound(toItem * itemWidth);
    }
    if (pxWidth < 2) pxWidth = 2;
    if (pxStart + pxWidth > bw) pxWidth = bw - pxStart;

    QRect doneRect(pxStart, 0, pxWidth, r.barHeight);
    pnt.fillRect(doneRect, gradient);
    pnt.end();
    update();
}

void Progress::updateCursor(int item, int items)
{
    int i = rowIndex(idImageCache);
    if (i < 0 || items <= 0) return;
    showRow(idImageCache, true);    // reveal once there is real cache progress
    Row &r = rows[i];

    int pos = prevCursorPos;

    QPainter pnt(&r.bar);
    int bw = barWidth();
    float itemWidth = static_cast<float>(bw) / items;
    int pxStartOld = qRound(pos * itemWidth);
    int pxWidth = static_cast<int>(itemWidth) + 1;
    if (pxWidth < minCursorWidth) pxWidth = minCursorWidth;

    // paint out the old cursor location (over-paint with cached green)
    QRect oldRect(pxStartOld, 0, pxWidth, r.barHeight);
    pnt.fillRect(oldRect, imageCacheGradient);

    pos = item;
    int pxStartNew = qRound(pos * itemWidth);

    // paint in the new cursor location
    QRect newRect(pxStartNew, 0, pxWidth, r.barHeight);
    pnt.fillRect(newRect, cursorGradient);
    pnt.end();
    update();

    prevCursorPos = pos;
}

/* ------------------------------------------------------ MetaRead row paint */

void Progress::clearMetaReadProgress()
{
    int i = rowIndex(idMetaRead);
    if (i < 0) return;
    fillBarBackground(rows[i]);
    showRow(idMetaRead, false);   // collapse the row when inactive
    update();
}

void Progress::updateMetaReadProgress(int item, int items, QColor color)
{
    int i = rowIndex(idMetaRead);
    if (i < 0 || items <= 0) return;
    showRow(idMetaRead, true);    // reveal the row while active
    Row &r = rows[i];
    QPainter pnt(&r.bar);
    float itemWidth = static_cast<float>(barWidth()) / items;
    int pxStart = qRound(item * itemWidth);
    int pxWidth = itemWidth + 1;

    QRect doneRect(pxStart, 0, pxWidth, r.barHeight);
    pnt.fillRect(doneRect, color);
    pnt.end();
    update();
}

/* ---------------------------------------------------- FocusStack row paint */

void Progress::clearFocusStackProgress()
{
    int i = rowIndex(idFocusStack);
    if (i < 0) return;
    fillBarBackground(rows[i]);
    showRow(idFocusStack, false);   // collapse the row when inactive
    update();
}

void Progress::updateFocusStackProgress(int item, int items, QColor color)
{
    int i = rowIndex(idFocusStack);
    if (i < 0 || items <= 0) return;
    showRow(idFocusStack, true);    // reveal the row while active
    Row &r = rows[i];
    QPainter pnt(&r.bar);
    float itemWidth = static_cast<float>(barWidth()) / items;
    int pxStart = qRound(item * itemWidth);
    int pxWidth = itemWidth + 1;
    int pxEnd = pxStart + pxWidth;

    // focus stack fills from the start of the bar up to the current item
    QRect doneRect(0, 0, pxEnd, r.barHeight);
    pnt.fillRect(doneRect, color);
    pnt.end();
    update();
}
