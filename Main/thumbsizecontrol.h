#ifndef THUMBSIZECONTROL_H
#define THUMBSIZECONTROL_H

#include <QWidget>
#include <QPointer>

class QSlider;
class IconView;

/*
    The status bar's thumbnail sizer: a 100 px slider pinned to the right edge, drawn as
    a wedge that grows from thin and dark (small thumbnails) to fat and light (large).

    It walks the same discrete steps as the [ and ] keys (IconView::setSizeStep, see
    Views/thumbsizesteps.h): thumbs per row in a wrapping view, the 1.1x ladder in the
    filmstrip. One slider tick is one key press.

    It drives one view at a time: the grid when the grid is visible, else the thumbnails
    (setViews, which watches both being shown and hidden). The view's thumbSizeChanged
    keeps it in step with any other size change (keys, a window resize, a thumb dock
    drag, a workspace restore). With no view it stays visible but disabled, and the
    tooltip says why.
*/
class ThumbSizeControl : public QWidget
{
    Q_OBJECT
public:
    explicit ThumbSizeControl(QWidget *parent = nullptr);
    /* The two views it can drive. It watches them being shown and hidden and drives
       the grid when that is visible, else the thumbnails (the view [ and ] resize). */
    void setViews(IconView *gridView, IconView *thumbView);
    void sync();
    /* Pins the sizer to the right edge of bar, offset px in from it, outside the bar's
       layout. See the comment in the .cpp. */
    void pinToRightEdge(QWidget *bar, int offset);

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    void chooseView();
    void place();
    void setView(IconView *view, const QString &disabledReason = QString());
    void onSliderMoved(int value);

    QPointer<IconView> gridView;
    QPointer<IconView> thumbView;
    QPointer<IconView> view;
    bool isChoosePending = false;
    QString disabledReason;
    QSlider *slider;
    QMetaObject::Connection viewConn;
    bool isSyncing = false;
    QPointer<QWidget> bar;
    int rightOffset = 0;
};

#endif // THUMBSIZECONTROL_H
