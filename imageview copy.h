#ifndef IMAGEVIEW_H
#define IMAGEVIEW_H

#include <QtWidgets>
#include <QHash>
#include "metadata.h"
#include "imagecache.h"
#include "thumbcache.h"
#include "dropshadowlabel.h"

class ImageView : public QWidget
{
    Q_OBJECT

public:
    ImageView(QWidget *parent, Metadata *metadata, ImageCache *imageCacheThread,
             ThumbView *thumbView, bool isShootingInfoVisible, bool isCompareMode);

    QScrollArea *scrlArea;
    QLabel *infoLabel;
    QLabel *infoLabelShadow;
    DropShadowLabel *infoDropShadow;
    float zoom;
    QModelIndex imageIndex;

    bool loadPixmap(QString &imageFullPath, QPixmap &pm);
    bool loadImage(QModelIndex idx, QString imageFileName);
    void clear();
    void setCursorHiding(bool hide);
    void setClickZoom(float clickZoom);
    void refresh();
    int getImageWidth();    // used for make thumbs fit which may be toast
    int getImageHeight();

    void compareZoomAtCoord(QPointF coord, bool isZoom);
    void deltaMoveImage(QPoint &delta);

    void setImageLabelSize(QSize newSize);      // req'd?
    QSize imageSize();

    void rotateByExifRotation(QImage &image, QString &imageFullPath);
    void setInfo(QString infoString);
    QLabel *pickLabel;      // visibility controlled in MW

public slots:
    void monitorCursorState();
    void copyImage();
    void pasteImage();
    void thumbClick(float xPct, float yPct);
    void zoomIn();
    void zoomOut();
    void zoomToFit();
    void zoom50();
    void zoom100();
    void zoom200();
    void zoomTo(float zoomTo);
    void zoomToggle();

signals:
    void togglePick();
    void updateStatus(bool, QString);
    void compareZoom(QPointF coord, QModelIndex imageIndex, bool isZoom);
    void comparePan(QPoint delta, QModelIndex imageIndex);

private slots:

protected:
    void resizeEvent(QResizeEvent *event);
//    void showEvent(QShowEvent *event);
    void mouseMoveEvent(QMouseEvent *event);
    void mouseDoubleClickEvent(QMouseEvent *event);
    void mousePressEvent(QMouseEvent *event);
    void mouseReleaseEvent(QMouseEvent *event);
    void enterEvent(QEvent *event);

private:
    QWidget *mainWindow;
    Metadata *metadata;
    ImageCache *imageCacheThread;
    ThumbView *thumbView;
    bool isCompareMode;
    QImageReader imageReader;
    QLabel *imageLabel;
    QPixmap displayPixmap;
    QImage origImage;
    QImage thumbsUp;
    QImage displayImage;
    QTimer *mouseMovementTimer;

    struct intSize
    {
        int w;
        int h;
        int x;
        int y;
    };

    intSize i;     // image
    intSize v;     // view
    intSize w;     // canvas or label
    intSize f;     // label in fit view

    struct floatSize
    {
        float w;
        float h;
    };

    struct pt
    {
        int x;
        int y;
    };

    typedef pt pt;
    pt mouse;

    QPointF compareMouseRelLoc;

    QString currentImagePath;
    bool shootingInfoVisible;

    bool cursorIsHidden;                // use for slideshow and full screen - review rgh
    bool moveImageLocked;               // control when con drag image around
    bool mouseZoom100Pct;               // flag in resize to scale to 100%
    bool mouseZoomFit;                  // flag in resize to scale to fit window
    bool isZoom;
    bool isMouseDrag;
    bool isMouseDoubleClick;
    bool isMouseClickInLabel;
    bool isResizeSourceMouseClick;      // prevent recursive infinite loop
    bool isPreview;

    float zoomFit;
    float zoomInc = 0.1;    // 10% delta
    float zoomMin = 0.05;   // 5% of original  rgh add to pref
    float zoomMax = 8.0;    // 800% of original
    float clickZoom = 1.0;

    void resizeImage();
    void movePickIcon();
    void setMouseMoveData(bool lockMove, int lMouseX, int lMouseY);
    void centerImage(QSize &imgSize);
    void moveImage(QSize &imgSize);
    bool previewFitsZoom();

    QRect windowRect();
    bool inImageView(QPoint canvasPt);
    void transform();
};

#endif // IMAGEVIEW_H
