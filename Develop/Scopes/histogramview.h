#ifndef HISTOGRAMVIEW_H
#define HISTOGRAMVIEW_H

#include <QWidget>
#include "Develop/Scopes/scopedata.h"

/*
    A live RGB + luma histogram of the image currently shown in the loupe. setData()
    copies the 256-bin counts and repaints; clear() blanks it (no image). The three
    colour channels are drawn as filled polylines with an additive blend on a dark
    ground so overlaps read as the mixed colour, with luma as a faint outline.

    Counts come from MW::updateDevelopScopes (a strided sample of the displayed image),
    so cost is independent of the file's resolution.
*/
class HistogramView : public QWidget
{
    Q_OBJECT
public:
    explicit HistogramView(QWidget *parent = nullptr);
    void setData(const ScopeData &d);
    void clear();
    void setMarker(int r, int g, int b);   // pixel under the loupe cursor: vertical channel ticks
    void clearMarker();
    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;
    /* Consume double-clicks so they do not bubble to the dock (which would un/redock). */
    void mouseDoubleClickEvent(QMouseEvent *event) override;

private:
    quint32 hist[4][256];
    bool hasData = false;
    int marker[3] = { 0, 0, 0 };   // cursor pixel R,G,B
    bool hasMarker = false;
    /* Opacity of the neutral core -- the per-bin min of R,G,B, where all three channels overlap
       and blend to white. 1.0 = the classic full-white core, 0.0 = hidden (only the colour
       excess shows), 0.5 = a dimmed grey core. The colour bands above the core are unaffected. */
    double whiteOpacity = 0.5;
};

#endif // HISTOGRAMVIEW_H
