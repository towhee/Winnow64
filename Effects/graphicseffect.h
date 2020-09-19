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
public:
    GraphicsEffect(QList<winnow_effects::Effect> &effects, int globalLightDirection);
    struct PainterParameters {
        QPainter* painter;
        QPixmap px;
        QImage im;
        QRectF bound;
        QPoint offset;
        QRectF newSourceRect;
    };
    PainterParameters p;


private:
//    virtual QRectF boundingRectFor( const QRectF& sourceRect ) const;
    void clear(PainterParameters &p);
    void shadowEffect(PainterParameters &p,
                      const int size,
                      const double radius,
                      const QColor color);
    void highlightEffect(PainterParameters &p,
                         QColor color,
                         QPointF offset);
    void blurEffect(PainterParameters &p, qreal radius, bool quality,
                    bool alphaOnly, int transposed = 0);

    QList<winnow_effects::Effect> &effects;
    int lightDirection;

//    void blurImage(QPainter *p, QImage &blurImage, qreal radius, bool quality,
//                   bool alphaOnly, int transposed = 0);
//    QImage halfScaled(const QImage &source);
//    void expblur(QImage &img, qreal radius, bool improvedQuality = false, int transposed = 0);

protected:
    virtual void draw(QPainter *painter);
};

#endif // GRAPHICSEFFECT_H
