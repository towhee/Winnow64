#ifndef EMBEL_H
#define EMBEL_H

#include <QtWidgets>
#include "embelview.h"
#include "Properties/embelproperties.h"

class Embel : public QObject
{
    Q_OBJECT
//    friend class EmbelProperties;

public:
    Embel(EmbelView *ev, EmbelProperties *p);
    void build();
    void test();
    void diagnostic();

private:
    EmbelView *ev;
    EmbelProperties *p;

//    enum ModelHeaders {
//        MH_Templates,
//        MH_File,
//        MH_Image,
//        MH_Borders,
//        MH_Texts,
//        MH_Rectangles,
//        MH_Graphics
//    };

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
