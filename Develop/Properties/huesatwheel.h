#ifndef HUESATWHEEL_H
#define HUESATWHEEL_H

#include <QWidget>
#include <QImage>
#include <QPointF>

/*
    HueSatWheel -- the shared HSV disc behind every Develop hue/saturation wheel:
    ColorGradeWheel (tonal-range grading), ColorRangeWheel (Color Range mask selection)
    and PrimaryWheel (Calibrate primaries). It owns everything the three have in common
    and nothing any one of them is about:

      * the cached HSV disc  hue = ANGLE (0 deg = red at 3 o'clock, growing
                             ANTICLOCKWISE), saturation = RADIUS / disc radius, value 1,
                             with a one-pixel rim alpha ramp. Rebuilt on resize only.
      * centre / radius      disc geometry, from the widget size less discMargin.
      * posFor / hueSatAt    the two directions of hue/sat <-> widget pixels. Both flip
                             y, since screen y grows downward and hue angle does not.
      * paintDisc            draws the cache + the rim ring.

    discMargin is the rim-to-edge gap and is the one thing subclasses size differently:
    a bare wheel wants a few px, one that labels its handles OUTSIDE the rim wants room
    for the labels. Set it in the subclass constructor, before the first resize.

    Subclasses own their dots/handles (paintEvent), their interaction (mouse events) and
    their signals -- the base deliberately declares none, because "changed" means a
    different thing to a grade, a selection band and a primary.
*/
class HueSatWheel : public QWidget
{
    Q_OBJECT
public:
    explicit HueSatWheel(QWidget *parent = nullptr);

protected:
    void resizeEvent(QResizeEvent *) override;

    void    rebuildWheel();                 // render the HSV disc into wheelCache
    void    paintDisc(QPainter &p);         // draw the cache + rim ring (call first)
    QPointF posFor(float hueDeg, float satUnit) const;        // hue/sat -> widget px
    void    hueSatAt(const QPointF &pos, float &hueDeg, float &sat) const;   // inverse

    float   discMargin = 12.0f;             // px between the disc rim and the widget edge
    QImage  wheelCache;
    QPointF centre;
    float   radius = 1.0f;

    static constexpr float kPi = 3.14159265358979323846f;
};

#endif // HUESATWHEEL_H
