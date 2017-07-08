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
    void refresh();
    int getImageWidth();    // used for make thumbs fit which may be toast
    int getImageHeight();

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
    void zoom100();
    void zoomTo(float zoomTo);
    void zoomToggle();

signals:
    void togglePick();
    void updateStatus(bool, QString);

private slots:

protected:
    void resizeEvent(QResizeEvent *event);
//    void showEvent(QShowEvent *event);
    void mouseMoveEvent(QMouseEvent *event);
    void mouseDoubleClickEvent(QMouseEvent *event);
    void mousePressEvent(QMouseEvent *event);
    void mouseReleaseEvent(QMouseEvent *event);

private:
    QWidget *mainWindow;
    Metadata *metadata;
    ImageCache *imageCacheThread;
    ThumbView *thumbView;
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

    pt mouse;

    QString currentImagePath;
    bool shootingInfoVisible;

    bool cursorIsHidden;        // use for slideshow and full screen - review rgh
    bool moveImageLocked;       // control when con drag image around
    bool mouseZoom100Pct;       // flag in resize to scale to 100%
    bool mouseZoomFit;          // flag in resize to scale to fit window
    bool isZoom;
    bool isMouseDrag;
    bool isMouseDoubleClick;
    bool isMouseClickInLabel;

    float zoomFit;
    float zoomInc = 0.1;    // 10% delta
    float zoomMin = 0.05;   // 5% of original  rgh add to pref
    float zoomMax = 8.0;    // 800% of original

    void resizeImage();
    void movePickIcon();
    void setMouseMoveData(bool lockMove, int lMouseX, int lMouseY);
    void centerImage(QSize &imgSize);
    void moveImage(QSize &imgSize);

    QRect windowRect();
    bool inImageView(QPoint canvasPt);
    void transform();
};

#endif // IMAGEVIEW_H

