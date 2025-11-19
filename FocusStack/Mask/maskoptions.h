#ifndef MASKOPTIONS_H
#define MASKOPTIONS_H

#include <QColor>

struct MaskAssessOptions
{
    bool grayscaleBase = false;
    float maskOpacity = 0.6f;      // 0..1
    QColor maskColor = QColor(255, 0, 0, 255);
    bool drawBoundary = true;
};

#endif // MASKOPTIONS_H
