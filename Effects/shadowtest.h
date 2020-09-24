#ifndef SHADOWTEST_H
#define SHADOWTEST_H

#include <QWidget>
#include <QGraphicsEffect>
#include "Main/global.h"

class ShadowTest: public QGraphicsEffect
{
    Q_OBJECT
    /*
    Q_PROPERTY(QPointF offset READ offset WRITE setOffset NOTIFY offsetChanged)
    Q_PROPERTY(qreal xOffset READ xOffset WRITE setXOffset NOTIFY offsetChanged)
    Q_PROPERTY(qreal yOffset READ yOffset WRITE setYOffset NOTIFY offsetChanged)
    Q_PROPERTY(qreal blurRadius READ blurRadius WRITE setBlurRadius NOTIFY blurRadiusChanged)
    Q_PROPERTY(QColor color READ color WRITE setColor NOTIFY colorChanged)
    */
public:
    ShadowTest(QObject *parent = nullptr);

    QRectF boundingRectFor(const QRectF &rect) const override;
    /*
    QPointF offset() const;

    inline qreal xOffset() const
    { return offset().x(); }

    inline qreal yOffset() const
    { return offset().y(); }

    qreal blurRadius() const;
    QColor color() const;
    */
    void set(qreal dx, qreal dy, qreal radius);

public slots:
    /*
    void setOffset(const QPointF &ofs);

    inline void setOffset(qreal dx, qreal dy)
    { setOffset(QPointF(dx, dy)); }

    inline void setOffset(qreal d)
    { setOffset(QPointF(d, d)); }

    inline void setXOffset(qreal dx)
    { setOffset(QPointF(dx, yOffset())); }

    inline void setYOffset(qreal dy)
    { setOffset(QPointF(xOffset(), dy)); }

    void setBlurRadius(qreal blurRadius);
    void setColor(const QColor &color);
    */
signals:
    /*
    void offsetChanged(const QPointF &offset);
    void blurRadiusChanged(qreal blurRadius);
    void colorChanged(const QColor &color);
    */
protected:
    void draw(QPainter *painter) override;

private:
    void drawShadow(QPainter *p, const QPointF &pos, const QPixmap &px, const QRectF &src = QRectF()) const;
    QPointF _offset;
    qreal _radius;
    QColor _color;
};

#endif // SHADOWTEST_H
