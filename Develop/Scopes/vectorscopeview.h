#ifndef VECTORSCOPEVIEW_H
#define VECTORSCOPEVIEW_H

#include <QWidget>
#include "Develop/Scopes/scopedata.h"

class QPainter;
class QMenu;

/*
    A live vectorscope of the image currently shown in the loupe: a 2-D Cb/Cr
    accumulation drawn on a dark circular graticule. Hue is angle, saturation is radius
    from the neutral centre, so casts and over-saturation read at a glance (handy for
    white balance). setData() copies the VN x VN counts and repaints; clear() blanks it.

    The zoom readout ("150%", shown only above 100%) is painted in the bottom right
    corner. Closing the strip is the ScopesView's [X], not this scope's.

    Counts come from the same MW::updateDevelopScopes sample pass that feeds the
    histogram.
*/
class VectorscopeView : public QWidget
{
    Q_OBJECT
public:
    explicit VectorscopeView(QWidget *parent = nullptr);
    void setData(const ScopeData &d);
    void clear();
    void setMarker(int r, int g, int b);   // pixel under the loupe cursor: a ring at its Cb/Cr
    void clearMarker();
    void setZoom(double z);                 // 1.0 / 1.5 / 2.0 -- scales the plotted trace + marker
    void setSkinLine(bool on);              // show the skin-tone (flesh) reference line
    QSize sizeHint() const override;

signals:
    void zoomChanged(double z);             // right-click menu choice (MW persists it)
    void skinLineChanged(bool on);          // right-click menu toggle (MW persists it)
    /* Right-click: `menu` already holds this scope's own zoom / skin-line items and the
       receiver (ScopesView -> MW) appends the shared scopes-layout section and shows it.
       The menu lives on the emitter's stack, so handling must be synchronous. */
    void menuRequested(QMenu *menu, QPoint globalPos);

protected:
    void paintEvent(QPaintEvent *event) override;
    /* 100/150/200% zoom + skin line, then the shared scopes-layout section. */
    void contextMenuEvent(QContextMenuEvent *event) override;
    /* Consume double-clicks so they do not bubble to the dock (which would un/redock). */
    void mouseDoubleClickEvent(QMouseEvent *event) override;

private:
    void drawZoomLabel(QPainter &p);   // bottom right readout, painted last


    quint32 vec[ScopeData::VN][ScopeData::VN];
    bool hasData = false;
    int marker[3] = { 0, 0, 0 };   // cursor pixel R,G,B
    bool hasMarker = false;
    double zoom = 1.0;             // plot magnification (graticule = the 100% ring)
    bool showSkinLine = false;     // skin-tone (YIQ I-axis) reference line
};

#endif // VECTORSCOPEVIEW_H
