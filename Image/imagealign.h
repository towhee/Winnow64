#ifndef IMAGEALIGN_H
#define IMAGEALIGN_H

#include <QObject>
#include <QImage>
#include <QColor>
#include "testaligndlg.h"

class ImageAlign : public QObject
{
    Q_OBJECT
public:
    explicit ImageAlign(int thumbSize, qreal targetPct, QObject *parent = nullptr);
    QPointF alignImage(QImage *baseImage, QImage *image, QPointF &basePosPct);

signals:

public slots:

private:
    QImage *target;
    QImage *baseThumb;
    QImage *alignThumb;
    QPoint basePos;
    int thumbSize;             // long dimension number of pixels
    qreal targetPct;           // percent of long dimension made into a square
    int targetSize;
};

#endif // IMAGEALIGN_H
