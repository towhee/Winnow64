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
    void test();

private:
    EmbelView *ev;
    EmbelProperties *p;

    struct Border {
        int x, y, w, h, l, r, t, b;     //  l, r, t, b = left, right, top, bottom
    };
    struct Image {
        int x, y, w, h;
    } image;

    Border b;
    QRect sR;
    QRect bR;
    QRect iR;
    int ls;
    int shortside;
    void resizeItems();
};

#endif // EMBEL_H
