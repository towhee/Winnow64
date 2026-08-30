#ifndef DETAILPREVIEW_H
#define DETAILPREVIEW_H

#include <QWidget>
#include <QImage>
#include <QPoint>

/*
    The Detail panel's 1:1 preview: a square window showing one patch of the image at ONE
    image pixel per logical pixel, embedded as a spanned row at the head of the Detail
    section (see DevelopProperties::addDetail). Lightroom's Detail panel preview, in
    Winnow's dark panel styling.

    WHY IT EXISTS. Sharpening is the only Develop op measured in ABSOLUTE pixels
    (Develop/sharpen.h), so it is the only one the interactive proxy cannot show: at proxy
    scale the effective sigma collapses to the kMinSigma floor and moving the slider from
    0 to maximum changes acutance by ~5% instead of the ~70% it changes at full
    resolution. Fitting the loupe to the window hides it for the same reason -- the view
    downsamples away the very acutance the slider added. Before this widget the only way
    to judge sharpening or noise reduction was to zoom the loupe to 100% and wait for the
    settle render.

    The pixels come from MW::renderDetailRoi, which renders a small full-resolution ROI
    through the same develop stack on every slider tick -- so unlike the loupe, this
    preview IS live during a drag. This widget only displays what it is handed; it owns no
    render and no params.

    1:1 MEANS THE LOUPE'S 1:1. One image pixel is painted as one LOGICAL pixel, which is
    what the loupe calls 100% -- so the preview and a zoomed loupe agree. On a retina
    display that is a 2x2 block of device pixels, exactly as the loupe draws it.

    INTERACTION. Dragging inside the preview slides the sample point (emitting pointNudged
    in image pixels, which the panel converts to a normalized point); the pick-a-location
    tool itself lives on the loupe -- see ImageView::beginDetailPick.
*/
class DetailPreview : public QWidget
{
    Q_OBJECT
public:
    explicit DetailPreview(QWidget *parent = nullptr);

    /* The rendered ROI, already oriented and already at 1:1. A null image clears back to
       the placeholder. */
    void setImage(const QImage &img);

    /* Placeholder text shown when there is no image yet -- "Pick a location" and the
       like. The widget draws it centred and dimmed. */
    void setMessage(const QString &text);

    /* The side, in image pixels, of the ROI this widget can show at 1:1. The render asks
       for exactly this so nothing is scaled. */
    int roiSize() const;

signals:
    /* The user dragged inside the preview: move the sample point by this many IMAGE
       pixels (oriented frame). Emitted live during the drag. */
    void pointNudged(int dx, int dy);

protected:
    void paintEvent(QPaintEvent *) override;
    void mousePressEvent(QMouseEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void mouseReleaseEvent(QMouseEvent *) override;
    /* An unhandled double click inside the Develop dock floats / redocks it, so it is
       swallowed here for the same reason CurveEditor swallows it. */
    void mouseDoubleClickEvent(QMouseEvent *) override;

private:
    QImage roi;
    QString message;
    QPoint dragFrom;
    bool dragging = false;
};

#endif // DETAILPREVIEW_H
