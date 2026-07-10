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
    - One container holding many rows.  Each row has its own height, bar color,
      font and dedicated update function (e.g. updateImageCacheProgress).
    - Each row owns its own bar pixmap so incremental paints (e.g. the moving
      ImageCache cursor) only touch that row.
    - The container width is settable; the bars fit the available width.  The
      container height grows to fit the visible rows.
    - The background matches MW::statusBar.
    - Progress never references MW.  All host interaction is via public methods
      and the heightChanged() / clicked() signals, so it can be reparented into
      a standalone window later with no internal changes.

    A row is created once with addRow() and is afterwards addressed by the int
    id it returns.  Adding a new indicator is just: addRow() at setup, plus a
    dedicated update function that paints into that row's bar.
*/

class Progress : public QWidget
{
    Q_OBJECT
public:
    explicit Progress(QWidget *parent = nullptr);

    /* Row management */
    int addRow(const QString &name, int barHeight, const QColor &color);
    void setRowText(int id, const QString &text);
    void showRow(int id, bool visible);
    /* Enable/disable the cache rows (ImageCache + MetaRead) as a group. When
       disabled they stay hidden even if their update functions request them, so
       the cache rows honor the "Show caching progress" preference while the
       FocusStack row can still appear (e.g. during a focus stack). */
    void setCacheRowsEnabled(bool enabled);

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

public slots:
    void clearMetaReadProgress();
    void updateMetaReadProgress(int item, int items, QColor color = Qt::blue);
    void clearFocusStackProgress();
    void updateFocusStackProgress(int item, int items, QColor color = Qt::blue);

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
        bool    active = false;  // row has content to show (set by update fns)
        bool    enabled = true;  // preference gate (see setCacheRowsEnabled)
        bool    visible = false; // effective = active && enabled (used by layout/paint)
        QPixmap bar;             // backing store for the bar column
        int     top = 0;         // computed y within container
        int     height = 0;      // computed row height
    };

    void setRowEnabled(int id, bool enabled);
    void applyRowVisibility(int i); // recompute Row::visible from active && enabled
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
    int idMetaRead = -1;
    int idFocusStack = -1;
    int idRawDenoise = -1;

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
