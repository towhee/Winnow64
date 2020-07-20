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

    // Canvas pixel coordinates
    struct Border {
        int x, y, w, h, l, r, t, b;     //  l, r, t, b = left, right, top, bottom
        QPoint tl, tc, tr, cl, cc, cr, bl, bc, br;
    };
    QList<Border>b;
//    QVector<Border>b;
    QList<QGraphicsRectItem*> bItems;
//    QVector<QGraphicsRectItem*> bItems;

    // Canvas pixel coordinates
    struct Image {
        int x, y, w, h;
        QPoint tl, tc, tr, cl, cc, cr, bl, bc, br;
    } ;
    Image image;

    int ls, w, h;
    int shortside;

    void doNotEmbellish();
    void createBorders();
    void borderImageCoordinates();
    void addBordersToScene();
    void translateImage();
};

#endif // EMBEL_H
