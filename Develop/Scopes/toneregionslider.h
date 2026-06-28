#ifndef TONEREGIONSLIDER_H
#define TONEREGIONSLIDER_H

#include <QWidget>

/*
    A Lightroom-style tone-region control drawn directly under the histogram: a black -> white
    gradient track with three draggable handles that split the tonal range into blacks | shadows
    | highlights | whites. The handles set WHERE the shadows/highlights tone controls act and how
    far each reaches (the middle handle is the crossover); blacks and whites stay pinned at the
    ends. Positions are perceptual 0..1 and align with the histogram's x-axis above.

    valueChanged is emitted live during a drag (so the develop preview re-renders like a slider);
    setPositions() pushes values in WITHOUT echoing the signal (used when an image is selected).
*/
class ToneRegionSlider : public QWidget
{
    Q_OBJECT
public:
    explicit ToneRegionSlider(QWidget *parent = nullptr);

    void setPositions(double shadow, double crossover, double highlight);   // no signal
    double shadow() const    { return pos[0]; }
    double crossover() const { return pos[1]; }
    double highlight() const { return pos[2]; }
    QSize sizeHint() const override;

signals:
    void valueChanged(double shadow, double crossover, double highlight);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;

private:
    double pos[3] = { 0.25, 0.50, 0.75 };
    int dragHandle = -1;                 // index being dragged, -1 = none
    static constexpr double kMinGap = 0.02;

    int xOf(double p) const;             // position 0..1 -> pixel x (aligned with histogram)
    double pOf(int x) const;             // pixel x -> position 0..1
    int handleAt(int x) const;           // nearest handle within the hit threshold, else -1
    void clampOrder();                   // keep pos[0] < pos[1] < pos[2] with kMinGap, in [0,1]
};

#endif // TONEREGIONSLIDER_H
