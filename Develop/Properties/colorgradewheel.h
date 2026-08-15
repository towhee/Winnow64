#ifndef COLORGRADEWHEEL_H
#define COLORGRADEWHEEL_H

#include "Develop/Properties/huesatwheel.h"

#include <QPointF>

/*
    ColorGradeWheel -- the hue/saturation wheel for the Develop "Color Grade" panel. The
    HSV disc, its geometry and the hue/sat <-> pixel mapping come from HueSatWheel;
    this class adds four range dots: 0 = shadows, 1 = midtones, 2 = highlights and
    3 = GLOBAL (not tone-selective -- it tints every pixel at full weight). A drag moves
    every ACTIVE range (set via setActiveMask) to the cursor's hue/sat, so checking one
    range edits it alone and checking several edits them together. Active dots draw
    bright, dim otherwise.

    Luminance is NOT on the wheel -- it is a separate slider in the panel. The wheel emits
    gradeChanged live during a drag (drive the preview) and gradeCommitted on release
    (commit / undo), mirroring how a SliderEditor behaves.
*/
class ColorGradeWheel : public HueSatWheel
{
    Q_OBJECT
public:
    explicit ColorGradeWheel(QWidget *parent = nullptr);

    void setRange(int range, float hueDeg, float sat);   // range 0..3 (S/M/H/Global)
    void setActiveMask(int mask);   // bit0 shadow, bit1 mid, bit2 high, bit3 global
    int  activeRanges() const { return activeMask; }
    float hue(int range) const { return hueDeg[range]; }
    float sat(int range) const { return satUnit[range]; }

signals:
    void gradeChanged();      // live, during a drag
    void gradeCommitted();    // on release

protected:
    void paintEvent(QPaintEvent *) override;
    void mousePressEvent(QMouseEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void mouseReleaseEvent(QMouseEvent *) override;
    void mouseDoubleClickEvent(QMouseEvent *) override;   // reset active range(s)

private:
    void applyPos(const QPointF &pos);    // cursor -> hue/sat for every active range

    float hueDeg[4]  = {0.0f, 0.0f, 0.0f, 0.0f};
    float satUnit[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    int   activeMask = 0x2;               // midtones by default
    bool  dragging   = false;
};

#endif // COLORGRADEWHEEL_H
