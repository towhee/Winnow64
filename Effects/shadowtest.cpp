#include "shadowtest.h"

ShadowTest::ShadowTest(QObject *parent)
{

}

void ShadowTest::set(qreal dx, qreal dy, qreal radius)
{
    _offset = QPointF(dx, dy);
    _radius = radius;
    updateBoundingRect();
}
/*
qreal ShadowTest::blurRadius() const
{
    return _radius;
}

void ShadowTest::setBlurRadius(qreal radius)
{
    _radius = radius;
    updateBoundingRect();
    emit blurRadiusChanged(radius);
}

QColor ShadowTest::color() const
{
    return _color;
}

void ShadowTest::setColor(const QColor &color)
{
    _color = color;
    update();
    emit colorChanged(color);
}

QPointF ShadowTest::offset() const
{
    return _offset;
}

void ShadowTest::setOffset(const QPointF &offset)
{
    _offset = offset;
    updateBoundingRect();
    emit offsetChanged(offset);
}
*/
QRectF ShadowTest::boundingRectFor(const QRectF &rect) const
{
    return rect.united(rect.translated(_offset).adjusted(-_radius, -_radius, _radius, _radius));
}

void ShadowTest::draw(QPainter *painter)
{
    PixmapPadMode mode = PadToEffectiveBoundingRect;

    // Draw pixmap in device coordinates to avoid pixmap scaling.
    QPoint offset;
    const QPixmap pixmap = sourcePixmap(Qt::DeviceCoordinates, &offset, mode);
    qDebug() << __FUNCTION__ << offset;
    if (pixmap.isNull())
        return;

    QTransform restoreTransform = painter->worldTransform();
    painter->setWorldTransform(QTransform());
    drawShadow(painter, offset, pixmap);
    painter->setWorldTransform(restoreTransform);
}

QT_BEGIN_NAMESPACE
  extern Q_WIDGETS_EXPORT void qt_blurImage(QPainter *p, QImage &blurImage, qreal radius, bool quality, bool alphaOnly, int transposed = 0 );
QT_END_NAMESPACE

void ShadowTest::drawShadow(QPainter *p,
                            const QPointF &pos,
                            const QPixmap &px,
                            const QRectF &src) const
{
    if (px.isNull())
        return;

    QImage tmp(px.size(), QImage::Format_ARGB32_Premultiplied);
    tmp.setDevicePixelRatio(px.devicePixelRatioF());
    tmp.fill(0);
    QPainter tmpPainter(&tmp);
    tmpPainter.setCompositionMode(QPainter::CompositionMode_Source);
    tmpPainter.drawPixmap(_offset, px);
    tmpPainter.end();

    // blur the alpha channel
    QImage blurred(tmp.size(), QImage::Format_ARGB32_Premultiplied);
    blurred.setDevicePixelRatio(px.devicePixelRatioF());
    blurred.fill(0);
    QPainter blurPainter(&blurred);
    qt_blurImage(&blurPainter, tmp, _radius, false, true);
    blurPainter.end();

    tmp = std::move(blurred);

    // apply color to shadow
    tmpPainter.begin(&tmp);
    tmpPainter.setCompositionMode(QPainter::CompositionMode_SourceIn);
    tmpPainter.fillRect(tmp.rect(), _color);
    tmpPainter.end();

    // draw the blurred drop shadow...
    p->drawImage(pos, tmp);

    // Draw the actual pixmap...
    p->drawPixmap(pos, px, src);
}
