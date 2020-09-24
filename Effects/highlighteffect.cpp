#include "highlighteffect.h"

HighlightEffect::HighlightEffect( qreal offset ) :
 QGraphicsEffect(),
// GraphicsEffect(),
 mColor( 255, 255, 0, 128 ), // yellow, semi-transparent
 mOffset( offset, offset ) {}

QRectF HighlightEffect::boundingRectFor( const QRectF& sourceRect ) const
{
 return sourceRect.adjusted( -mOffset.x(), -mOffset.y(), mOffset.x(), mOffset.y() );
}

void HighlightEffect::draw( QPainter* painter )
{
 QPoint offset;
 QPixmap pixmap;

 pixmap = sourcePixmap( Qt::LogicalCoordinates, &offset );
 QRectF bound = boundingRectFor( pixmap.rect() );

 painter->save();
 painter->setPen( Qt::NoPen );
 painter->setBrush( mColor );
 QPointF p( offset.x()-mOffset.x(), offset.y()-mOffset.y() );
 qDebug() << __FUNCTION__
          << "p.bound" << bound
          << "p.offset" << offset
          << "offset" << mOffset
          << "pt" << p
          << "pixmap.devicePixelRatioF() =" << pixmap.devicePixelRatioF();
// p.bound QRectF(0,0 184x68) p.offset QPoint(0,0) offset QPointF(0,0) pt QPointF(0,0)

 bound.moveTopLeft( p );
 painter->drawRoundedRect( bound, 5, 5, Qt::RelativeSize );
 painter->drawPixmap( offset, pixmap );
 painter->restore();
}
