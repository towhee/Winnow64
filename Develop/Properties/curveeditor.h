#ifndef CURVEEDITOR_H
#define CURVEEDITOR_H

#include <QWidget>
#include "Develop/editparams.h"
#include "Develop/tonecurve.h"
#include "Develop/Scopes/scopedata.h"

/*
    The Curves panel's plot: a Lightroom Tone Curve editor embedded as a spanned row
    in the Develop Edits tree (see DevelopProperties::addCurves). Two modes over the
    SAME square plot, with the image's histogram behind:

      Point       the free-form curve. A channel combo picks RGB (the composite,
                  applied to every channel) or Red / Green / Blue (layered on the
                  composite). Drag a
                  point; click empty space to add one; double-click a point to delete it;
                  double-click the background to reset that channel to the diagonal. The
                  endpoints are pinned in x but free in y -- that is the black-point /
                  white-point move.

      Parametric  the curve the BASIC panel's tone sliders already produce, drawn from
                  Develop::ParametricCurve so it cannot drift from what actually renders.
                  Dragging inside one of the four tonal bands writes that band's EXISTING
                  Basic slider (blacks | shadows | highlights | whites, in x order). There
                  are no separate parametric params: the Curves panel is a second view of
                  Basic's tone controls, not a second tone system. The three band
                  splits are the tone-split params, edited by the ToneRegionSlider the
                  panel places
                  directly beneath this widget (and mirrored under the histogram scope).

    SIGNALS follow the wheels' contract (colorgradewheel.h): xxxChanged fires live on
    every move so the preview re-renders, xxxCommitted on release so the caller knows
    the drag
    ended. setParams() pushes values IN without echoing either, so a programmatic sync can
    never loop.

    NOTE mouseDoubleClickEvent is overridden unconditionally: an unhandled double click
    inside the Develop dock floats / redocks it.
*/
class CurveEditor : public QWidget
{
    Q_OBJECT
public:
    enum Mode { Parametric = 0, Point = 1 };

    /* Border between the widget edge and the plot's 0..1 x-axis. Public so the panel
       can inset the ToneRegionSlider it places underneath by the same amount and the
       split handles line up with the curve above them. */
    static constexpr int kPlotMargin = 6;

    explicit CurveEditor(QWidget *parent = nullptr);

    void setMode(Mode m);
    Mode mode() const { return curMode; }

    /* Which curve Point mode edits: 0 = RGB composite, 1/2/3 = R/G/B. */
    void setChannel(int c);
    int  channel() const { return curChannel; }

    /* Programmatic push (image change, history, preset). No signal. */
    void setParams(const EditParams &p);
    const EditParams &params() const { return prm; }

    /* The image's histogram, painted behind the plot. Fed from the same ScopeData
       MW::updateDevelopScopes hands to ScopesView, so it costs no extra sampling. */
    void setScopeData(const ScopeData &d);
    void clearScopeData();

    QSize sizeHint() const override;

signals:
    /* Point mode. The caller reads the new control points from params(). */
    void curveChanged();
    void curveCommitted();
    /* Parametric mode. band: 0 = blacks, 1 = shadows, 2 = highlights, 3 = whites;
       value is the new ABSOLUTE slider value (-100..100) for that band's Basic param. */
    void parametricChanged(int band, double value);
    void parametricCommitted();

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    EditParams prm;
    Mode curMode    = Parametric;
    int  curChannel = 0;

    ScopeData hist;
    bool hasHist = false;

    /* Point mode drag state. */
    int dragPt   = -1;
    int hoverPt  = -1;
    /* Parametric mode drag state: which band, where the press was, and the band's
       value at press -- so the drag is an offset from the press, not an accumulation
       (which
       would drift if a move event were coalesced away). */
    int    dragBand   = -1;
    int    hoverBand  = -1;
    int    pressY     = 0;
    double pressValue = 0.0;

    QRect plotRect() const;                 // drawing area inside the margins
    QPointF toPlot(double x, double y) const;      // curve 0..1 -> widget pixels
    QPointF fromPlot(const QPointF &pt) const;     // pixels -> curve 0..1 (clamped)
    int  pointAt(const QPoint &pos) const;         // control point under cursor, else -1
    int  bandAt(const QPoint &pos) const;          // tonal band under the cursor, else -1
    void bandSplits(double s[3]) const;            // the three split positions, ordered
    float *bandParam(int band);                    // the EditParams field a band writes
    static QString bandName(int band);

    /* Sample the curve currently being edited (Point mode) or the parametric shape
       (Parametric mode) into a polyline across the plot. */
    QPolygonF curvePolyline() const;
    void insertPointAt(double x);                  // add a point on the live curve
    void resetChannel();                           // back to the diagonal

    int  pointCount() const  { return prm.curveN[curChannel]; }
    float *px()              { return prm.curveX[curChannel]; }
    float *py()              { return prm.curveY[curChannel]; }
    const float *px() const  { return prm.curveX[curChannel]; }
    const float *py() const  { return prm.curveY[curChannel]; }
};

#endif // CURVEEDITOR_H
