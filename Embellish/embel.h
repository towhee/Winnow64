#ifndef EMBEL_H
#define EMBEL_H

#include <QtWidgets>
#include "Views/imageview.h"
#include "Properties/embelproperties.h"

class Embel : public QObject
{
    Q_OBJECT

public:
    Embel(ImageView *iv, EmbelProperties *p);
    void test();
    void diagnostic();

public slots:
    void build();
    void clear();

private:
    ImageView *iv;
    EmbelProperties *p;

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

    int ls, w, h;
    int shortside;

    void doNotEmbellish();
    void createBorders();
    void borderImageCoordinates();
    void fitAspect(double aspect, Hole &size);
    QPoint textCanvasCoord(int n);
    QPoint anchorPointOffset(QString anchorPoint, int w, int h);
    void addBordersToScene();
    void createTexts();
    void addTextsToScene();
    void addImageToScene();
};

#endif // EMBEL_H
