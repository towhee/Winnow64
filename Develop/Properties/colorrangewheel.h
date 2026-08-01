#ifndef COLORRANGEWHEEL_H
#define COLORRANGEWHEEL_H

#include <QWidget>
#include <QImage>
#include <QPointF>
#include <QVector>

/*
    ColorRangeWheel -- the hue/saturation wheel for the Develop Color Range MASK. It
    mirrors ColorGradeWheel's HSV disc (hue = angle, 0 deg = red at 3 o'clock growing
    anticlockwise; saturation = radius) but visualises a SELECTION rather than a grade:

      * a small dot for every sampled colour (the pipette picks), at its hue/sat;
      * a band SECTOR around each sample -- hue [sample - hueLo, sample + hueHi], sat
        [sample - satLo, sample + satHi] -- the region RangeMask::colorCoverage keeps;
      * four GRAB HANDLES on the anchor (first) sample: the two hue edges and the
        inner/outer saturation arcs. Dragging a handle changes the corresponding GLOBAL
        bound (all samples share one set of bounds), kept in sync with the dock sliders.

    hueLo/hueHi are degrees; satLo/satHi are saturation fractions (0..1). The wheel emits
    boundsChanged live during a drag (drive the preview) and boundsCommitted on release
    (commit / debounced persist), like a SliderEditor / ColorGradeWheel.
*/
class ColorRangeWheel : public QWidget
{
    Q_OBJECT
public:
    explicit ColorRangeWheel(QWidget *parent = nullptr);

    /* samples: each QPointF is (hueDeg 0..360, sat 0..1). */
    void setSamples(const QVector<QPointF> &s);
    void setBounds(float hueLoDeg, float hueHiDeg, float satLo, float satHi);

    float hueLo() const { return m_hueLo; }
    float hueHi() const { return m_hueHi; }
    float satLo() const { return m_satLo; }
    float satHi() const { return m_satHi; }

signals:
    void boundsChanged();     // live, during a handle drag
    void boundsCommitted();   // on release

protected:
    void paintEvent(QPaintEvent *) override;
    void resizeEvent(QResizeEvent *) override;
    void mousePressEvent(QMouseEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void mouseReleaseEvent(QMouseEvent *) override;

private:
    enum Handle { None = -1, HueLoH = 0, HueHiH, SatInH, SatOutH };

    void    rebuildWheel();                       // render the HSV disc into wheelCache
    QPointF posFor(float hueDeg, float satUnit) const;   // hue/sat -> widget px
    bool    anchor(float &hueDeg, float &sat) const;     // first sample; false if none
    void    handlePositions(QPointF out[4]) const;       // 4 grab handles on the anchor
    int     hitHandle(const QPointF &pos) const;
    void    applyDrag(const QPointF &pos);

    QVector<QPointF> m_samples;                    // (hueDeg, sat)
    float   m_hueLo = 20.0f, m_hueHi = 20.0f;      // degrees
    float   m_satLo = 0.25f, m_satHi = 0.25f;      // saturation fractions 0..1
    int     m_drag  = None;

    QImage  wheelCache;
    QPointF centre;
    float   radius = 1.0f;
};

#endif // COLORRANGEWHEEL_H
