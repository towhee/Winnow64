#include "graphicseffect.h"
#include "REF_imageblitz_effects.h"

GraphicsEffect::GraphicsEffect(QString src, QObject */*parent*/)
{
//    qDebug() << "GraphicsEffect::GraphicsEffect" << "src =" << src;
    this->objectName() = "GraphicsEffect";
    this->src = src;
    useSnapshot = false;
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
    padding, shift, rotation, light direction
*/

    this->effects = &effects;
    lightDirection = globalLightDirection;
    this->rotation = rotation;
    srcRectZeroRotation = boundRect;

    drawCount = -1;
//    qDebug() << "GraphicsEffect::set  boundRect =" << boundRect;

    // iterate effects in style to set boundingRect
    using namespace winnow_effects;
    /*
    std::sort(effects.begin(), effects.end(), [](Effect const &l, Effect const &r) {
              return l.effectOrder < r.effectOrder; });
              //*/
    for (int i = 0; i < effects.length(); ++i) {
        //qDebug() << "GraphicsEffect::set" << i;
        const Effect &ef = effects.at(i);
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
                int r = static_cast<int>(ef.blur.radius);
                /*
                if (ef.blur.outer) {
                    if (m.top < r) m.top = r;
                    if (m.left < r) m.left = r;
                    if (m.right < r) m.right = r;
                    if (m.bottom < r) m.bottom = r;
                }
                updateBoundingRect();
                //*/
                break;
            }
            case shadow: {
                // shadow offset
                qreal length = ef.shadow.length;
                double rads = lightDirection * (3.14159 / 180);
                int dx = qRound(-sin(rads) * length);
                int dy = qRound(+cos(rads) * length);
                offset.setX(dx);
                offset.setY(dy);
                // blur expansion
                int r = static_cast<int>(ef.shadow.blurRadius);
                m.top = r;
                m.left = r;
                m.right = r;
                m.bottom = r;
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
if (G::isFileLogger) Utilities::log("GraphicsEffect::draw", "");
#endif

//    if (!okToDraw) return;
//    okToDraw = false;
//    if (drawCount) return;
    drawCount++;
    status = "DrawCount" + QString::number(drawCount);
//    if (drawCount == 0) painter->eraseRect(boundingRect());

/*
    qDebug() << "GraphicsEffect::draw" << okToDraw << QTime::currentTime() << painter;

    if (effects->length() == 0) return;
    */
    //qDebug() << "GraphicsEffect::draw" << "drawCount =" << drawCount;
    if (this->sourcePixmap().isNull()) return;

    painter->save();

    QPoint srcOffset;
    /*
    PixmapPadMode mode = PadToEffectiveBoundingRect;
    */
    // unpadded image is req'd for some effects like emboss
    unpaddedSrcImage = sourcePixmap(Qt::DeviceCoordinates, &srcOffset, NoPad).toImage();
    srcPixmap = sourcePixmap(Qt::DeviceCoordinates, &srcOffset, PadToEffectiveBoundingRect);
    overlay = QImage();
    overlay = srcPixmap.toImage();
    saveSnapshot("unpadsrc", unpaddedSrcImage);
    saveSnapshot("src", overlay);
    /*
    qDebug() << "GraphicsEffect::draw"
             << "boundingRect =" << boundingRect()
             << "boundingRect.x() =" << boundingRect().x()
             << "overlay =" << overlay.rect()
             << "srcPixmap =" << srcPixmap.rect()
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
    saveSnapshot("drawfinal", overlay);
    painter->restore();
    return;
}

QT_BEGIN_NAMESPACE
  extern Q_WIDGETS_EXPORT void qt_blurImage(QPainter *p, QImage &blurImage, qreal radius, bool quality, bool alphaOnly, int transposed = 0 );
QT_END_NAMESPACE

void GraphicsEffect::blurEffect(qreal radius, QPainter::CompositionMode mode)
{
#ifdef ISLOGGER
if (G::isFileLogger) Utilities::log("GraphicsEffect::blurEffect", "");
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
if (G::isFileLogger) Utilities::log("GraphicsEffect::sharpenEffect", "");
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
if (G::isFileLogger) Utilities::log("GraphicsEffect::shadowEffect", "");
#endif
/*
    qDebug() << "GraphicsEffect::shadowEffect"
             << "boundingRect =" << boundingRect()
             << "overlay.size() =" << overlay.size()
                ;
    //*/

    if (overlay.isNull()) return;

    // update bounding rect
    double rads = lightDirection * (3.14159 / 180);
    int dx = qRound(-sin(rads) * length);
    int dy = qRound(+cos(rads) * length);
    offset.setX(dx);
    offset.setY(dy);
    // blur expansion
    int r = static_cast<int>(radius);
    m.top = r;
    m.left = r;
    m.right = r;
    m.bottom = r;
//    updateBoundingRect();

//    double rads = lightDirection * (3.14159 / 180);
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
//    saveSnapshot("shadow2", overlay);

    return;
}

void GraphicsEffect::shadowEffect1(double length, double radius, QColor color, double opacity,
                                  QPainter::CompositionMode mode)
{
#ifdef ISLOGGER
if (G::isFileLogger) Utilities::log("GraphicsEffect::shadowEffect1", "");
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
if (G::isFileLogger) Utilities::log("GraphicsEffect::raiseEffect", "");
#endif
//    QImage temp(overlay.size(), QImage::Format_ARGB32_Premultiplied);
//    temp = overlay;
//    effect.raise(temp, margin, 0.2, false);

//    QPainter overlayPainter(&overlay);
//    overlayPainter.setCompositionMode(mode);
//    overlayPainter.drawImage(0,0, temp);
//    overlayPainter.end();
}

void GraphicsEffect::brightnessEffect(qreal evDelta, QPainter::CompositionMode mode)
{
#ifdef ISLOGGER
if (G::isFileLogger) Utilities::log("GraphicsEffect::brightnessEffect", "");
#endif
//    qDebug() << "GraphicsEffect::brightnessEffect" << QTime::currentTime();
    if (overlay.isNull()) return;

//    QImage temp(overlay.size(), QImage::Format_ARGB32/*_Premultiplied*/);
//    temp = overlay;
    QImage temp(overlay.size(), QImage::Format_ARGB32);
    temp.fill(Qt::transparent);
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
if (G::isFileLogger) Utilities::log("GraphicsEffect::strokeEffect", "");
#endif
    /*
    qDebug() << "GraphicsEffect::strokeEffect"
             << "boundingRect =" << boundingRect()
             << "overlay.size() =" << overlay.size()
                ;
    //*/

    if (overlay.isNull() || width < 1) return;

//    saveSnapshot("stroke0", overlay);
    QImage temp(overlay.size(), QImage::Format_ARGB32);
    temp.fill(Qt::transparent);
//    saveSnapshot("stroketemp", temp);
    QPainter tempPainter(&temp);
    // transparency not working unless add overlay in a painter
    tempPainter.drawImage(0, 0, overlay);
    tempPainter.end();
//    saveSnapshot("stroke1", temp);
    Effects effect;
    if (effect.stroke(temp, width, color, opacity, true)) {
//        saveSnapshot("stroke2", overlay);
        QPainter overlayPainter(&overlay);
        overlayPainter.setCompositionMode(mode);
        overlayPainter.drawImage(0, 0, temp);
        overlayPainter.end();
//        saveSnapshot("stroke3", overlay);
    }
}

void GraphicsEffect::glowEffect(double width, QColor color, double blurRadius, QPainter::CompositionMode mode)
{
#ifdef ISLOGGER
if (G::isFileLogger) Utilities::log("GraphicsEffect::glowEffect", "");
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
if (G::isFileLogger) Utilities::log("GraphicsEffect::embossEffect", "");
#endif
//    qDebug() << "GraphicsEffect::embossEffect" << QTime::currentTime();
    if (overlay.isNull()) return;

    QImage temp(overlay.size(), QImage::Format_ARGB32);
    temp.fill(Qt::transparent);
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

void GraphicsEffect::saveSnapshot(QString name, QImage image)
{
    if (!useSnapshot) return;
    QString s = path + QString::number(order++).rightJustified(3, '0') +
                status + "_" + name + ".png";
    image.save(s);
}
