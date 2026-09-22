#ifndef SHOWHIDEBAR_H
#define SHOWHIDEBAR_H

#include <QWidget>

/*
    ShowHideBar: a thin strip on one edge of the window carrying a single solid triangle.
    One click clears that whole side of panels away from the photo; one click brings back
    exactly what was there (Lightroom's panel bars).

        +-+------+------------+------+-+
        |<|Source|            |Devel.|>|
        | |      |   photo    |      | |
        +-+------+------------+------+-+
        |         filmstrip           |
        +-----------------------------+
        |          |v|                |
        +-----------------------------+
        | Developed  Zoom 19%  Pos 3/8|     <- the status bar, below everything
        +-----------------------------+

    THE TRIANGLE POINTS WHERE THE PANELS ARE ABOUT TO GO, so it reads as an instruction
    rather than as a label for which edge this is: the left bar shows a left-pointing
    triangle while its panels are up (click and they collapse leftwards) and a
    right-pointing one once they are down. Each of the three bars therefore has its own
    pair of glyphs, which is what makes them tellable apart at a glance.

    PAINTED, not an icon. Six orientations from one widget, crisp at any DPI, and the
    flip is a repaint rather than a resource swap -- the arrow PNGs in winnow.qrc are
    two-direction chevrons, not solid triangles. ToneRegionSlider does the same for its
    handles.

    This class knows nothing about docks. It paints, it tracks hover, and it emits
    clicked(); MW::toggleDockArea decides what that means. The WHOLE BAR is the click
    target -- a 14px strip is already a small thing to hit without asking the user to
    find the triangle inside it.
*/
class ShowHideBar : public QWidget
{
    Q_OBJECT
public:
    /* Which window edge this bar sits on. Fixes both the bar's orientation and which way
       its triangle points in each state. */
    enum Edge { Left, Right, Bottom };

    explicit ShowHideBar(Edge edge, QWidget *parent = nullptr);

    /* True while the panels on this edge are showing -- ie the click will COLLAPSE them.
       Drives the triangle's direction and nothing else. */
    void setExpanded(bool expanded);
    bool isExpanded() const { return expanded; }

    /* The strip's thickness across its short axis. Public so the dock hosting the bar can
       pin itself to the same value. */
    static int thickness();

signals:
    void clicked();

protected:
    void paintEvent(QPaintEvent *) override;
    void enterEvent(QEnterEvent *) override;
    void leaveEvent(QEvent *) override;
    void mousePressEvent(QMouseEvent *) override;

private:
    /* The triangle for the current edge + expanded state, centred in `r`. */
    QPolygonF triangle(const QRectF &r) const;

    Edge edge;
    bool expanded = true;
    bool hovered  = false;
};

#endif // SHOWHIDEBAR_H
