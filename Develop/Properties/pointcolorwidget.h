#ifndef POINTCOLORWIDGET_H
#define POINTCOLORWIDGET_H

#include <QWidget>
#include <QIcon>
#include <QImage>
#include "Develop/pointcolor.h"

/*
    PointColorWidget -- the picture half of the Develop "Point Color" panel, modelled on
    Lightroom's Color Mixer > Point Color. One painted widget, top to bottom:

      o the eyedropper button, then one swatch per sampled point (the SELECTED one is
        outlined), then a delete cross for the selected point. Right-click a swatch for
        Delete / Delete All.
      o the hue x saturation FIELD about the selected sample (x = hue, the sample in the
        middle; y = chroma, saturated at the top) and, beside it, the LUMINANCE strip.
        The small ring is where the sample is, the large ring where the shifts take it;
        dragging the large ring writes Hue / Saturation Shift (field) or Luminance Shift
        (strip).
      o the before | after bar: the sample, and the sample as shifted.

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
    void setPickArmed(bool armed);
    QSize sizeHint() const override;
    static int preferredHeight();

signals:
    void pickToggled();                     // the eyedropper button
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
    QRect fieldRect() const;
    QRect stripRect() const;
    QRect barRect() const;
    int   swatchAt(const QPoint &pos) const;
    bool  hasSel() const { return sel >= 0 && sel < n; }

    /* Field / strip <-> colour. The field spans the sample's hue +/- kFieldHueSpan and
       chroma 0 .. fieldCMax(); the strip is L 0 (bottom) .. 1 (top). */
    float fieldCMax() const;
    QPointF fieldPos(float h, float C) const;
    float stripY(float L) const;
    void  rebuildFieldImage();
    void  dragTo(const QPoint &pos, bool commit);

    int n = 0;
    PointColor::Point pts[PointColor::kMaxPts];
    int sel = -1;
    float Ls = 0.0f, Cs = 0.0f, hs = 0.0f;     // the selected sample, OKLCh
    QIcon dropIdle, dropArmed;
    bool  armed = false;
    enum Drag { None, Field, Strip } drag = None;
    QImage fieldImg, stripImg;                 // cached for the selected sample + size
    QSize  imgFor;
    float  imgL = -1.0f, imgC = -1.0f, imgH = -1.0f;
};

#endif // POINTCOLORWIDGET_H
