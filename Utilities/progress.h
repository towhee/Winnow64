#ifndef PROGRESS_H
#define PROGRESS_H

#include <QWidget>
#include <QString>
#include <QColor>
#include <QPixmap>
#include <QLinearGradient>
#include <QList>
#include "Main/global.h"

/*
    Progress is a self-contained container widget that shows any number of
    progress "rows".  Each row has two columns: a text label and a progress
    bar.  It replaced the older ProgressBar (a friend of MW that painted into an
    MW-owned pixmap); Progress instead owns all of its own state.

    Design goals:
    - One container holding many rows.  Each row has its own height, bar color
      and font.
    - Each row owns its own bar pixmap so incremental paints (e.g. the moving
      ImageCache cursor) only touch that row.
    - The container width is settable; the bars fit the available width.  The
      container height grows to fit the visible rows.
    - The background matches MW::statusBar.
    - Progress never references MW.  All host interaction is via public methods
      and the heightChanged() / clicked() signals, so it can be reparented into
      a standalone window later with no internal changes.

    Rows are added at runtime by whoever drives them: addRow() returns an int id,
    the owner holds it, and drives the row with the generic updateProgress(id, ...)
    / clearProgress(id).  A row's Fill style (FromStart vs Cell) decides how
    updateProgress paints, so no per-row update/clear function is needed.

    The ImageCache row is the one exception: its moving cursor and directional
    range fills are not expressible as a simple Fill, so it is created here and
    keeps its own updateImageCacheProgress()/updateCursor()/clearImageCacheProgress().
*/

class Progress : public QWidget
{
    Q_OBJECT
public:
    explicit Progress(QWidget *parent = nullptr);

    /* How a generic row's updateProgress() paints:
         FromStart - fill the bar from 0 up to the current item (sequential
                     progress, e.g. FocusStack, RawDenoise).
         Cell      - fill only the current item's cell (items complete out of
                     order, e.g. MetaRead reading metadata across threads). */
    enum class Fill { FromStart, Cell };

    /* Row management.  addRow() may be called at runtime; it returns the id used
       to address the row in the generic update/clear functions below. */
    int addRow(const QString &name, int barHeight, const QColor &color,
               Fill fill = Fill::FromStart);
    void setRowText(int id, const QString &text);
    void showRow(int id, bool visible);
    /* Enable/disable a row as a preference gate.  A disabled row stays hidden
       even when its update function requests it, without losing its painted
       content.  The host uses this to honor "Show caching progress" for the
       ImageCache + MetaRead rows while leaving other rows (e.g. FocusStack)
       free to appear. */
    void setRowEnabled(int id, bool enabled);

    /* Generic row driver (every row except the ImageCache row).  updateProgress
       reveals the row and paints per the row's Fill style; an invalid color uses
       the row's own bar color.  clearProgress refills the bar and hides the row. */
    void updateProgress(int id, int item, int items, QColor color = QColor());
    void clearProgress(int id);

    /* Clear and hide every row (e.g. when the folder is closed). */
    void reset();

    /* Force the whole container hidden regardless of row content (e.g. during a
       random slideshow). While suppressed, active rows stay laid out but the
       widget is not shown; clearing suppression re-shows it if any row is live.
       Progress otherwise manages its own visibility: it shows itself whenever a
       row is visible and hides itself when none are, so hosts never toggle the
       widget's visibility for a per-row concern. */
    void setSuppressed(bool suppressed);

    /* Id of the ImageCache row (created in the ctor), for the host's cache gate. */
    int imageCacheRow() const { return idImageCache; }

    /* Container configuration */
    void setContainerWidth(int w);
    void setTextFontSize(int pt);
    void setTextColumnWidth(int w);
    void setBackgroundColor(const QColor &bg);

    /* Geometry */
    int preferredHeight() const;
    QSize sizeHint() const override;

    /* ImageCache row update functions (ported from ProgressBar) */
    void clearImageCacheProgress();
    void updateImageCacheProgress(int fromItem, int toItem, int items,
                                  QLinearGradient gradient);
    void updateCursor(int item, int items);

    /* Gradient helpers / palette (referenced by MW) */
    QLinearGradient getGradient(QColor c1, int barHeight);

    QColor cursorColor;
    QColor bgColor;
    QColor targetColor;
    QColor imageCacheColor;
    QColor metaReadCacheColor;

    QLinearGradient bgGradient;
    QLinearGradient imageCacheGradient;
    QLinearGradient cursorGradient;
    QLinearGradient targetColorGradient;

signals:
    void heightChanged(int newHeight);
    void clicked();

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;

private:
    struct Row {
        QString name;            // identifier key
        QString text;            // label, empty by default
        int     fontSize;        // point size for this row's text
        int     barHeight;       // pixel height of the bar
        QColor  barColor;        // base color
        Fill    fill = Fill::FromStart; // how updateProgress paints this row
        bool    active = false;  // row has content to show (set by update fns)
        bool    enabled = true;  // preference gate (see setRowEnabled)
        bool    visible = false; // effective = active && enabled (used by layout/paint)
        QPixmap bar;             // backing store for the bar column
        int     top = 0;         // computed y within container
        int     height = 0;      // computed row height
    };

    void applyRowVisibility(int i); // recompute Row::visible from active && enabled
    void updateContainerVisibility(); // show/hide widget from row content + suppressed
    int rowIndex(int id) const;          // bounds-checked, -1 if invalid
    int barWidth() const;                // container width minus text column
    int barX() const;                    // x where the bar column starts
    int barNudge() const;                // px to nudge the bar down vs the text (platform-specific)
    int singleProgressItemNudge() const; // extra px to nudge the row when only one is visible (platform-specific)
    int effectiveTextColWidth() const;   // explicit textColWidth, else auto-fit
    void rebuildRowPixmap(Row &r);       // (re)create r.bar at current barWidth
    void fillBarBackground(Row &r);      // fill r.bar with the bg gradient (so progress shows against it)
    void relayout();                     // recompute tops/heights, emit height
    void rebuildGradients();             // bg/cursor/imageCache/target gradients

    QList<Row> rows;
    int idImageCache = -1;
    bool suppressed = false;             // force-hide the widget regardless of rows

    int containerWidth = 200;            // total widget width
    int textColWidth = 0;                // column 1 width (0 = bars use full width)
    int textGap = 6;                     // gap between text and bar columns
    int textFontSize = 8;                // 0 = use widget font
    int vGap = 3;                        // vertical gap between rows (not after last)
    int topPad = 2;                      // top margin: keeps a single row centered with other status text
    int botPad = 2;                      // bottom margin: matches topPad so content stays centered

    int minCursorWidth = 2;              // min px width of the cache cursor
    int icBarHeight = 8;                 // remembered ImageCache bar height
    int prevCursorPos = 0;               // last painted cache cursor position
    int rowsBlockHeight = 0;             // height of the visible rows (no margins)
};

#endif // PROGRESS_H
