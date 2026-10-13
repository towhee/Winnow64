#ifndef POINTCOLORWIDGET_H
#define POINTCOLORWIDGET_H

#include <QWidget>
#include <QIcon>
#include <QImage>
#include "Develop/pointcolor.h"

/*
    PointColorWidget -- the picture half of the Develop "Point Color" panel, modelled on
    Lightroom's Color Mixer > Point Color. One painted widget, top to bottom:

      o the eyedropper button, then one swatch per point (the SELECTED one is outlined;
        a point with several samples shows them as stripes), then, at the right, the
        ADD-SAMPLE dropper (adds a sample to the selected point, broadening the colours
        it reaches -- see MULTIPLE SAMPLES in pointcolor.h) and a delete cross for the
        selected point. Right-click a swatch for Delete / Delete All and the sample
        removals.
      o the hue x saturation FIELD about the selected point (x = hue, the point's centre
        in the middle; y = chroma, saturated at the top) and, beside it, the LUMINANCE
        strip. The small rings are the samples (with a dashed box round the region they
        span when there are several), the large ring where the shifts take the centre;
        dragging it writes Hue / Saturation Shift (field) or Luminance Shift (strip).
      o the before | after bar: the samples, and the samples as shifted.

    The sliders below it in the panel (Hue / Sat / Lum Shift, Variance, the ranges) are
    ordinary addSlider rows; this widget owns no params. DevelopProperties pushes the
    points in (setPoints) and acts on the signals. Coordinates and colours all go
    through Develop/pointcolor.h, so the rings sit exactly where the render puts them.
*/
class PointColorWidget : public QWidget
{
    Q_OBJECT
public:
    explicit PointColorWidget(QWidget *parent = nullptr);

    void setPoints(int n, const PointColor::Point *pts, int selected);
    void setDropperIcons(const QIcon &idle, const QIcon &armed);
    /* addMode: armed by the add-sample dropper rather than the new-point one. */
    void setPickArmed(bool armed, bool addMode = false);
    QSize sizeHint() const override;
    static int preferredHeight();

signals:
    void pickToggled();                     // the eyedropper button
    void addSampleToggled();                // the add-sample dropper
    void lastSampleRemoved(int index);      // drop the point's most recent added sample
    void addedSamplesCleared(int index);    // back to the point's first sample only
    void pointSelected(int index);
    void pointDeleted(int index);
    void allPointsDeleted();
    /* A drag of the large ring: the selected point's new shifts. commit on release. */
    void shiftsDragged(float hueShift, float satShift, float lumShift, bool commit);

protected:
    void paintEvent(QPaintEvent *) override;
    void mousePressEvent(QMouseEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void mouseReleaseEvent(QMouseEvent *) override;
    void contextMenuEvent(QContextMenuEvent *) override;
    bool event(QEvent *) override;           // region tooltips

private:
    QRect dropperRect() const;
    QRect swatchRect(int i) const;
    QRect deleteRect() const;
    QRect addSampleRect() const;
    bool  canAddSample() const;
    QRect fieldRect() const;
    QRect stripRect() const;
    QRect barRect() const;
    int   swatchAt(const QPoint &pos) const;
    bool  hasSel() const { return sel >= 0 && sel < n; }

    /* Field / strip <-> colour. The field spans the point's centre hue +/- hueSpan
       (wide enough for every sample) and chroma 0 .. fieldCMax(); the strip is L 0
       (bottom) .. 1 (top). */
    float fieldCMax() const;
    QPointF fieldPos(float h, float C) const;
    float stripY(float L) const;
    void  rebuildFieldImage();
    void  dragTo(const QPoint &pos, bool commit);

    int n = 0;
    PointColor::Point pts[PointColor::kMaxPts];
    int sel = -1;
    float Ls = 0.0f, Cs = 0.0f, hs = 0.0f;     // the selected point's centre, OKLCh
    PointColor::Envelope env;                  //   and the region its samples span
    float hueSpan = 1.3089969f;                // field half-width, radians (75 deg)
    QIcon dropIdle, dropArmed;
    bool  armed = false;
    bool  armedAdd = false;
    enum Drag { None, Field, Strip } drag = None;
    QImage fieldImg, stripImg;                 // cached for the selected sample + size
    QSize  imgFor;
    float  imgL = -1.0f, imgC = -1.0f, imgH = -1.0f, imgSpan = -1.0f;
    float  imgCMax = -1.0f;
};

#endif // POINTCOLORWIDGET_H
