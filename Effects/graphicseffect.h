#ifndef GRAPHICSEFFECT_H
#define GRAPHICSEFFECT_H

#include <QtWidgets>
#include <QGraphicsEffect>
#include "Main/global.h"

namespace winnow_effects
{
    struct Shadow {
        qreal length;               // % of object long side or text height
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
        int top, left, right, bottom;
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
    void set(QList<winnow_effects::Effect> &effects,
             int globalLightDirection,
             double rotation,
             QRectF boundRect);
    struct Margin {
        int top = 0;
        int left = 0;
        int right = 0;
        int bottom = 0;
    };

private:
    virtual QRectF boundingRectFor(const QRectF& rect ) const;
    void shadowEffect(double size, double radius, QColor color);
    void highlightEffect(QColor color, Margin margin);
    void blurEffect(QPainter *painter, qreal radius, bool quality, bool alphaOnly, int transposed = 0);

    QList<winnow_effects::Effect> effects;

    QRectF srcRectZeroRotation;
    QRectF srcRect;
    QPixmap srcPixmap;
    QImage overlay;
    QPointF offset;
    Margin m;
    qreal expansion = 0;
    int lightDirection;
    double rotation;

    /*
    void blurImage(QPainter *p, QImage &blurImage, qreal radius, bool quality,
                   bool alphaOnly, int transposed = 0);
    QImage halfScaled(const QImage &source);
    void expblur(QImage &img, qreal radius, bool improvedQuality = false, int transposed = 0);
    */
protected:
    virtual void draw(QPainter *painter);
};

#endif // GRAPHICSEFFECT_H
