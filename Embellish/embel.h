#ifndef EMBEL_H
#define EMBEL_H

#include <QtWidgets>
#include "Views/imageview.h"
#include "Properties/embelproperties.h"

class Embel : public QObject
{
    Q_OBJECT

public:
    Embel(ImageView *ev, EmbelProperties *p);
    void build();
    void test();
    void diagnostic();

public slots:
    void doNotEmbellish();

private:
    ImageView *ev;
    EmbelProperties *p;

    // Canvas pixel coordinates
    struct Border {
        int x, y, w, h, l, r, t, b;     //  l, r, t, b = left, right, top, bottom
        QPoint tl, tc, tr, cl, cc, cr, bl, bc, br;
    };
    QVector<Border>b;
    QVector<QGraphicsRectItem*> bItems;

    // Canvas pixel coordinates
    struct Image {
        int x, y, w, h;
        QPoint tl, tc, tr, cl, cc, cr, bl, bc, br;
    } ;
    Image image;

    int ls, w, h;
    int shortside;

//    void readModel();
    void createBorders();
    void borderImageCoordinates();
    void addBordersToScene();
    void translateImage();
};

#endif // EMBEL_H
