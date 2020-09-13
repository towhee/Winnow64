#include "graphicseffect.h"

GraphicsEffect::GraphicsEffect(QList<winnow_effects::Effect> &effects, int globalLightDirection)
    : effects(effects)
{
    lightDirection = globalLightDirection;
}

//QRectF GraphicsEffect::boundingRectFor(const QRectF& sourceRect) const
//{
//    return p.bound;
////    return sourceRect.adjusted(-10,-10,10,10);
//}

void GraphicsEffect::draw(QPainter* painter)
{
    if (effects.length() == 0) return;

    p.painter = painter;
    QPixmap pixmap = sourcePixmap(Qt::LogicalCoordinates, &p.offset);
    p.px = sourcePixmap(Qt::LogicalCoordinates, &p.offset);
    p.canvas = pixmap.toImage();
    p.bound  = pixmap.rect();
    p.canvas.save("D:/Pictures/Temp/effect/im1.tif");

    painter->save();

    // iterate effects in style
    using namespace winnow_effects;
    for (int i = 0; i < effects.length(); ++i) {
        const Effect &ef = effects.at(i);
        QColor color;
        QPointF pt;
        qDebug() << __FUNCTION__
                 << "ef.effectType =" << ef.effectType
                 << "ef.effectName =" << ef.effectName;
        switch (ef.effectType) {
        case blur:
            blurEffect(p, ef.blur.radius, ef.blur.quality, ef.blur.transposed);
            break;
        case highlight:
            color.setRgb(ef.highlight.r, ef.highlight.g, ef.highlight.b, ef.highlight.a);
            pt.setX(ef.highlight.margin);
            pt.setY(ef.highlight.margin);
            highlightEffect(p, color, pt);
            break;
        case shadow:
            color.setRgb(ef.shadow.r, ef.shadow.g, ef.shadow.b, ef.shadow.a);
            shadowEffect(p, ef.shadow.size, ef.shadow.blurRadius, color);
        }
    }

    painter->restore();
}

QT_BEGIN_NAMESPACE
  extern Q_WIDGETS_EXPORT void qt_blurImage(QPainter *p, QImage &blurImage, qreal radius, bool quality, bool alphaOnly, int transposed = 0 );
QT_END_NAMESPACE

void GraphicsEffect::blurEffect(PainterParameters &p, qreal radius, bool quality, bool alphaOnly, int transposed)
{
//  QTransform transform = p.painter->worldTransform();
  qt_blurImage(p.painter, p.canvas, radius, quality, alphaOnly);
//  p.painter->setWorldTransform(transform);
}

void GraphicsEffect::shadowEffect(PainterParameters &p,
                                  const int size,
                                  const double radius,
                                  const QColor color)
{
    if (p.px.isNull())
        return;

    // shadow offset from object
    QPointF offset;
    double rads = lightDirection * (3.14159 / 180);
    offset.setX(-sin(rads) * size);
    offset.setY(+cos(rads) * size);

    QSize shadSize(p.px.size().width() + 2 * size, p.px.size().height() + 2 * size);
    QImage shad(shadSize, QImage::Format_ARGB32_Premultiplied);
    shad.setDevicePixelRatio(p.px.devicePixelRatioF());
    shad.fill(0);
    QPainter shadPainter(&shad);
    shadPainter.setCompositionMode(QPainter::CompositionMode_Source);
    shadPainter.drawPixmap(offset, p.px);
    shadPainter.end();

    // blur the alpha channel
    QImage blurred(shad.size(), QImage::Format_ARGB32_Premultiplied);
    blurred.setDevicePixelRatio(p.px.devicePixelRatioF());
    blurred.fill(0);
    QPainter blurPainter(&blurred);
    qt_blurImage(&blurPainter, shad, radius, false, true);
    blurPainter.end();

    shad = std::move(blurred);

    // blacken the image...
    shadPainter.begin(&shad);
    shadPainter.setCompositionMode(QPainter::CompositionMode_SourceIn);
    shadPainter.fillRect(shad.rect(), color);
    shadPainter.end();

    QPointF pos(0,0);
    // draw the blurred drop shadow...
    p.painter->drawImage(pos, shad);

    // draw the actual pixmap...
    p.painter->drawPixmap(pos, p.px, p.bound);

    qDebug() << __FUNCTION__
             << "size =" << size
             << "lightDirection =" << lightDirection
             << "p.px.size() =" << p.px.size()
             << "shadSize =" << shadSize
             << "offset =" << offset
                ;
}

void GraphicsEffect::highlightEffect(PainterParameters &p,
                                     QColor color,
                                     QPointF offset)
{
    QRectF newBound = p.bound.adjusted(-offset.x(), -offset.y(), offset.x(), offset.y());
    int w = static_cast<int>(newBound.width());
    int h = static_cast<int>(newBound.height());
    double scale = p.painter->deviceTransform().m11();
//    QImage newCanvas(QSize(w, h), QImage::Format_ARGB32_Premultiplied);
    QImage newCanvas(QSize(w/scale, h/scale), QImage::Format_ARGB32_Premultiplied);
    newCanvas.fill(color);
    QPainter tmp(&newCanvas);
//    qDebug() << __FUNCTION__ << "p.painter->worldTransform() =" << p.painter->worldTransform()
//                << "tmp->worldTransform() =" << tmp.worldTransform()
//                ;
//    tmp.setWorldTransform(p.painter->worldTransform());
//    tmp.setTransform(p.painter->deviceTransform());
//    QTransform transform = tmp.worldTransform();  // nada
    tmp.drawImage(offset, p.canvas);
//    tmp.setOpacity(0.5);
    tmp.worldTransform();
    p.canvas = newCanvas;
    p.painter->drawImage(-offset, p.canvas);
    return;
}


//#define AVG(a,b)  ( ((((a)^(b)) & 0xfefefefeUL) >> 1) + ((a)&(b)) )
//#define AVG16(a,b)  ( ((((a)^(b)) & 0xf7deUL) >> 1) + ((a)&(b)) )

//QImage GraphicsEffect::halfScaled(const QImage &source)
//{
//    if (source.width() < 2 || source.height() < 2)
//        return QImage();

//    QImage srcImage = source;

//    if (source.format() == QImage::Format_Indexed8 || source.format() == QImage::Format_Grayscale8) {
//        // assumes grayscale
//        QImage dest(source.width() / 2, source.height() / 2, srcImage.format());
//        dest.setDevicePixelRatio(source.devicePixelRatioF());

//        const uchar *src = reinterpret_cast<const uchar*>(const_cast<const QImage &>(srcImage).bits());
//        int sx = srcImage.bytesPerLine();
//        int sx2 = sx << 1;

//        uchar *dst = reinterpret_cast<uchar*>(dest.bits());
//        int dx = dest.bytesPerLine();
//        int ww = dest.width();
//        int hh = dest.height();

//        for (int y = hh; y; --y, dst += dx, src += sx2) {
//            const uchar *p1 = src;
//            const uchar *p2 = src + sx;
//            uchar *q = dst;
//            for (int x = ww; x; --x, ++q, p1 += 2, p2 += 2)
//                *q = ((int(p1[0]) + int(p1[1]) + int(p2[0]) + int(p2[1])) + 2) >> 2;
//        }

//        return dest;
//    } else if (source.format() == QImage::Format_ARGB8565_Premultiplied) {
//        QImage dest(source.width() / 2, source.height() / 2, srcImage.format());
//        dest.setDevicePixelRatio(source.devicePixelRatioF());

//        const uchar *src = reinterpret_cast<const uchar*>(const_cast<const QImage &>(srcImage).bits());
//        int sx = srcImage.bytesPerLine();
//        int sx2 = sx << 1;

//        uchar *dst = reinterpret_cast<uchar*>(dest.bits());
//        int dx = dest.bytesPerLine();
//        int ww = dest.width();
//        int hh = dest.height();

//        for (int y = hh; y; --y, dst += dx, src += sx2) {
//            const uchar *p1 = src;
//            const uchar *p2 = src + sx;
//            uchar *q = dst;
//            for (int x = ww; x; --x, q += 3, p1 += 6, p2 += 6) {
//                // alpha
//                q[0] = AVG(AVG(p1[0], p1[3]), AVG(p2[0], p2[3]));
//                // rgb
//                const quint16 p16_1 = (p1[2] << 8) | p1[1];
//                const quint16 p16_2 = (p1[5] << 8) | p1[4];
//                const quint16 p16_3 = (p2[2] << 8) | p2[1];
//                const quint16 p16_4 = (p2[5] << 8) | p2[4];
//                const quint16 result = AVG16(AVG16(p16_1, p16_2), AVG16(p16_3, p16_4));
//                q[1] = result & 0xff;
//                q[2] = result >> 8;
//            }
//        }

//        return dest;
//    } else if (source.format() != QImage::Format_ARGB32_Premultiplied
//               && source.format() != QImage::Format_RGB32)
//    {
//        srcImage = source.convertToFormat(QImage::Format_ARGB32_Premultiplied);
//    }

//    QImage dest(source.width() / 2, source.height() / 2, srcImage.format());
//    dest.setDevicePixelRatio(source.devicePixelRatioF());

//    const quint32 *src = reinterpret_cast<const quint32*>(const_cast<const QImage &>(srcImage).bits());
//    int sx = srcImage.bytesPerLine() >> 2;
//    int sx2 = sx << 1;

//    quint32 *dst = reinterpret_cast<quint32*>(dest.bits());
//    int dx = dest.bytesPerLine() >> 2;
//    int ww = dest.width();
//    int hh = dest.height();

//    for (int y = hh; y; --y, dst += dx, src += sx2) {
//        const quint32 *p1 = src;
//        const quint32 *p2 = src + sx;
//        quint32 *q = dst;
//        for (int x = ww; x; --x, q++, p1 += 2, p2 += 2)
//            *q = AVG(AVG(p1[0], p1[1]), AVG(p2[0], p2[1]));
//    }

//    return dest;
//}

//template<int aprec, int zprec>
//inline void qt_blurinner(uchar *bptr, int &zR, int &zG, int &zB, int &zA, int alpha)
//{
//    QRgb *pixel = (QRgb *)bptr;

//#define Z_MASK (0xff << zprec)
//    const int A_zprec = qt_static_shift<zprec - 24>(*pixel) & Z_MASK;
//    const int R_zprec = qt_static_shift<zprec - 16>(*pixel) & Z_MASK;
//    const int G_zprec = qt_static_shift<zprec - 8>(*pixel)  & Z_MASK;
//    const int B_zprec = qt_static_shift<zprec>(*pixel)      & Z_MASK;
//#undef Z_MASK

//    const int zR_zprec = zR >> aprec;
//    const int zG_zprec = zG >> aprec;
//    const int zB_zprec = zB >> aprec;
//    const int zA_zprec = zA >> aprec;

//    zR += alpha * (R_zprec - zR_zprec);
//    zG += alpha * (G_zprec - zG_zprec);
//    zB += alpha * (B_zprec - zB_zprec);
//    zA += alpha * (A_zprec - zA_zprec);

//#define ZA_MASK (0xff << (zprec + aprec))
//    *pixel =
//        qt_static_shift<24 - zprec - aprec>(zA & ZA_MASK)
//        | qt_static_shift<16 - zprec - aprec>(zR & ZA_MASK)
//        | qt_static_shift<8 - zprec - aprec>(zG & ZA_MASK)
//        | qt_static_shift<-zprec - aprec>(zB & ZA_MASK);
//#undef ZA_MASK
//}

//const int alphaIndex = (QSysInfo::ByteOrder == QSysInfo::BigEndian ? 0 : 3);

//template<int aprec, int zprec, bool alphaOnly>
//inline void qt_blurrow(QImage & im, int line, int alpha)
//{
//    uchar *bptr = im.scanLine(line);

//    int zR = 0, zG = 0, zB = 0, zA = 0;

//    if (alphaOnly && im.format() != QImage::Format_Indexed8)
//        bptr += alphaIndex;

//    const int stride = im.depth() >> 3;
//    const int im_width = im.width();
//    for (int index = 0; index < im_width; ++index) {
//        if (alphaOnly)
//            qt_blurinner_alphaOnly<aprec, zprec>(bptr, zA, alpha);
//        else
//            qt_blurinner<aprec, zprec>(bptr, zR, zG, zB, zA, alpha);
//        bptr += stride;
//    }

//    bptr -= stride;

//    for (int index = im_width - 2; index >= 0; --index) {
//        bptr -= stride;
//        if (alphaOnly)
//            qt_blurinner_alphaOnly<aprec, zprec>(bptr, zA, alpha);
//        else
//            qt_blurinner<aprec, zprec>(bptr, zR, zG, zB, zA, alpha);
//    }
//}

//void GraphicsEffect::expblur(QImage &img, qreal radius, bool improvedQuality, int transposed)
//{
//    // halve the radius if we're using two passes
//    if (improvedQuality)
//        radius *= qreal(0.5);

//    Q_ASSERT(img.format() == QImage::Format_ARGB32_Premultiplied
//             || img.format() == QImage::Format_RGB32
//             || img.format() == QImage::Format_Indexed8
//             || img.format() == QImage::Format_Grayscale8);

//    // choose the alpha such that pixels at radius distance from a fully
//    // saturated pixel will have an alpha component of no greater than
//    // the cutOffIntensity
//    const qreal cutOffIntensity = 2;
//    int alpha = radius <= qreal(1e-5)
//        ? ((1 << aprec)-1)
//        : qRound((1<<aprec)*(1 - qPow(cutOffIntensity * (1 / qreal(255)), 1 / radius)));

//    int img_height = img.height();
//    for (int row = 0; row < img_height; ++row) {
//        for (int i = 0; i <= int(improvedQuality); ++i)
//            qt_blurrow<aprec, zprec, alphaOnly>(img, row, alpha);
//    }

//    QImage temp(img.height(), img.width(), img.format());
//    temp.setDevicePixelRatio(img.devicePixelRatioF());
//    if (transposed >= 0) {
//        if (img.depth() == 8) {
//            qt_memrotate270(reinterpret_cast<const quint8*>(img.bits()),
//                            img.width(), img.height(), img.bytesPerLine(),
//                            reinterpret_cast<quint8*>(temp.bits()),
//                            temp.bytesPerLine());
//        } else {
//            qt_memrotate270(reinterpret_cast<const quint32*>(img.bits()),
//                            img.width(), img.height(), img.bytesPerLine(),
//                            reinterpret_cast<quint32*>(temp.bits()),
//                            temp.bytesPerLine());
//        }
//    } else {
//        if (img.depth() == 8) {
//            qt_memrotate90(reinterpret_cast<const quint8*>(img.bits()),
//                           img.width(), img.height(), img.bytesPerLine(),
//                           reinterpret_cast<quint8*>(temp.bits()),
//                           temp.bytesPerLine());
//        } else {
//            qt_memrotate90(reinterpret_cast<const quint32*>(img.bits()),
//                           img.width(), img.height(), img.bytesPerLine(),
//                           reinterpret_cast<quint32*>(temp.bits()),
//                           temp.bytesPerLine());
//        }
//    }

//    img_height = temp.height();
//    for (int row = 0; row < img_height; ++row) {
//        for (int i = 0; i <= int(improvedQuality); ++i)
//            qt_blurrow<aprec, zprec, alphaOnly>(temp, row, alpha);
//    }

//    if (transposed == 0) {
//        if (img.depth() == 8) {
//            qt_memrotate90(reinterpret_cast<const quint8*>(temp.bits()),
//                           temp.width(), temp.height(), temp.bytesPerLine(),
//                           reinterpret_cast<quint8*>(img.bits()),
//                           img.bytesPerLine());
//        } else {
//            qt_memrotate90(reinterpret_cast<const quint32*>(temp.bits()),
//                           temp.width(), temp.height(), temp.bytesPerLine(),
//                           reinterpret_cast<quint32*>(img.bits()),
//                           img.bytesPerLine());
//        }
//    } else {
//        img = temp;
//    }
//}

//void GraphicsEffect::blurImage(QPainter *p, QImage &blurImage, qreal radius,
//                               bool quality, bool alphaOnly, int transposed)
//{
//    if (blurImage.format() != QImage::Format_ARGB32_Premultiplied
//        && blurImage.format() != QImage::Format_RGB32)
//    {
//        blurImage = blurImage.convertToFormat(QImage::Format_ARGB32_Premultiplied);
//    }

//    qreal scale = 1;
//    if (radius >= 4 && blurImage.width() >= 2 && blurImage.height() >= 2) {
//        blurImage = halfScaled(blurImage);
//        scale = 2;
//        radius *= qreal(0.5);
//    }

//    if (alphaOnly)
//        qt_expblur<12, 10, true>(blurImage, radius, quality, transposed);
//    else
//        expblur<12, 10, false>(blurImage, radius, quality, transposed);

//    if (p) {
//        p->scale(scale, scale);
//        p->setRenderHint(QPainter::SmoothPixmapTransform);
//        p->drawImage(QRect(QPoint(0, 0), blurImage.size() / blurImage.devicePixelRatioF()), blurImage);
//    }
//}
