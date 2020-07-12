#ifndef EMBELVIEW_H
#define EMBELVIEW_H

#include <QtWidgets>
#include <QHash>
#include "Datamodel/datamodel.h"
#include "Cache/imagecache.h"
#include "Views/iconview.h"
#include "Image/pixmap.h"

#include "Dialogs/patterndlg.h"

//#ifdef Q_OS_WIN
//#include "Utilities/icc.h"
//#endif


class EmbelView : public QGraphicsView
{
    Q_OBJECT

public:
    EmbelView(QWidget *parent,
              QWidget *centralWidget,
              Metadata *metadata,
              DataModel *dm,
              ImageCache *imageCacheThread,
              IconView *thumbView);

    int cwMargin = 20;

    QGraphicsScene *scene;
    QGraphicsPixmapItem *pmItem;
    QGraphicsTextItem *infoItem;
    QMatrix matrix;
    Pixmap *pixmap;

    qreal imAspect = 1;
    qreal zoom;
    qreal zoomFit;
    bool isFit;
    qreal refZoom;                      // adjusted to real screen pixels
    qreal toggleZoom;
    bool isRubberBand;

    QByteArray tileBa;
    QString tileName;

    bool loadImage(QString imageFileName);
    qreal getFitScaleFactor(QRectF container, QRectF content);
    void clear();
    void setCursorHiding(bool hide);
    bool isBusy;
    void setBackgroundColor(QColor bg);

    void rotateByExifRotation(QImage &image, QString &imageFullPath);
    void rotate(int degrees);
    void sceneGeometry(QPoint &sceneOrigin, QRectF &sceneR, QRect &centralWidgetRect);
    bool isFirstImageNewFolder;              // new folder, first image, set zoom = fit
    bool limitFit100Pct = true;

    QString diagnostics();

public slots:
    void monitorCursorState();
    void copyImage();
    void thumbClick(float xPct, float yPct);
    void updateToggleZoom(qreal toggleZoomValue);
    void zoomIn();
    void zoomOut();
    void zoomToFit();
    void zoomTo(qreal zoomTo);
    void zoomToggle();
    void resetFitZoom();
    void setFitZoom();
    void hideCursor();
    void refresh();
    void activateRubberBand();
    void quitRubberBand();

signals:
    void updateStatus(bool, QString, QString);
    void zoomChange(qreal zoomValue);
    void handleDrop(const QMimeData *mimeData);
    void updateEmbel();
    void newTile();

private slots:

protected:
    void resizeEvent(QResizeEvent *event);
    void scrollContentsBy(int dx, int dy);
    void wheelEvent(QWheelEvent *event);
    void mouseDoubleClickEvent(QMouseEvent *event);
    void mousePressEvent(QMouseEvent *event);
    void mouseMoveEvent(QMouseEvent *event);
    void mouseReleaseEvent(QMouseEvent *event);
    void enterEvent(QEvent *event);
    void paintEvent(QPaintEvent *event);
    void dropEvent(QDropEvent *event);
    void dragEnterEvent(QDragEnterEvent *event);


private:
    void noJpgAvailable();
    void scale();
    qreal getZoom();

    QPointF getScrollPct();
    void getScrollBarStatus();
    void setScrollBars(QPointF scrollPct);

    QWidget *mainWindow;
    Metadata *metadata;
    DataModel *dm;
    ImageCache *imageCacheThread;
    IconView *thumbView;

    bool sceneBiggerThanView();
    bool resizeIsSmaller();
    void transform();


    QTimer *mouseMovementTimer;
    QTimer *loadFullSizeTimer;
    QElapsedTimer t;
    QElapsedTimer t1;

    struct pt
    {
        int x;
        int y;
    };

    typedef pt pt;
    pt mouse;

    struct scrollStatus {
        int hVal;
        int hMax;
        int hMin;
        qreal hPct;
        int vVal;
        int vMax;
        int vMin;
        qreal vPct;
    } scrl;

    QPointF scrollPct;          // current position
    QPoint mousePressPt;        // for mouse scrolling

    QSize full;

    QString currentImagePath;

    bool cursorIsHidden;        // use for slideshow and full screen - review rgh
    bool moveImageLocked;       // control when con drag image around
    bool isZoomToggled;
    bool isScrollable;
    bool isMouseDrag;
    bool isTrackpadScroll;
    bool isLeftMouseBtnPressed;
    bool isMouseDoubleClick;

    int scrollCount;

    qreal zoomInc = 0.1;    // 10% delta
    qreal zoomMin = 0.05;   // 5% of original  rgh add to pref
    qreal zoomMax = 8.0;    // 800% of original

    QPoint origin;
    QRubberBand *rubberBand;
};


#endif // EMBELVIEW_H
