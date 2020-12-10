#ifndef GRAPHICSEFFECT_H
#define GRAPHICSEFFECT_H

#include <QtWidgets>
#include <QGraphicsEffect>
#include "Main/global.h"
#include "effects.h"

namespace winnow_effects
{
    struct Shadow {
        qreal length;               // % of object long side or text height
        qreal blurRadius;
        int r, g, b, a;
        QPainter::CompositionMode blendMode;
    };

    struct Blur {
        qreal radius;
        QPainter::CompositionMode blendMode;
    };

    struct Stroke {
        qreal width;
        int r, g, b, a;
        bool inner;
        QPainter::CompositionMode blendMode;
    };

    struct Glow {
        qreal width;
        int r, g, b, a;
        int blurRadius;
        QPainter::CompositionMode blendMode;
    };

    struct Sharpen {
        qreal radius;
        QPainter::CompositionMode blendMode;
    };

    struct Raise {
        int m;
        double taper;
        int blurWidth;
        bool isRaise;
        QPainter::CompositionMode blendMode;
    };

    struct Highlight {
        int r, g, b, a;
        int top, left, right, bottom;
        QPainter::CompositionMode blendMode;
    };

    struct Brighten {
        qreal evDelta;
        QPainter::CompositionMode blendMode;
    };

//    enum Contour {contour_flat, contour_convex_round};
    struct Emboss {
        double size;
        double exposure;
        double start;
        double mid;
        double end;
        double inflection;
        double umbra;
        bool isUmbraGradient;
        double contrast;
        QPainter::CompositionMode blendMode;
    };

    // when add an effect must add to enum and union
    enum EffectType {blur, sharpen, highlight, shadow, brighten, emboss, stroke, glow};
    struct Effect {
        enum EffectType effectType;
        QString effectName;         // unique: req'd in case more than one of same effect
        int effectOrder;            // order executed within style
        union {
            Blur blur;
            Sharpen sharpen;
            Highlight highlight;
            Shadow shadow;
            Brighten brighten;
            Emboss emboss;
            Stroke stroke;
            Glow glow;
        };
    };

    static QStringList blendModes (QStringList()
        << "Above"
        << "Below"
        << "Clear"
        << "Source"
        << "Destination"
        << "SourceIn"
        << "DestinationIn"
        << "SourceOut"
        << "DestinationOut"
        << "SourceAtop"
        << "DestinationAtop"
        << "Xor"
        << "Plus"
        << "Multiply"
        << "Screen"
        << "Overlay"
        << "Darken"
        << "Lighten"
        << "Color dodge"
        << "Color burn"
        << "Hard light"
        << "Soft light"
        << "Difference"
        << "Exclusion"
        );

    static QMap<QString, QPainter::CompositionMode> blendModeMap{
        {"Above", QPainter::CompositionMode_SourceOver},
        {"Below", QPainter::CompositionMode_DestinationOver},
        {"Clear", QPainter::CompositionMode_Clear},
        {"Source", QPainter::CompositionMode_Source},
        {"Destination", QPainter::CompositionMode_Destination},
        {"SourceIn", QPainter::CompositionMode_SourceIn},
        {"DestinationIn", QPainter::CompositionMode_DestinationIn},
        {"SourceOut", QPainter::CompositionMode_SourceOut},
        {"DestinationOut", QPainter::CompositionMode_DestinationOut},
        {"SourceAtop", QPainter::CompositionMode_SourceAtop},
        {"DestinationAtop", QPainter::CompositionMode_DestinationAtop},
        {"Xor", QPainter::CompositionMode_Xor},
        {"Plus", QPainter::CompositionMode_Plus},
        {"Multiply", QPainter::CompositionMode_Multiply},
        {"Screen", QPainter::CompositionMode_Screen},
        {"Overlay", QPainter::CompositionMode_Overlay},
        {"Darken", QPainter::CompositionMode_Darken},
        {"Lighten", QPainter::CompositionMode_Lighten},
        {"Color dodge", QPainter::CompositionMode_ColorDodge},
        {"Color burn", QPainter::CompositionMode_ColorBurn},
        {"Hard light", QPainter::CompositionMode_HardLight},
        {"Soft light", QPainter::CompositionMode_SoftLight},
        {"Difference", QPainter::CompositionMode_Difference},
        {"Exclusion", QPainter::CompositionMode_Exclusion}
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
    void shadowEffect(double size, double radius, QColor color, QPainter::CompositionMode mode);
    void highlightEffect(QColor color, Margin margin, QPainter::CompositionMode mode);
    void strokeEffect(double width, QColor color, QPainter::CompositionMode mode);
    void glowEffect(double width, QColor color, double blurRadius, QPainter::CompositionMode mode);
    void blurEffect(qreal radius, QPainter::CompositionMode mode);
    void brightenEffect(qreal delta, QPainter::CompositionMode mode);
    void sharpenEffect(qreal radius, QPainter::CompositionMode mode);
    void raiseEffect(int margin, QPainter::CompositionMode mode);
    void embossEffect(double size, double exposure, double umbra, double inflection,
                      double startEV, double midEV, double endEV, bool isUmbraGradient,
                      double soften, QPainter::CompositionMode mode);

    QList<winnow_effects::Effect> *effects;
    Effects effect;

    QRectF srcRectZeroRotation;
    QRectF srcRect;
    QImage unpaddedSrcImage;
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
