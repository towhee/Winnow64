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
    QPoint canvasCoord(QString object, QString container, double x, double y);
    QPoint anchorPointOffset(QString anchorPoint, int w, int h);
    void addBordersToScene();
    void createTexts();
    void addTextsToScene();
    void addImageToScene();
};

#endif // EMBEL_H
