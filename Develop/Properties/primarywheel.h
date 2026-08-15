#ifndef PRIMARYWHEEL_H
#define PRIMARYWHEEL_H

#include "Develop/Properties/huesatwheel.h"

#include <QPointF>

/*
    PrimaryWheel -- the hue/saturation wheel for the Develop "Calibrate" panel. The HSV
    disc and its geometry come from HueSatWheel; this class carries three dots, one per
    RGB PRIMARY (0 = red, 1 = green, 2 = blue).

    It differs from ColorGradeWheel in what a dot MEANS. A grade dot is an absolute
    hue/sat position, so a drag sets it outright. A primary dot is a DELTA from a fixed
    home angle -- red 0 deg, green 120 deg, blue 240 deg -- so it starts on its own spoke
    at mid radius, and a drag reads off:

      * hue  = the ANGLE swept away from that primary's home, -100..100 mapped to
               +/-Calibrate::kMaxHueDeg. Dragging clockwise/anticlockwise shifts the
               primary; the value is signed and does not wrap past full scale.
      * sat  = the RADIUS relative to the neutral mid point, -100..100 (centre = -100
               = fully desaturated primary, rim = +100 = doubled chroma).

    A drag moves every ACTIVE primary (setActiveMask, bit0 red / bit1 green / bit2 blue)
    by the SAME delta, so checking all three nudges the whole set together. Double-click
    resets the checked primaries to 0/0.

    Emits primaryChanged live during a drag and primaryCommitted on release, matching
    ColorGradeWheel so the panel wires them the same way.
*/
class PrimaryWheel : public HueSatWheel
{
    Q_OBJECT
public:
    explicit PrimaryWheel(QWidget *parent = nullptr);

    void setPrimary(int p, float hue, float sat);   // p 0..2 (R/G/B), both -100..100
    void setActiveMask(int mask);                   // bit0 red, bit1 green, bit2 blue
    float hue(int p) const { return hueVal[p]; }
    float sat(int p) const { return satVal[p]; }

    static float homeAngle(int p) { return 120.0f * p; }   // red 0, green 120, blue 240

signals:
    void primaryChanged();      // live, during a drag
    void primaryCommitted();    // on release

protected:
    void paintEvent(QPaintEvent *) override;
    void mousePressEvent(QMouseEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void mouseReleaseEvent(QMouseEvent *) override;
    void mouseDoubleClickEvent(QMouseEvent *) override;   // reset active primaries

private:
    QPointF dotPos(int p) const;          // primary's (hue,sat) delta -> widget px
    void    applyPos(const QPointF &pos); // cursor -> hue/sat for every active primary

    float hueVal[3] = {0.0f, 0.0f, 0.0f};   // -100..100, delta from the home angle
    float satVal[3] = {0.0f, 0.0f, 0.0f};   // -100..100, delta from mid radius
    int   activeMask = 0x1;                 // red by default
    bool  dragging   = false;
};

#endif // PRIMARYWHEEL_H
