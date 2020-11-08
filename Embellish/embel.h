#ifndef EMBEL_H
#define EMBEL_H

#include <QtWidgets>
//#include "Views/imageview.h"
//#include "Embellish/embelexport.h"
#include "Properties/embelproperties.h"
#include "Effects/highlighteffect.h"
#include "Effects/graphicseffect.h"
#include "Effects/shadowtest.h"
#include "Effects/effects.h"       // temp to test

class Embel : public QObject
{
    Q_OBJECT

public:
    Embel(QGraphicsScene *scene, QGraphicsPixmapItem *pmItem, EmbelProperties *p);
//    Embel(ImageView *gv, EmbelProperties *p);
//    Embel(EmbelExport *ee, EmbelProperties *p);
    ~Embel();
    void updateBorder(int i);
    void updateText(int i);
    void updateGraphic(int i);
    void updateImage();
    void removeBorder(int i);
    void removeText(int i);
    void removeGraphic(int i);
    void updateStyle(QString style);
    void exportImage();
    void test();
    void diagnostic();

public slots:
    void build();
    void clear();
    void flashObject(QString type = "", int index = 0, bool show = false);

signals:
    void done();

private:
    QGraphicsScene *scene;
    QGraphicsPixmapItem *pmItem;
    EmbelProperties *p;

    enum zLevel {
        ZBorder,
        ZImage,
        ZGraphic,
        ZText,
        ZFlash
    };

    // the area inside the frame where the image fits
    struct Hole {
        int w;          // width
        int h;          // height
        int wb;         // total amount of left/right borders
        int hb;         // total amount of top/bottom borders
        int area;
    };
    Hole hole;
//    QVector<Hole> hole;

    // Image canvas pixel coordinates
    struct Image {
        int x, y, w, h;
        QPoint tl, tc, tr, cl, cc, cr, bl, bc, br;
    } ;
    Image image;
    qreal imageAspect;

    // Border canvas pixel coordinates
    struct Border {
        int x, y, w, h, l, r, t, b;     //  l, r, t, b = left, right, top, bottom
        QPoint tl, tc, tr, cl, cc, cr, bl, bc, br;
    };
    QList<Border>b;
    QList<QGraphicsRectItem*> bItems;

    // Text canvas pixel coordinates
    struct Text {};
    QList<QGraphicsTextItem*> tItems;

    QList<QGraphicsPixmapItem*> gItems;
    QList<QPixmap> graphicPixmaps;

    QGraphicsRectItem *flashItem;

    int ls, w, h;
    int shortside;

    void doNotEmbellish();
    void borderImageCoordinates();
    void fitAspect(double aspect, Hole &size);
    QPoint canvasCoord(double x, double y, QString anchorObject, QString anchorContainer);
    QPoint anchorPointOffset(QString anchorPoint, int w, int h);
    void createBorders();
    void createTexts();
    void createGraphics();
    void addBordersToScene();
    void addTextsToScene();
    void addGraphicsToScene();
    void addImageToScene();
    void addFlashToScene();
};

#endif // EMBEL_H
