#include "graphicseffect.h"
#include "effect.h"

GraphicsEffect::GraphicsEffect(QObject *parent)
{
    qDebug() << __FUNCTION__;
}

QRectF GraphicsEffect::boundingRectFor(const QRectF& rect) const
{
    return rect.united(rect.translated(offset).adjusted(-m.left, -m.top, m.right, m.bottom));
}

void GraphicsEffect::set(QList<winnow_effects::Effect> &effects,
                         int globalLightDirection,
                         double rotation,
                         QRectF boundRect)
{
    this->effects = &effects;
    lightDirection = globalLightDirection;
    this->rotation = rotation;
    srcRectZeroRotation = boundRect;

    // iterate effects in style to set boundingRect
    using namespace winnow_effects;
    /*
    std::sort(effects.begin(), effects.end(), [](Effect const &l, Effect const &r) {
              return l.effectOrder < r.effectOrder; });
//              */
    for (int i = 0; i < effects.length(); ++i) {
        const Effect &ef = effects.at(i);
        /*
        qDebug() << __FUNCTION__
                 << "ef.effectOrder =" << ef.effectOrder
                 << "ef.effectName =" << ef.effectName
                    ;
//                    */
        switch (ef.effectType) {
        case blur:
//            if (m.top < ef.blur.radius) m.top = ef.blur.radius;
//            if (m.left < ef.blur.radius) m.left = ef.blur.radius;
//            if (m.right < ef.blur.radius) m.right = ef.blur.radius;
//            if (m.bottom < ef.blur.radius) m.bottom = ef.blur.radius;
            break;
        case highlight:
            if (m.top < ef.highlight.top) m.top = ef.highlight.top;
            if (m.left < ef.highlight.left) m.left = ef.highlight.left;
            if (m.right < ef.highlight.right) m.right = ef.highlight.right;
            if (m.bottom < ef.highlight.bottom) m.bottom = ef.highlight.bottom;
            break;
        case shadow:
            qreal length = ef.shadow.length;
            double rads = lightDirection * (3.14159 / 180);
            qreal dx = -sin(rads) * length;
            qreal dy = +cos(rads) * length;
//            if (offset.x() < dx) offset.setX(dx);
//            if (offset.y() < dy) offset.setY(dy);
//            if (m.top < ef.shadow.blurRadius) m.top = ef.shadow.blurRadius;
//            if (m.left < ef.shadow.blurRadius) m.left = ef.shadow.blurRadius;
            if (m.right < ef.shadow.blurRadius) m.right = ef.shadow.blurRadius;
            if (m.bottom < ef.shadow.blurRadius) m.bottom = ef.shadow.blurRadius;
            updateBoundingRect();
        }
    }
}

void GraphicsEffect::draw(QPainter* painter)
{
    if (effects->length() == 0) return;
//    qDebug() << __FUNCTION__ << "effects->length() =" << effects->length();

    painter->save();

    QPoint srcOffset;
    PixmapPadMode mode = PadToEffectiveBoundingRect;
    srcPixmap = sourcePixmap(Qt::DeviceCoordinates, &srcOffset, mode);
    overlay = srcPixmap.toImage();
//  overlay.setDevicePixelRatio(srcPixmap.devicePixelRatioF());
//    srcPixmap.save("D:/Pictures/Temp/effect/srcPixmap.tif");

    // iterate effects in style
    using namespace winnow_effects;
    /*
    std::sort(effects.begin(), effects.end(), [](Effect const &l, Effect const &r) {
              return l.effectOrder < r.effectOrder; });
//              */
    for (int i = 0; i < effects->length(); ++i) {
        const Effect &ef = effects->at(i);
        QColor color;
        QPointF pt;
        Margin margin;
        /*
        qDebug() << __FUNCTION__
                 << "ef.effectOrder =" << ef.effectOrder
                 << "ef.effectName =" << ef.effectName
                    ;
//                    */
        switch (ef.effectType) {
        case blur:
            blurEffect(ef.blur.radius, ef.blur.blendMode);
            break;
        case sharpen:
            sharpenEffect(ef.sharpen.radius, ef.sharpen.blendMode);
            break;
        case highlight:
            color.setRgb(ef.highlight.r, ef.highlight.g, ef.highlight.b, ef.highlight.a);
            margin.top = ef.highlight.top;
            margin.left = ef.highlight.left;
            margin.right = ef.highlight.right;
            margin.bottom = ef.highlight.bottom;
            highlightEffect(color, margin, ef.highlight.blendMode);
            break;
        case shadow:
            color.setRgb(ef.shadow.r, ef.shadow.g, ef.shadow.b, ef.shadow.a);
            shadowEffect(ef.shadow.length, ef.shadow.blurRadius, color, ef.shadow.blendMode);
            break;
        case emboss:
            qDebug() << __FUNCTION__ << "ef.emboss.contour =" << ef.emboss.contour;
            embossEffect(ef.emboss.size, ef.emboss.highlight, ef.emboss.shadow,
                         ef.emboss.contour, ef.emboss.white, ef.emboss.black, ef.emboss.shade,
                         ef.emboss.soften, ef.emboss.opacity, ef.emboss.blendMode);
            break;
        case brighten:
            brightenEffect(ef.brighten.evDelta, ef.brighten.blendMode);
            break;
        }
    }

    // world transform required for object rotation
    painter->setWorldTransform(QTransform());

    if (rotation != 0.0) {
        // Rotate the overlay
        double ovrX = overlay.width() / 2;
        double ovrY = overlay.height() / 2;
        double ovrZ = sqrt(ovrX * ovrX + ovrY * ovrY); // radius of circle large enough to hold rotated overlay
        int side = static_cast<int>(ovrZ * 2);

        // image to hold rotated overlay
        QImage rotatedOverlay(QSize(side,side), QImage::Format_ARGB32_Premultiplied);
        rotatedOverlay.fill(Qt::transparent);
        QPainter rotatedOverlayPainter(&rotatedOverlay);
        rotatedOverlayPainter.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
        // translate origin to center of rotatedOverlayImage
        rotatedOverlayPainter.translate(ovrZ, ovrZ);
        // rotate axis
        rotatedOverlayPainter.rotate(rotation);
        // draw overlay with center offset to translated origin
        rotatedOverlayPainter.drawImage(QPointF(-ovrX, -ovrY), overlay);
        rotatedOverlayPainter.end();

        // delta offset based on offset to center from top left corner
        int srcCtrX = srcPixmap.width()/2;
        int srcCtrY = srcPixmap.height()/2;
        int ovrCtrX = rotatedOverlay.width()/2;
        int ovrCtrY = rotatedOverlay.height()/2;
        int dx = srcCtrX - ovrCtrX;
        int dy = srcCtrY - ovrCtrY;
        QPoint deltaOffset(dx, dy);

        painter->drawImage(srcOffset + deltaOffset, rotatedOverlay);
    }
    else {
//        overlay.save("D:/Pictures/Temp/effect/overlay.tif");
        painter->drawImage(srcOffset, overlay);
    }

    painter->restore();
}

QT_BEGIN_NAMESPACE
  extern Q_WIDGETS_EXPORT void qt_blurImage(QPainter *p, QImage &blurImage, qreal radius, bool quality, bool alphaOnly, int transposed = 0 );
QT_END_NAMESPACE

void GraphicsEffect::blurEffect(qreal radius, QPainter::CompositionMode mode)
{
    qDebug() << __FUNCTION__;

    QImage blurred(overlay.size(), QImage::Format_ARGB32_Premultiplied);
    blurred = overlay;
    Effects effect;
    effect.blur(blurred, radius);

//    effect.raise(blurred, radius);
//    effect.brighten(blurred, static_cast<int>(radius) * 4);
//    QImage blurred = WinnowGraphicEffect::blur(overlay, radius);

    blurred.save("D:/Pictures/Temp/effect/blurred.tif");

    QPainter overlayPainter(&overlay);
    overlayPainter.setCompositionMode(mode);
    overlayPainter.drawImage(0,0, blurred);
    overlayPainter.end();
}

void GraphicsEffect::sharpenEffect(qreal radius, QPainter::CompositionMode mode)
{
    qDebug() << __FUNCTION__;

    if (overlay.isNull()) return;

    QImage sharpened = WinnowGraphicEffect::sharpen(overlay, radius);
    sharpened.save("D:/Pictures/Temp/effect/sharpened.tif");

    QPainter overlayPainter(&overlay);
    overlayPainter.setCompositionMode(mode);
    overlayPainter.drawImage(0,0, sharpened);
    overlayPainter.end();
}

void GraphicsEffect::shadowEffect(double length, double radius, QColor color,
                                  QPainter::CompositionMode mode)
{
    qDebug() << __FUNCTION__;

    if (overlay.isNull()) return;

    double rads = lightDirection * (3.14159 / 180);
    QPointF shadowOffset;
    shadowOffset.setX(-sin(rads) * length);
    shadowOffset.setY(+cos(rads) * length);

    // create a transparent image with room for the shadow
    QImage shadIm(overlay.size(), QImage::Format_ARGB32_Premultiplied);
    shadIm.fill(Qt::transparent);
    QPainter shadPainter(&shadIm);
    shadPainter.drawImage(0,0, overlay);
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
    QPainter overlayPainter(&overlay);
    overlayPainter.setCompositionMode(mode);
    overlayPainter.drawImage(shadowOffset, shadIm);
    overlayPainter.end();
    return;
}

void GraphicsEffect::highlightEffect(QColor color, Margin margin, QPainter::CompositionMode mode)
{
    qDebug() << __FUNCTION__;

    if (overlay.isNull()) return;

    QPointF highlightOffset(margin.left, margin.top);
    int w = overlay.size().width() + margin.left + margin.right;
    int h = overlay.size().height() + margin.top + margin.bottom;
    QSize highlightBackgroundSize(w, h);
    QImage highlightBackgroundImage(highlightBackgroundSize, QImage::Format_ARGB32_Premultiplied);
    highlightBackgroundImage.fill(color);
    /*
    qDebug() << __FUNCTION__
             << "Margin.top =" << margin.top
             << "Margin.left =" << margin.left
             << "Margin.right =" << margin.right
             << "Margin.bottom =" << margin.bottom
             << "highlightBackgroundSize =" << highlightBackgroundSize;
//    */
    QPainter overlayPainter(&overlay);
    overlayPainter.translate(-highlightOffset);
    overlayPainter.setCompositionMode(mode);
    overlayPainter.drawImage(0, 0, highlightBackgroundImage);
    overlayPainter.end();

    return;
}

void GraphicsEffect::raiseEffect(int margin, QPainter::CompositionMode mode)
{
    qDebug() << __FUNCTION__;
    QImage temp(overlay.size(), QImage::Format_ARGB32_Premultiplied);
    temp = overlay;
    effect.raise(temp, margin, 0.2, false);

    QPainter overlayPainter(&overlay);
    overlayPainter.setCompositionMode(mode);
    overlayPainter.drawImage(0,0, temp);
    overlayPainter.end();
}

void GraphicsEffect::brightenEffect(qreal evDelta, QPainter::CompositionMode mode)
{
    qDebug() << __FUNCTION__;

    if (overlay.isNull()) return;

    QImage temp(overlay.size(), QImage::Format_ARGB32_Premultiplied);
    temp = overlay;
    Effects effect;
    effect.brighten(temp, evDelta);
//    effect.emboss(temp, evDelta, 0, 0, 1, -1, lightDirection);
//    temp.save("D:/Pictures/Temp/effect/brightened.tif");

    QPainter overlayPainter(&overlay);
    overlayPainter.setCompositionMode(mode);
    overlayPainter.drawImage(0,0, temp);
    overlayPainter.end();
}

void GraphicsEffect::embossEffect(double size, double highlight, double shadow,
                                  int contour, double white, double black, bool shade,
                                  double soften, int opacity, QPainter::CompositionMode mode)
{
    if (overlay.isNull()) return;

    QImage temp(overlay.size(), QImage::Format_ARGB32_Premultiplied);
    temp = overlay;
    Effects effect;
    effect.emboss(temp, lightDirection, size, highlight, shadow, contour, white, black, soften, shade);
//    temp.save("D:/Pictures/Temp/effect/embossed.tif");

    QPainter overlayPainter(&overlay);
    // any mode other than source causes a crash.  Don't know why
    overlayPainter.setCompositionMode(mode);
    overlayPainter.drawImage(0,0, temp);
    overlayPainter.end();
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
