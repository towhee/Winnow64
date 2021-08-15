#ifndef EMBEL_H
#define EMBEL_H

#include <QtWidgets>
//#include "Views/imageview.h"
//#include "Embellish/embelexport.h"
#include "Main/global.h"
#include "Datamodel/datamodel.h"
#include "Cache/imagecache.h"
#include "Properties/embelproperties.h"
#include "Effects/graphicseffect.h"
#include "Effects/effects.h"       // temp to test
#include "Effects/graphicsitemeventfilter.h"

class Embel : public QObject
{
    Q_OBJECT

public:
    Embel(QGraphicsScene *scene, QGraphicsPixmapItem *pmItem,
          EmbelProperties *p, ImageCache *imCache, QString src = "Internal",
          QObject *object = nullptr);
//    Embel(ImageView *gv, EmbelProperties *p);
//    Embel(EmbelExport *ee, EmbelProperties *p);
    ~Embel() override;
    void updateBorder(int i);
    void updateText(int i);
    void updateGraphic(int i);
    void updateImage();
    void removeBorder(int i);
    void removeText(int i);
    void removeGraphic(int i);
    void updateStyle(QString style);
    void exportImage();
    void setRemote(bool remote = false);
    void test();
    void diagnostics(QTextStream &rpt);
    int ls;     // long side
    int ss;     // short side
    int stu;    // side to use for relative measures
    QString src;

public slots:
    void build(QString fPath = "", QString src = "");
    void clear();
    void flashObject(QString type = "", int index = 0, bool show = false);
    void refreshTexts();

protected:
    bool eventFilter(QObject *object, QEvent *event) override;

signals:
    void done();

private:
    QGraphicsScene *scene;
    QGraphicsPixmapItem *pmItem;
    GraphicsItemEventFilter *itemEventFilter;
    EmbelProperties *p;
    ImageCache *imCache;
    QString fPath;

    // set true when called from EmbelExport (see explanation in EmBel::build)
    bool isRemote = false;

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
    QList<QGraphicsTextItem*> tItems;
    QList<QGraphicsPixmapItem*> gItems;
    QList<QPixmap> graphicPixmaps;
    QGraphicsRectItem *flashItem;

    int w, h;
    int shortside;

    void doNotEmbellish();
    void borderImageCoordinates();
    void fitAspect(double aspect, Hole &size);
    QPoint canvasCoord(double x, double y, QString anchorObject,
                       QString anchorContainer, bool alignWithImageEdge);
    QPoint anchorPointOffset(QString anchorPoint, int w, int h, double rotation);
    QString anchorPointRotationEquivalent(QString anchorPoint, double rotation);
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
