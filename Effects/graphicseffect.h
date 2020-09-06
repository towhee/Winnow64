#ifndef GRAPHICSEFFECT_H
#define GRAPHICSEFFECT_H

#include <QtWidgets>
#include <QGraphicsEffect>

class GraphicsEffect : public QGraphicsEffect
{
public:
    GraphicsEffect();
    struct PainterParameters {
        QPainter* painter;
        QPixmap px;
        QImage canvas;
        QRectF bound;
        QPoint offset;
        QRectF newSourceRect;
    };
    PainterParameters p;

private:
//    virtual QRectF boundingRectFor( const QRectF& sourceRect ) const;
    void shadowEffect(PainterParameters &p,
                      const QPointF offset,
                      const double radius,
                      const QColor color/*,
                      const QPointF &pos,
                      const QPixmap &px,
                      const QRectF &src*/);
    void highlightEffect(PainterParameters &p,
                         QColor color,
                         QPointF offset);
    void blurEffect(PainterParameters &p, qreal radius, bool quality,
                    bool alphaOnly, int transposed = 0);
//    void blurImage(QPainter *p, QImage &blurImage, qreal radius, bool quality,
//                   bool alphaOnly, int transposed = 0);
//    QImage halfScaled(const QImage &source);
//    void expblur(QImage &img, qreal radius, bool improvedQuality = false, int transposed = 0);

protected:
    virtual void draw(QPainter *painter);
};

#endif // GRAPHICSEFFECT_H
