#ifndef SCOPESVIEW_H
#define SCOPESVIEW_H

#include <QWidget>
#include "Develop/Scopes/scopedata.h"
#include "Develop/Scopes/histogramview.h"
#include "Develop/Scopes/vectorscopeview.h"
#include "Develop/Scopes/toneregionslider.h"

/*
    The Develop dock's scopes panel: a fixed-height strip placed ABOVE the property
    tree (see MW::createDevelopDock) holding the histogram on the left and the
    vectorscope on the right. The left/right split is the QHBoxLayout stretch ratio
    (histogram wider, since its x-axis is the full tonal range).

    setData() fans one ScopeData out to both child scopes; clear() blanks them when no
    image is shown. MW owns this widget and feeds it from updateDevelopScopes().
*/
class ScopesView : public QWidget
{
    Q_OBJECT
public:
    explicit ScopesView(QWidget *parent = nullptr);
    void setData(const ScopeData &d);
    void clear();
    void setMarker(int r, int g, int b);   // loupe cursor readout on both scopes
    void clearMarker();
    void setVectorscopeZoom(double z);     // apply a persisted/initial zoom
    void setVectorscopeSkinLine(bool on);  // apply a persisted/initial skin-line state
    ToneRegionSlider *toneRegionSlider() const { return tone; }

signals:
    void vectorscopeZoomChanged(double z);     // user picked a zoom from the vectorscope menu
    void vectorscopeSkinLineChanged(bool on);  // user toggled the skin line

protected:
    /* Consume double-clicks landing on the strip's margins so they do not bubble to the dock
       (which would un/redock); the child scopes consume their own. */
    void mouseDoubleClickEvent(QMouseEvent *event) override;

private:
    HistogramView *histogram;
    VectorscopeView *vectorscope;
    ToneRegionSlider *tone;
};

#endif // SCOPESVIEW_H
