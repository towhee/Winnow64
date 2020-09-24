#ifndef GRAPHICSEFFECT_H
#define GRAPHICSEFFECT_H

#include <QtWidgets>
#include <QGraphicsEffect>
#include "Main/global.h"

namespace winnow_effects
{
    struct Shadow {
        qreal size;         // % of object long side
        qreal blurRadius;
        int r, g, b, a;
    };

    struct Blur {
        qreal radius;
        bool quality;
        int transposed;
    };

    struct Highlight {
        int r, g, b, a;
        int margin;
    };

    // when add an effect must add to enum and union
    enum EffectType {blur, highlight, shadow};
    struct Effect {
        enum EffectType effectType;
        QString effectName;         // unique: req'd in case more than one of same effect
        int effectOrder;            // order executed within style
        union {
            Blur blur;
            Highlight highlight;
            Shadow shadow;
        };
    };
}

class GraphicsEffect : public QGraphicsEffect
{
    Q_OBJECT
public:
    GraphicsEffect(QObject *parent = nullptr);
//    GraphicsEffect(QList<winnow_effects::Effect> &effects, int globalLightDirection);
    struct PainterParameters {
        QPainter* painter;
        QSize srcSize;
        QImage srcIm;                   // original image
        QImage overlay;                   // effect applied and may use by another effect
        int dx = 0;                     // offset from source bounding rect
        int dy = 0;                     // offset from source bounding rect
        int dw = 0;                     // delta from source bounding rect width
        int dh = 0;                     // delta from source bounding rect height
        QPointF offset;
        QRectF rect;
    };

    PainterParameters p;

    void set(QList<winnow_effects::Effect> &effects, int globalLightDirection);

private:
    virtual QRectF boundingRectFor(const QRectF& rect ) const;
//    void clear(PainterParameters &p);
    void shadowEffect(PainterParameters &p,
                      const int size,
                      double radius,
                      QColor color);
    void highlightEffect(PainterParameters &p,
                         QColor color,
                         QPointF offset);
    void blurEffect(PainterParameters &p, qreal radius, bool quality,
                    bool alphaOnly, int transposed = 0);

    QList<winnow_effects::Effect> effects;
//    QList<winnow_effects::Effect> &effects;
    int lightDirection;
    qreal _expansion;

//    void blurImage(QPainter *p, QImage &blurImage, qreal radius, bool quality,
//                   bool alphaOnly, int transposed = 0);
//    QImage halfScaled(const QImage &source);
//    void expblur(QImage &img, qreal radius, bool improvedQuality = false, int transposed = 0);

protected:
    virtual void draw(QPainter *painter);
};

#endif // GRAPHICSEFFECT_H
