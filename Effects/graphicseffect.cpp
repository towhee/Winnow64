#include "graphicseffect.h"
#include "REF_imageblitz_effects.h"

GraphicsEffect::GraphicsEffect(QString src, QObject */*parent*/)
{
//    qDebug() << "GraphicsEffect::GraphicsEffect";
    this->objectName() = "GraphicsEffect";
    this->src = src;
}

QRectF GraphicsEffect::boundingRectFor(const QRectF& rect) const
{
    /*
    qDebug() << "GraphicsEffect::boundingRectFor"
             << "rect =" << rect
             << "offset =" << offset
             << "m.top =" << m.top
             << "m.left =" << m.left
             << "m.right =" << m.right
             << "m.bottom =" << m.bottom
             << "new =" << rect.united(rect.translated(offset).adjusted(-m.left, -m.top, m.right, m.bottom))
                ;
//                        */
    return rect.united(rect.translated(offset).adjusted(-m.left, -m.top, m.right, m.bottom));
}

void GraphicsEffect::set(QList<winnow_effects::Effect> &effects,
                         int globalLightDirection,
                         double rotation,
                         QRectF boundRect)
{
/*
       padding, shift, rotation, ligt direction
*/

    this->effects = &effects;
    lightDirection = globalLightDirection;
    this->rotation = rotation;
    srcRectZeroRotation = boundRect;

    /*
    // reset bounding rect
    offset.setX(0);
    offset.setY(0);
    m.top = 0;
    m.left = 0;
    m.right = 0;
    m.bottom = 0;
    updateBoundingRect();
//    */

    // iterate effects in style to set boundingRect
    using namespace winnow_effects;
    /*
    std::sort(effects.begin(), effects.end(), [](Effect const &l, Effect const &r) {
              return l.effectOrder < r.effectOrder; });
//              */
    for (int i = 0; i < effects.length(); ++i) {
        const Effect &ef = effects.at(i);
        /*
        qDebug() << "GraphicsEffect::set"
                 << "ef.effectOrder =" << ef.effectOrder
                 << "ef.effectName =" << ef.effectName
                    ;
//                    */
        switch (ef.effectType) {
            case emboss:
            case sharpen:
            case brightness:
            case stroke:
            case glow:
                break;
            case highlighter: {
                if (m.top < ef.highlighter.top) m.top = ef.highlighter.top;
                if (m.left < ef.highlighter.left) m.left = ef.highlighter.left;
                if (m.right < ef.highlighter.right) m.right = ef.highlighter.right;
                if (m.bottom < ef.highlighter.bottom) m.bottom = ef.highlighter.bottom;
                shift.x += -m.left;
                shift.y += -m.top;
                break;
            }
            case blur: {
//                int r = static_cast<int>(ef.blur.radius);
//                if (ef.blur.outer) {
//                    if (m.top < r) m.top = r;
//                    if (m.left < r) m.left = r;
//                    if (m.right < r) m.right = r;
//                    if (m.bottom < r) m.bottom = r;
//                }
                updateBoundingRect();
                break;
            }
            case shadow: {
                // shadow offset
                qreal length = ef.shadow.length;
                double rads = lightDirection * (3.14159 / 180);
                int dx = qRound(-sin(rads) * length);
                int dy = qRound(+cos(rads) * length);
//                int dx = static_cast<int>(-sin(rads) * length);
//                int dy = static_cast<int>(+cos(rads) * length);
                offset.setX(dx);
                offset.setY(dy);
                // blur expansion
                int r = static_cast<int>(ef.shadow.blurRadius);
                /*
                int dt, dl, dr, db;  // dt = delta top etc
                dt = dl = dr = db = 0;
                if (dx > 0) {
                    dr = r;
                    dl = 0;
                }
                else {
                    dl = r;
                    dr = 0;
                }
                if (dy > 0) {
                    db = r;
                    dt = 0;
                }
                else {
                    dt = r;
                    db = 0;
                }
                // expand for greater of shadow length or shadow blur
                if (m.top < dt) m.top = dt;
                if (m.left < dl) m.left = dl;
                if (m.right < dr) m.right = dr;
                if (m.bottom < db) m.bottom = db;

                // or...  */
                m.top = r;
                m.left = r;
                m.right = r;
                m.bottom = r;

                /*
                qDebug() << "GraphicsEffect::set"
                         << "dx =" << dx
                         << "dy =" << dy
                         << "r =" << r
                         << "m.top =" << m.top
                         << "m.left =" << m.left
                         << "m.right =" << m.right
                         << "m.bottom =" << m.bottom
                            ;
    //                        */
                updateBoundingRect();
                break;
            }
        }
    }
}

/* void GraphicsEffect::sourceChanged(QGraphicsEffect::ChangeFlags flags)
{
    qDebug() << "GraphicsEffect::sourceChanged" << flags;
    if (flags & QGraphicsEffect::SourceInvalidated ||
        flags & QGraphicsEffect::SourceBoundingRectChanged)
        okToDraw = true;
    else okToDraw = false;
} */

void GraphicsEffect::draw(QPainter* painter)
{
#ifdef ISLOGGER
Utilities::log("GraphicsEffect::draw", "");
#endif
    /*
    qDebug() << "GraphicsEffect::draw" << okToDraw << QTime::currentTime() << painter;
    if (!okToDraw) return;
    okToDraw = false;

    qDebug() << "GraphicsEffect::draw" << "effects->length() =" << effects->length();

    if (effects->length() == 0) return;
    */
    if (this->sourcePixmap().isNull()) return;

    painter->save();

    QPoint srcOffset;
    /*
    PixmapPadMode mode = PadToEffectiveBoundingRect;
    */
    // unpadded image is req'd for some effects like emboss
    unpaddedSrcImage = sourcePixmap(Qt::DeviceCoordinates, &srcOffset, NoPad).toImage();
    srcPixmap = sourcePixmap(Qt::DeviceCoordinates, &srcOffset, PadToEffectiveBoundingRect);
//    qDebug() << "GraphicsEffect::draw" << "srcPixmap.width() =" << srcPixmap.width();
    overlay = srcPixmap.toImage();
    /*
    qDebug() << "GraphicsEffect::draw"
             << "boundingRect =" << boundingRect()
             << "boundingRect.x() =" << boundingRect().x()
             << "overlay =" << overlay.rect()
                ;
//                */

    // iterate effects in style
    using namespace winnow_effects;
    /*
    std::sort(effects.begin(), effects.end(), [](Effect const &l, Effect const &r) {
              return l.effectOrder < r.effectOrder; });
//              */

    // Winnow shows images at real scale so render effects using real device pixels
    if (src == "Internal") overlay.setDevicePixelRatio(1.0);

    for (int i = 0; i < effects->length(); ++i) {
        const Effect &ef = effects->at(i);
        QColor color;
        QPointF pt;
        Margin margin;
        /*
        qDebug() << "GraphicsEffect::draw"
                 << "i =" << i
                 << "ef.effectOrder =" << ef.effectOrder
                 << "ef.effectName =" << ef.effectName
                 << "ef.effectType =" << ef.effectType
                    ;
//                    */
        switch (ef.effectType) {
        case blur:
            blurEffect(ef.blur.radius, ef.blur.blendMode);
            break;
        case sharpen:
            sharpenEffect(ef.sharpen.radius, ef.sharpen.blendMode);
            break;
        case stroke:
            color.setRgb(ef.stroke.r, ef.stroke.g, ef.stroke.b, ef.stroke.a);
            strokeEffect(ef.stroke.width, color, ef.stroke.opacity, ef.stroke.blendMode);
            break;
        case glow:
            color.setRgb(ef.glow.r, ef.glow.g, ef.glow.b, ef.glow.a);
            glowEffect(ef.glow.width, color, ef.glow.blurRadius, ef.glow.blendMode);
            break;
        case highlighter:
            color.setRgb(ef.highlighter.r, ef.highlighter.g, ef.highlighter.b, ef.highlighter.a);
            margin.top = ef.highlighter.top;
            margin.left = ef.highlighter.left;
            margin.right = ef.highlighter.right;
            margin.bottom = ef.highlighter.bottom;
            highligherEffect(color, margin, ef.highlighter.blendMode);
            break;
        case shadow:
            color.setRgb(ef.shadow.r, ef.shadow.g, ef.shadow.b, ef.shadow.a);
            shadowEffect(ef.shadow.length, ef.shadow.blurRadius, color,
                         ef.shadow.opacity, ef.shadow.blendMode);
            break;
        case emboss:
            embossEffect(ef.emboss.size, ef.emboss.exposure, ef.emboss.umbra,
                         ef.emboss.inflection, ef.emboss.start, ef.emboss.mid, ef.emboss.end,
                         ef.emboss.isUmbraGradient, ef.emboss.contrast, ef.emboss.blendMode);
            break;
        case brightness:
            brightnessEffect(ef.brightness.evDelta, ef.brightness.blendMode);
            break;
        }
    }

    // Go back to device pixel ratio
    if (src == "Internal") overlay.setDevicePixelRatio(G::sysDevicePixelRatio);

    // world transform required for object rotation
    painter->setWorldTransform(QTransform());

    painter->drawImage(srcOffset, overlay);
    painter->restore();
    return;
    /*
    if (rotation != 0.0) {
        qDebug() << "GraphicsEffect::draw" << "rotation =" << rotation;
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
    */
}

QT_BEGIN_NAMESPACE
  extern Q_WIDGETS_EXPORT void qt_blurImage(QPainter *p, QImage &blurImage, qreal radius, bool quality, bool alphaOnly, int transposed = 0 );
QT_END_NAMESPACE

void GraphicsEffect::blurEffect(qreal radius, QPainter::CompositionMode mode)
{
#ifdef ISLOGGER
Utilities::log("GraphicsEffect::blurEffect", "");
#endif
//    qDebug() << "GraphicsEffect::blurEffect" << QTime::currentTime();

    QImage temp(overlay.size(), QImage::Format_ARGB32_Premultiplied);
    temp = overlay;
    Effects effect;
    effect.boxBlur(temp, static_cast<int>(radius));
//    effect.boxBlur2D(temp, static_cast<int>(radius));

//    effect.boxBlur1(temp, static_cast<int>(radius));
//    effect.blur(temp, static_cast<int>(radius));
//    effect.blurOriginal(temp, static_cast<int>(radius));

//    temp.save("D:/Pictures/Temp/effect/blurred.tif");

    QPainter overlayPainter(&overlay);
    overlayPainter.setCompositionMode(mode);
    // qt blur
//    qt_blurImage(&overlayPainter, temp, radius, true, false);
    overlayPainter.drawImage(0,0, temp);
    overlayPainter.end();
}

void GraphicsEffect::sharpenEffect(qreal radius, QPainter::CompositionMode mode)
{
#ifdef ISLOGGER
Utilities::log("GraphicsEffect::sharpenEffect", "");
#endif
    qDebug() << "GraphicsEffect::sharpenEffect" << QTime::currentTime();

    if (overlay.isNull()) return;

    QImage sharpened = WinnowGraphicEffect::sharpen(overlay, radius);
//    sharpened.save("D:/Pictures/Temp/effect/sharpened.tif");

    QPainter overlayPainter(&overlay);
    overlayPainter.setCompositionMode(mode);
    overlayPainter.drawImage(0,0, sharpened);
    overlayPainter.end();
}

void GraphicsEffect::shadowEffect(double length, double radius, QColor color, double opacity,
                                  QPainter::CompositionMode mode)
{
#ifdef ISLOGGER
Utilities::log("GraphicsEffect::shadowEffect", "");
#endif
//    qDebug() << "GraphicsEffect::shadowEffect" << QTime::currentTime();

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

//    QImage shadIm = srcPixmap.toImage();

    // blur the alpha channel
    QImage blurred(shadIm.size(), QImage::Format_ARGB32_Premultiplied);
    blurred.fill(Qt::transparent);
    QPainter blurPainter(&blurred);
    qt_blurImage(&blurPainter, shadIm, radius, false, true);
    blurPainter.end();

    shadIm = std::move(blurred);

    // apply color to shadow

//    QPainter shadPainter(&shadIm);

    shadPainter.begin(&shadIm);
    shadPainter.setOpacity(opacity);
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

void GraphicsEffect::shadowEffect1(double length, double radius, QColor color, double opacity,
                                  QPainter::CompositionMode mode)
{
#ifdef ISLOGGER
Utilities::log("GraphicsEffect::shadowEffect1", "");
#endif
    /*
    qDebug() << "GraphicsEffect:shadowEffect1:" << QTime::currentTime();
    qDebug() << "GraphicsEffect::shadowEffect1"
             << "overlay.rect() =" << overlay.rect()
                ;
//                */

    if (overlay.isNull()) return;

//    overlay.save("D:/Pictures/Temp/effect/o1.tif");
    double rads = lightDirection * (3.14159 / 180);
    int dx = qRound(-sin(rads) * length);
    int dy = qRound(+cos(rads) * length);
//    int x = boundingRect().x();
//    int y = boundingRect().y();
//    int w = boundingRect().width();
//    int h = boundingRect().height();
    /*
    qDebug() << "GraphicsEffect::shadowEffect1"
             << "unpaddedSrcImage.rect() =" << unpaddedSrcImage.rect()
             << "overlay.rect() =" << overlay.rect()
             << "boundingRect.rect() =" << boundingRect()
                ;
//              */

    // Create the shadow image offset by the shadow length
    QImage shadIm(overlay.size(), QImage::Format_ARGB32_Premultiplied);
//    shadIm.fill(Qt::transparent);
    QPainter shadPainter(&shadIm);
    shadPainter.drawImage(dx, dy, overlay);
    shadPainter.end();

    // Make a blurred copy of the offset overlay
    QImage blurred(overlay.size(), QImage::Format_ARGB32);
//    blurred.fill(Qt::transparent);
    QPainter blurPainter(&blurred);
    qt_blurImage(&blurPainter, shadIm, radius, false, true);
  /*
    QRegion all(0, 0, overlay.width(), overlay.height());
    QRegion blur(dx, dy, overlay.width(), overlay.height());
    QRegion topLeft = all.subtracted(blur);
    blurPainter.setClipRegion(topLeft);
    blurPainter.fillRect(0, 0, overlay.width(), overlay.height(), Qt::transparent);
//  */
    blurPainter.end();

//    blurred.save("D:/Pictures/Temp/effect/blurred.tif");
    // move the shadow back to the shadow image
    shadIm = std::move(blurred);

    // apply color to shadow
    shadPainter.begin(&shadIm);
    shadPainter.setOpacity(opacity);
    shadPainter.setCompositionMode(QPainter::CompositionMode_SourceIn);
    shadPainter.fillRect(shadIm.rect(), color);
   /*  erase blur except revealed shadow
    qDebug() << "GraphicsEffect::shadowEffect1"
             << "x =" << x
             << "y =" << y
             << "dy =" << dy
             << "dy =" << dy
             << "radius =" << radius
             << "overlay =" << overlay.rect()
                ;
    int w = shadIm.width();
    int h = shadIm.height();
    QRect top(0, 0, w, -y);
    QRect left(0, 0, -x, h);
    QRect right(w + x, 0, -x, h);
    QRect bottom(0, h + y, w, -y);
    shadPainter.setOpacity(1.0);
    shadPainter.setCompositionMode(QPainter::CompositionMode_Source);
    shadPainter.fillRect(top, Qt::green);
    shadPainter.fillRect(left, Qt::darkBlue);
    shadPainter.fillRect(right, Qt::darkCyan);
    shadPainter.fillRect(bottom, Qt::darkMagenta);
//  */
    shadPainter.end();
//    shadIm.save("D:/Pictures/Temp/effect/shadIm.tif");

    // compose adjusted image
    QPainter overlayPainter(&overlay);
    overlayPainter.setCompositionMode(mode);
    overlayPainter.drawImage(dx, dy, shadIm);
    overlayPainter.end();
}

void GraphicsEffect::highligherEffect(QColor color, Margin margin, QPainter::CompositionMode mode)
{
//    qDebug() << "GraphicsEffect::highligherEffect" /*<< QTime::currentTime()*/
//             << "boundingRect =" << boundingRect();

    if (overlay.isNull()) return;
//    overlay.save("D:/Pictures/Temp/effect/o3.tif");

    QPointF highlighterOffset(margin.left, margin.top);
    int w = overlay.size().width() + margin.left + margin.right;
    int h = overlay.size().height() + margin.top + margin.bottom;
    QSize highlighterBackgroundSize(w, h);
    QImage highlighterBackgroundImage(highlighterBackgroundSize, QImage::Format_ARGB32_Premultiplied);
    highlighterBackgroundImage.fill(color);
    /*
    qDebug() << "GraphicsEffect::highligherEffect"
             << "Margin.top =" << margin.top
             << "Margin.left =" << margin.left
             << "Margin.right =" << margin.right
             << "Margin.bottom =" << margin.bottom
             << "highlightBackgroundSize =" << highlightBackgroundSize;
//    */

//    overlay.save("D:/Pictures/Temp/effect/_stroked1.tif");
//    highlighterBackgroundImage.save("D:/Pictures/Temp/effect/hl.tif");

    QPainter overlayPainter(&overlay);
    overlayPainter.translate(-highlighterOffset);
    overlayPainter.setCompositionMode(mode);
    overlayPainter.drawImage(0, 0, highlighterBackgroundImage);
    overlayPainter.end();
//    overlay.save("D:/Pictures/Temp/effect/o4.tif");
    return;
}

void GraphicsEffect::raiseEffect(int margin, QPainter::CompositionMode mode)
{
//    qDebug() << "GraphicsEffect::raiseEffect" << QTime::currentTime();
#ifdef ISLOGGER
Utilities::log("GraphicsEffect::raiseEffect", "");
#endif
    QImage temp(overlay.size(), QImage::Format_ARGB32_Premultiplied);
    temp = overlay;
    effect.raise(temp, margin, 0.2, false);

    QPainter overlayPainter(&overlay);
    overlayPainter.setCompositionMode(mode);
    overlayPainter.drawImage(0,0, temp);
    overlayPainter.end();
}

void GraphicsEffect::brightnessEffect(qreal evDelta, QPainter::CompositionMode mode)
{
#ifdef ISLOGGER
Utilities::log("GraphicsEffect::brightnessEffect", "");
#endif
//    qDebug() << "GraphicsEffect::brightnessEffect" << QTime::currentTime();
    if (overlay.isNull()) return;

//    QImage temp(overlay.size(), QImage::Format_ARGB32/*_Premultiplied*/);
//    temp = overlay;
    QImage temp(overlay.size(), QImage::Format_ARGB32);
    QPainter tempPainter(&temp);
    // transparency not working unless add overlay in a painter
    tempPainter.drawImage(0, 0, overlay);
    tempPainter.end();
    Effects effect;
    effect.brightness(temp, evDelta);

    QPainter overlayPainter(&overlay);
    overlayPainter.setCompositionMode(mode);
    overlayPainter.drawImage(0,0, temp);
    overlayPainter.end();
}

void GraphicsEffect::strokeEffect(double width,
                                  QColor color,
                                  double opacity,
                                  QPainter::CompositionMode mode)
{
#ifdef ISLOGGER
Utilities::log("GraphicsEffect::strokeEffect", "");
#endif
//    qDebug() << "GraphicsEffect::strokeEffect" /*<< QTime::currentTime()*/
//             << "boundingRect =" << boundingRect();

    if (overlay.isNull() || width < 1) return;

    QImage temp(overlay.size(), QImage::Format_ARGB32);
    QPainter tempPainter(&temp);
    // transparency not working unless add overlay in a painter
    tempPainter.drawImage(0, 0, overlay);
    tempPainter.end();
    Effects effect;
    if (effect.stroke(temp, width, color, opacity, true)) {
//        temp.save("D:/Pictures/Temp/effect/stroke.tif");
        QPainter overlayPainter(&overlay);
        overlayPainter.setCompositionMode(mode);
        overlayPainter.drawImage(0, 0, temp);
        overlayPainter.end();
//        overlay.save("D:/Pictures/Temp/effect/o2.tif");
    }
}

void GraphicsEffect::glowEffect(double width, QColor color, double blurRadius, QPainter::CompositionMode mode)
{
#ifdef ISLOGGER
Utilities::log("GraphicsEffect::glowEffect", "");
#endif
//    qDebug() << "GraphicsEffect::glowEffect" << QTime::currentTime();
    if (overlay.isNull()) return;

    // silence issue
    blurRadius = 1;

    QImage temp(overlay.size(), QImage::Format_ARGB32_Premultiplied);
    temp = overlay;
    Effects effect;
    QImage edgeMap;
    effect.stroke(temp, width, color, 0.5, true);
//    temp.save("D:/Pictures/Temp/effect/_transparentEdgeMap.tif");



    QPainter overlayPainter(&overlay);
    overlayPainter.setCompositionMode(mode);
    overlayPainter.drawImage(0,0, temp);
    overlayPainter.end();
}

void GraphicsEffect::embossEffect(double size,
                                  double exposure,
                                  double umbra,
                                  double inflection,
                                  double startEV,
                                  double midEV,
                                  double endEV,
                                  bool isUmbraGradient,
                                  double contrast,
                                  QPainter::CompositionMode mode)
{
#ifdef ISLOGGER
Utilities::log("GraphicsEffect::embossEffect", "");
#endif
//    qDebug() << "GraphicsEffect::embossEffect" << QTime::currentTime();
    if (overlay.isNull()) return;

    QImage temp(overlay.size(), QImage::Format_ARGB32);
    QPainter tempPainter(&temp);
    // transparency not working unless add overlay in a painter
    tempPainter.drawImage(0, 0, overlay);
    tempPainter.end();
    Effects effect;
    effect.emboss(temp, lightDirection, size, exposure, contrast,
                  inflection, startEV, midEV, endEV,
                  umbra, isUmbraGradient);

//    temp.save("D:/Pictures/Temp/effect/embossed1.tif");

    QPainter overlayPainter(&overlay);
    // any mode other than source causes a crash.  Don't know why
    overlayPainter.setCompositionMode(mode);
    overlayPainter.drawImage(0,0, temp);
    overlayPainter.end();
//    overlay.save("D:/Pictures/Temp/effect/overlay.tif");
}

/* bool GraphicsEffect::eventFilter(QObject *obj, QEvent *event)
{
    qDebug() << "GraphicsEffect::embossEffect"
             << event << "\t"
             << event->type() << "\t"
             << obj << "\t"
             << obj->objectName();
    return QGraphicsEffect::eventFilter(obj, event);
} */

bool GraphicsEffect::event(QEvent *event)
{
    qDebug() << "GraphicsEffect::event"
             << event << "\t"
             << event->type()
                ;
    return  QGraphicsEffect::event(event);
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
