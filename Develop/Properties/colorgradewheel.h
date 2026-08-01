#ifndef COLORGRADEWHEEL_H
#define COLORGRADEWHEEL_H

#include <QWidget>
#include <QImage>
#include <QPointF>

/*
    ColorGradeWheel -- the shared hue/saturation wheel for the Develop "Color Mix" panel's
    colour grading. Hue is the angle (0 deg = red at 3 o'clock, increasing anticlockwise),
    saturation the radius (centre = neutral, rim = full). It carries three range dots
    (0 = shadows, 1 = midtones, 2 = highlights); a drag moves every ACTIVE range (set via
    setActiveMask) to the cursor's hue/sat, so checking one range edits it alone and
    checking all three edits them together (a global tint). Active dots draw bright, dim
    otherwise.

    Luminance is NOT on the wheel -- it is a separate slider in the panel. The wheel emits
    gradeChanged live during a drag (drive the preview) and gradeCommitted on release
    (commit / undo), mirroring how a SliderEditor behaves.
*/
class ColorGradeWheel : public QWidget
{
    Q_OBJECT
public:
    explicit ColorGradeWheel(QWidget *parent = nullptr);

    void setRange(int range, float hueDeg, float sat);   // range 0..2 (S/M/H)
    void setActiveMask(int mask);                     // bit0 shadow, bit1 mid, bit2 high
    int  activeRanges() const { return activeMask; }
    float hue(int range) const { return hueDeg[range]; }
    float sat(int range) const { return satUnit[range]; }

signals:
    void gradeChanged();      // live, during a drag
    void gradeCommitted();    // on release

protected:
    void paintEvent(QPaintEvent *) override;
    void resizeEvent(QResizeEvent *) override;
    void mousePressEvent(QMouseEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void mouseReleaseEvent(QMouseEvent *) override;
    void mouseDoubleClickEvent(QMouseEvent *) override;   // reset active range(s)

private:
    void    rebuildWheel();               // render the HSV disc into wheelCache
    void    applyPos(const QPointF &pos); // cursor -> hue/sat for every active range
    QPointF dotPos(int range) const;

    float hueDeg[3]  = {0.0f, 0.0f, 0.0f};
    float satUnit[3] = {0.0f, 0.0f, 0.0f};
    int   activeMask = 0x2;               // midtones by default
    bool  dragging   = false;
    QImage  wheelCache;
    QPointF centre;
    float   radius = 1.0f;
};

#endif // COLORGRADEWHEEL_H
