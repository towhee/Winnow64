#include "graphicseffect.h"

GraphicsEffect::GraphicsEffect(QObject *parent)
{
}

QRectF GraphicsEffect::boundingRectFor(const QRectF& rect) const
{
    return rect.united(rect.translated(p.offset).adjusted(-_expansion, -_expansion, _expansion, _expansion));
}

void GraphicsEffect::set(QList<winnow_effects::Effect> &effects, int globalLightDirection)
{
    this->effects = effects;
    lightDirection = globalLightDirection;

    // iterate effects in style to set boundingRect
    using namespace winnow_effects;
    /*
    std::sort(effects.begin(), effects.end(), [](Effect const &l, Effect const &r) {
              return l.effectOrder < r.effectOrder; });
//              */
    for (int i = 0; i < effects.length(); ++i) {
        const Effect &ef = effects.at(i);
//        qDebug() << __FUNCTION__ << ef.effectName;
        QColor color;
        QPointF pt;
        /*
        qDebug() << __FUNCTION__
                 << "ef.effectOrder =" << ef.effectOrder
                 << "ef.effectName =" << ef.effectName
                    ;
//                    */
        switch (ef.effectType) {
        case blur:

            break;
        case highlight:

            break;
        case shadow:
            qreal size = ef.shadow.size;
            double rads = lightDirection * (3.14159 / 180);
            p.offset.setX(-sin(rads) * size);
            p.offset.setY(+cos(rads) * size);
            qDebug() << __FUNCTION__ << "p.offset =" << p.offset;
            _expansion = ef.shadow.blurRadius;
            updateBoundingRect();
        }
    }
}

void GraphicsEffect::draw(QPainter* painter)
{
    if (effects.length() == 0) return;

    p.painter = painter;
    QPoint sourceOffset;
    PixmapPadMode mode = PadToEffectiveBoundingRect;
    const QPixmap pixmap = sourcePixmap(Qt::DeviceCoordinates, &sourceOffset, mode);
    qDebug() << __FUNCTION__ << sourceOffset;
    p.rect  = pixmap.rect();
    p.srcSize = pixmap.size();
    // source image ie text
    p.srcIm = pixmap.toImage();
    p.overlay = p.srcIm.copy();

//    painter->save();

    // iterate effects in style
    using namespace winnow_effects;
    /*
    std::sort(effects.begin(), effects.end(), [](Effect const &l, Effect const &r) {
              return l.effectOrder < r.effectOrder; });
//              */
    for (int i = 0; i < effects.length(); ++i) {
        const Effect &ef = effects.at(i);
        QColor color;
        QPointF pt;
        /*
        qDebug() << __FUNCTION__
                 << "ef.effectOrder =" << ef.effectOrder
                 << "ef.effectName =" << ef.effectName
                    ;
//                    */
        switch (ef.effectType) {
        case blur:
            blurEffect(p, ef.blur.radius, ef.blur.quality, 0/*ef.blur.transposed*/);
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

    // world transform required for object rotation
    QTransform restoreTransform = painter->worldTransform();
    painter->setWorldTransform(QTransform());
    p.painter->drawImage(sourceOffset, p.overlay, p.rect);
    painter->setWorldTransform(restoreTransform);

//    painter->restore();
}

QT_BEGIN_NAMESPACE
  extern Q_WIDGETS_EXPORT void qt_blurImage(QPainter *p, QImage &blurImage, qreal radius, bool quality, bool alphaOnly, int transposed = 0 );
QT_END_NAMESPACE

void GraphicsEffect::blurEffect(PainterParameters &p, qreal radius, bool quality, bool alphaOnly, int transposed)
{

    qt_blurImage(p.painter, p.overlay, radius, quality, alphaOnly);
//    qt_blurImage(p.painter, p.im, radius, quality, alphaOnly);
}

void GraphicsEffect::shadowEffect(PainterParameters &p,
                                  int size,
                                  double radius,
                                  QColor color)
{
    if (p.srcIm.isNull())
        return;

    double rads = lightDirection * (3.14159 / 180);
    QPointF shadowOffset;
    shadowOffset.setX(-sin(rads) * size);
    shadowOffset.setY(+cos(rads) * size);

    // create a transparent image with room for the shadow
    QImage shadIm(p.overlay.size(), QImage::Format_ARGB32_Premultiplied);
    shadIm.fill(Qt::transparent);
    QPainter shadPainter(&shadIm);
    shadPainter.drawImage(0,0, p.overlay);
    shadPainter.end();

    // blur the alpha channel
    QImage blurred(shadIm.size(), QImage::Format_ARGB32_Premultiplied);
    blurred.fill(Qt::transparent);
    QPainter blurPainter(&blurred);
    qt_blurImage(&blurPainter, shadIm, radius, false, true);
    blurPainter.end();

    shadIm = std::move(blurred);

    // apply color to shadow
    shadPainter.begin(&shadIm);
    shadPainter.setCompositionMode(QPainter::CompositionMode_SourceIn);
    shadPainter.fillRect(shadIm.rect(), color);
    shadPainter.end();

    // compose adjusted image
    QPainter overlayPainter(&p.overlay);
    overlayPainter.setCompositionMode(QPainter::CompositionMode_DestinationOver);
    overlayPainter.drawImage(shadowOffset, shadIm);
    overlayPainter.end();
    /*
    shadIm.save("D:/Pictures/Temp/effect/shad.tif");
    p.adjIm.save("D:/Pictures/Temp/effect/adjIm.tif");
//    */
    return;
}

void GraphicsEffect::highlightEffect(PainterParameters &p,
                                     QColor color,
                                     QPointF offset)
{
    QRectF newBound = p.rect.adjusted(-offset.x(), -offset.y(), offset.x(), offset.y());
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
    tmp.drawImage(offset, p.srcIm);
//    tmp.setOpacity(0.5);
    tmp.worldTransform();
    p.srcIm = newCanvas;
    p.painter->drawImage(-offset, p.srcIm);
    return;
}



/*
#define AVG(a,b)  ( ((((a)^(b)) & 0xfefefefeUL) >> 1) + ((a)&(b)) )
#define AVG16(a,b)  ( ((((a)^(b)) & 0xf7deUL) >> 1) + ((a)&(b)) )

QImage GraphicsEffect::halfScaled(const QImage &source)
{
    if (source.width() < 2 || source.height() < 2)
        return QImage();

    QImage srcImage = source;

    if (source.format() == QImage::Format_Indexed8 || source.format() == QImage::Format_Grayscale8) {
        // assumes grayscale
        QImage dest(source.width() / 2, source.height() / 2, srcImage.format());
        dest.setDevicePixelRatio(source.devicePixelRatioF());

        const uchar *src = reinterpret_cast<const uchar*>(const_cast<const QImage &>(srcImage).bits());
        int sx = srcImage.bytesPerLine();
        int sx2 = sx << 1;

        uchar *dst = reinterpret_cast<uchar*>(dest.bits());
        int dx = dest.bytesPerLine();
        int ww = dest.width();
        int hh = dest.height();

        for (int y = hh; y; --y, dst += dx, src += sx2) {
            const uchar *p1 = src;
            const uchar *p2 = src + sx;
            uchar *q = dst;
            for (int x = ww; x; --x, ++q, p1 += 2, p2 += 2)
                *q = ((int(p1[0]) + int(p1[1]) + int(p2[0]) + int(p2[1])) + 2) >> 2;
        }

        return dest;
    } else if (source.format() == QImage::Format_ARGB8565_Premultiplied) {
        QImage dest(source.width() / 2, source.height() / 2, srcImage.format());
        dest.setDevicePixelRatio(source.devicePixelRatioF());

        const uchar *src = reinterpret_cast<const uchar*>(const_cast<const QImage &>(srcImage).bits());
        int sx = srcImage.bytesPerLine();
        int sx2 = sx << 1;

        uchar *dst = reinterpret_cast<uchar*>(dest.bits());
        int dx = dest.bytesPerLine();
        int ww = dest.width();
        int hh = dest.height();

        for (int y = hh; y; --y, dst += dx, src += sx2) {
            const uchar *p1 = src;
            const uchar *p2 = src + sx;
            uchar *q = dst;
            for (int x = ww; x; --x, q += 3, p1 += 6, p2 += 6) {
                // alpha
                q[0] = AVG(AVG(p1[0], p1[3]), AVG(p2[0], p2[3]));
                // rgb
                const quint16 p16_1 = (p1[2] << 8) | p1[1];
                const quint16 p16_2 = (p1[5] << 8) | p1[4];
                const quint16 p16_3 = (p2[2] << 8) | p2[1];
                const quint16 p16_4 = (p2[5] << 8) | p2[4];
                const quint16 result = AVG16(AVG16(p16_1, p16_2), AVG16(p16_3, p16_4));
                q[1] = result & 0xff;
                q[2] = result >> 8;
            }
        }

        return dest;
    } else if (source.format() != QImage::Format_ARGB32_Premultiplied
               && source.format() != QImage::Format_RGB32)
    {
        srcImage = source.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    }

    QImage dest(source.width() / 2, source.height() / 2, srcImage.format());
    dest.setDevicePixelRatio(source.devicePixelRatioF());

    const quint32 *src = reinterpret_cast<const quint32*>(const_cast<const QImage &>(srcImage).bits());
    int sx = srcImage.bytesPerLine() >> 2;
    int sx2 = sx << 1;

    quint32 *dst = reinterpret_cast<quint32*>(dest.bits());
    int dx = dest.bytesPerLine() >> 2;
    int ww = dest.width();
    int hh = dest.height();

    for (int y = hh; y; --y, dst += dx, src += sx2) {
        const quint32 *p1 = src;
        const quint32 *p2 = src + sx;
        quint32 *q = dst;
        for (int x = ww; x; --x, q++, p1 += 2, p2 += 2)
            *q = AVG(AVG(p1[0], p1[1]), AVG(p2[0], p2[1]));
    }

    return dest;
}

template<int aprec, int zprec>
inline void qt_blurinner(uchar *bptr, int &zR, int &zG, int &zB, int &zA, int alpha)
{
    QRgb *pixel = (QRgb *)bptr;

#define Z_MASK (0xff << zprec)
    const int A_zprec = qt_static_shift<zprec - 24>(*pixel) & Z_MASK;
    const int R_zprec = qt_static_shift<zprec - 16>(*pixel) & Z_MASK;
    const int G_zprec = qt_static_shift<zprec - 8>(*pixel)  & Z_MASK;
    const int B_zprec = qt_static_shift<zprec>(*pixel)      & Z_MASK;
#undef Z_MASK

    const int zR_zprec = zR >> aprec;
    const int zG_zprec = zG >> aprec;
    const int zB_zprec = zB >> aprec;
    const int zA_zprec = zA >> aprec;

    zR += alpha * (R_zprec - zR_zprec);
    zG += alpha * (G_zprec - zG_zprec);
    zB += alpha * (B_zprec - zB_zprec);
    zA += alpha * (A_zprec - zA_zprec);

#define ZA_MASK (0xff << (zprec + aprec))
    *pixel =
        qt_static_shift<24 - zprec - aprec>(zA & ZA_MASK)
        | qt_static_shift<16 - zprec - aprec>(zR & ZA_MASK)
        | qt_static_shift<8 - zprec - aprec>(zG & ZA_MASK)
        | qt_static_shift<-zprec - aprec>(zB & ZA_MASK);
#undef ZA_MASK
}

const int alphaIndex = (QSysInfo::ByteOrder == QSysInfo::BigEndian ? 0 : 3);

template<int aprec, int zprec, bool alphaOnly>
inline void qt_blurrow(QImage & im, int line, int alpha)
{
    uchar *bptr = im.scanLine(line);

    int zR = 0, zG = 0, zB = 0, zA = 0;

    if (alphaOnly && im.format() != QImage::Format_Indexed8)
        bptr += alphaIndex;

    const int stride = im.depth() >> 3;
    const int im_width = im.width();
    for (int index = 0; index < im_width; ++index) {
        if (alphaOnly)
            qt_blurinner_alphaOnly<aprec, zprec>(bptr, zA, alpha);
        else
            qt_blurinner<aprec, zprec>(bptr, zR, zG, zB, zA, alpha);
        bptr += stride;
    }

    bptr -= stride;

    for (int index = im_width - 2; index >= 0; --index) {
        bptr -= stride;
        if (alphaOnly)
            qt_blurinner_alphaOnly<aprec, zprec>(bptr, zA, alpha);
        else
            qt_blurinner<aprec, zprec>(bptr, zR, zG, zB, zA, alpha);
    }
}

void GraphicsEffect::expblur(QImage &img, qreal radius, bool improvedQuality, int transposed)
{
    // halve the radius if we're using two passes
    if (improvedQuality)
        radius *= qreal(0.5);

    Q_ASSERT(img.format() == QImage::Format_ARGB32_Premultiplied
             || img.format() == QImage::Format_RGB32
             || img.format() == QImage::Format_Indexed8
             || img.format() == QImage::Format_Grayscale8);

    // choose the alpha such that pixels at radius distance from a fully
    // saturated pixel will have an alpha component of no greater than
    // the cutOffIntensity
    const qreal cutOffIntensity = 2;
    int alpha = radius <= qreal(1e-5)
        ? ((1 << aprec)-1)
        : qRound((1<<aprec)*(1 - qPow(cutOffIntensity * (1 / qreal(255)), 1 / radius)));

    int img_height = img.height();
    for (int row = 0; row < img_height; ++row) {
        for (int i = 0; i <= int(improvedQuality); ++i)
            qt_blurrow<aprec, zprec, alphaOnly>(img, row, alpha);
    }

    QImage temp(img.height(), img.width(), img.format());
    temp.setDevicePixelRatio(img.devicePixelRatioF());
    if (transposed >= 0) {
        if (img.depth() == 8) {
            qt_memrotate270(reinterpret_cast<const quint8*>(img.bits()),
                            img.width(), img.height(), img.bytesPerLine(),
                            reinterpret_cast<quint8*>(temp.bits()),
                            temp.bytesPerLine());
        } else {
            qt_memrotate270(reinterpret_cast<const quint32*>(img.bits()),
                            img.width(), img.height(), img.bytesPerLine(),
                            reinterpret_cast<quint32*>(temp.bits()),
                            temp.bytesPerLine());
        }
    } else {
        if (img.depth() == 8) {
            qt_memrotate90(reinterpret_cast<const quint8*>(img.bits()),
                           img.width(), img.height(), img.bytesPerLine(),
                           reinterpret_cast<quint8*>(temp.bits()),
                           temp.bytesPerLine());
        } else {
            qt_memrotate90(reinterpret_cast<const quint32*>(img.bits()),
                           img.width(), img.height(), img.bytesPerLine(),
                           reinterpret_cast<quint32*>(temp.bits()),
                           temp.bytesPerLine());
        }
    }

    img_height = temp.height();
    for (int row = 0; row < img_height; ++row) {
        for (int i = 0; i <= int(improvedQuality); ++i)
            qt_blurrow<aprec, zprec, alphaOnly>(temp, row, alpha);
    }

    if (transposed == 0) {
        if (img.depth() == 8) {
            qt_memrotate90(reinterpret_cast<const quint8*>(temp.bits()),
                           temp.width(), temp.height(), temp.bytesPerLine(),
                           reinterpret_cast<quint8*>(img.bits()),
                           img.bytesPerLine());
        } else {
            qt_memrotate90(reinterpret_cast<const quint32*>(temp.bits()),
                           temp.width(), temp.height(), temp.bytesPerLine(),
                           reinterpret_cast<quint32*>(img.bits()),
                           img.bytesPerLine());
        }
    } else {
        img = temp;
    }
}

void GraphicsEffect::blurImage(QPainter *p, QImage &blurImage, qreal radius,
                               bool quality, bool alphaOnly, int transposed)
{
    if (blurImage.format() != QImage::Format_ARGB32_Premultiplied
        && blurImage.format() != QImage::Format_RGB32)
    {
        blurImage = blurImage.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    }

    qreal scale = 1;
    if (radius >= 4 && blurImage.width() >= 2 && blurImage.height() >= 2) {
        blurImage = halfScaled(blurImage);
        scale = 2;
        radius *= qreal(0.5);
    }

    if (alphaOnly)
        qt_expblur<12, 10, true>(blurImage, radius, quality, transposed);
    else
        expblur<12, 10, false>(blurImage, radius, quality, transposed);

    if (p) {
        p->scale(scale, scale);
        p->setRenderHint(QPainter::SmoothPixmapTransform);
        p->drawImage(QRect(QPoint(0, 0), blurImage.size() / blurImage.devicePixelRatioF()), blurImage);
    }
}
*/
