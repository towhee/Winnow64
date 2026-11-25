#include "Main/thumbsizecontrol.h"
#include "Main/global.h"
#include "Views/iconview.h"
#include <QEvent>
#include <QHBoxLayout>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QSlider>
#include <QStyle>
#include <QTimer>

namespace {

/*
    A horizontal slider drawn as a wedge: the track is thin and dark at the left (small
    thumbnails) and fat and light at the right (large), so it reads as a size control
    without any icons. The handle is a round knob.

    It does its own mouse handling, because QSlider hit-tests the style's handle, which
    is not where this one is drawn. A press jumps to the step under the cursor and a
    drag follows it. setSliderDown emits sliderPressed / sliderReleased as usual, so a
    drag keeps one anchor row (IconView::beginSizeDrag).
*/
class WedgeSlider : public QSlider
{
public:
    WedgeSlider() : QSlider(Qt::Horizontal)
    {
        setFixedSize(100, 16);
        setFocusPolicy(Qt::NoFocus);    // arrow keys belong to the thumbnail views
        setAttribute(Qt::WA_Hover);
    }

protected:
    static constexpr int knobR = 5;         // knob radius
    static constexpr qreal thinH = 1.5;     // track height at the left
    static constexpr qreal fatH = 7.0;      // track height at the right

    int knobX() const
    {
        const int span = width() - 2 * knobR;
        return knobR + QStyle::sliderPositionFromValue(minimum(), maximum(),
                                                       sliderPosition(), span);
    }

    int valueAt(int x) const
    {
        const int span = width() - 2 * knobR;
        return QStyle::sliderValueFromPosition(minimum(), maximum(),
                                               qBound(0, x - knobR, span), span);
    }

    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        if (!isEnabled()) p.setOpacity(0.4);

        const qreal cy = height() / 2.0;
        const qreal x0 = knobR;
        const qreal x1 = width() - knobR;

        // Darker to lighter, left to right
        const int dark = qBound(0, G::backgroundShade + 35, 255);
        QLinearGradient grad(x0, 0, x1, 0);
        grad.setColorAt(0, QColor(dark, dark, dark));
        grad.setColorAt(1, G::textColor);

        // Thinner to fatter: a wedge with rounded ends
        QPainterPath track;
        track.moveTo(x0, cy - thinH / 2);
        track.lineTo(x1, cy - fatH / 2);
        track.arcTo(QRectF(x1 - fatH / 2, cy - fatH / 2, fatH, fatH), 90, -180);
        track.lineTo(x0, cy + thinH / 2);
        track.arcTo(QRectF(x0 - thinH / 2, cy - thinH / 2, thinH, thinH), 270, -180);
        track.closeSubpath();
        p.setPen(Qt::NoPen);
        p.setBrush(grad);
        p.drawPath(track);

        // Knob, brighter while hovered or dragged
        const bool hot = isSliderDown() || underMouse();
        QColor knob = G::textColor;
        if (!hot) knob = knob.darker(115);
        p.setBrush(knob);
        p.setPen(QPen(QColor(0, 0, 0, 120), 1));
        p.drawEllipse(QPointF(knobX(), cy), knobR, knobR);
    }

    void mousePressEvent(QMouseEvent *e) override
    {
        if (e->button() != Qt::LeftButton || !isEnabled()) { e->ignore(); return; }
        setSliderDown(true);                // emits sliderPressed
        setSliderPosition(valueAt(int(e->position().x())));
        e->accept();
    }

    void mouseMoveEvent(QMouseEvent *e) override
    {
        if (!isSliderDown()) { e->ignore(); return; }
        setSliderPosition(valueAt(int(e->position().x())));
        e->accept();
    }

    void mouseReleaseEvent(QMouseEvent *e) override
    {
        if (!isSliderDown()) { e->ignore(); return; }
        setSliderDown(false);               // emits sliderReleased
        update();
        e->accept();
    }
};

} // namespace

ThumbSizeControl::ThumbSizeControl(QWidget *parent)
    : QWidget(parent)
{
    setObjectName("ThumbSizeControl");

    slider = new WedgeSlider;
    slider->setSingleStep(1);
    slider->setPageStep(1);
    slider->setTracking(true);
    connect(slider, &QSlider::valueChanged, this, &ThumbSizeControl::onSliderMoved);
    connect(slider, &QSlider::sliderPressed, this, [this]() {
        if (view) view->beginSizeDrag();
    });
    connect(slider, &QSlider::sliderReleased, this, [this]() {
        if (view) view->endSizeDrag();
    });

    QHBoxLayout *layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(slider);

    setView(nullptr, "no thumbnails are showing");
}

void ThumbSizeControl::pinToRightEdge(QWidget *statusBar, int offset)
{
/*
    PINNED, NOT LAID OUT. As a QStatusBar permanent widget the sizer wandered across the
    bar while Winnow opened: the bar re-lays out its widgets whenever one changes width
    (the progress bar is resized to the space left, labels fill in), and while their
    minimum widths overflow the bar the permanent group is pushed around with them.

    So it is a plain child of the bar, placed by hand offset px in from the right edge
    and centered vertically, on every resize of the bar. The bar's right contents margin
    reserves its width, so the bar's own layout never puts anything under it.
*/
    bar = statusBar;
    rightOffset = offset;
    setParent(bar);
    QMargins m = bar->contentsMargins();
    m.setRight(m.right() + sizeHint().width() + 2 * offset);
    bar->setContentsMargins(m);
    bar->installEventFilter(this);
    show();
    place();
}

void ThumbSizeControl::place()
{
    if (!bar) return;
    const QSize s = sizeHint();
    setGeometry(bar->width() - rightOffset - s.width(),
                (bar->height() - s.height()) / 2,
                s.width(), s.height());
    raise();
}

void ThumbSizeControl::setViews(IconView *grid, IconView *thumbs)
{
    gridView = grid;
    thumbView = thumbs;
    if (gridView) gridView->installEventFilter(this);
    if (thumbView) thumbView->installEventFilter(this);
    chooseView();
}

bool ThumbSizeControl::eventFilter(QObject *obj, QEvent *event)
{
    const QEvent::Type t = event->type();
    if (obj == bar && (t == QEvent::Resize || t == QEvent::Show)) {
        place();
        return QWidget::eventFilter(obj, event);
    }

    /* A view switch (grid, loupe, table, compare) or the thumb dock opening or closing
       shows or hides a view. Choose once the switch has settled: during it both views
       can be visible, or neither. */
    if ((event->type() == QEvent::Show || event->type() == QEvent::Hide)
        && (obj == gridView || obj == thumbView) && !isChoosePending) {
        isChoosePending = true;
        QTimer::singleShot(0, this, [this]() {
            isChoosePending = false;
            chooseView();
        });
    }
    return QWidget::eventFilter(obj, event);
}

void ThumbSizeControl::chooseView()
{
    if (gridView && gridView->isVisible()) setView(gridView);
    else if (thumbView && thumbView->isVisible()) setView(thumbView);
    else setView(nullptr, "no thumbnails are showing");
}

void ThumbSizeControl::setView(IconView *v, const QString &reason)
{
    if (viewConn) disconnect(viewConn);
    if (view && view != v) view->endSizeDrag();
    view = v;
    disabledReason = reason;
    if (view) {
        viewConn = connect(view, &IconView::thumbSizeChanged,
                           this, &ThumbSizeControl::sync);
    }
    sync();
}

void ThumbSizeControl::sync()
{
    /* Re-read the range and the position from the view. The range moves with the view's
       width (a wider grid has more thumbs-per-row steps). Signals stay blocked so this
       never feeds back into setSizeStep. */
    const bool on = view != nullptr;
    slider->setEnabled(on);

    if (!on) {
        slider->setToolTip("Thumbnail sizer: unavailable, " + disabledReason + ".");
        return;
    }
    slider->setToolTip("Thumbnail sizer.  Shortcuts [ and ].");

    isSyncing = true;
    const QSignalBlocker block(slider);
    const int count = qMax(1, view->sizeStepCount());
    slider->setRange(0, count - 1);
    slider->setValue(view->sizeStep());
    slider->update();
    isSyncing = false;
}

void ThumbSizeControl::onSliderMoved(int value)
{
    if (isSyncing || !view) return;
    view->setSizeStep(value, "ThumbSizeControl");
    /* The view may not land exactly on the value, so show where it really is.
       setSizeStep emits thumbSizeChanged, which already synced unless the size did not
       change. */
    sync();
}
