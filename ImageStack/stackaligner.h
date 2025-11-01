#ifndef STACKALIGNER_H
#define STACKALIGNER_H

#include <QObject>
#include <QImage>
#include <QMap>
#include <QPoint>
#include <QVector>
#include <QtMath>
#include <QPainter>
#include <QTransform>
#include "main/global.h"

class StackAligner : public QObject
{
    Q_OBJECT
public:
    explicit StackAligner(QObject *parent = nullptr);

    // Align all images in stack to first image (key order respected)
    QMap<int, QImage> align(const QMap<int, QImage> &stack);

    // Configuration
    void setSearchRadius(int px);
    void setRotationStep(float deg);
    void setDownsample(int factor);
    void setUseEdgeMaskWeighting(bool enabled);

signals:
    void progress(QString msg, int current, int total);
    void updateStatus(bool keepBase, QString msg, QString src);
private:
    int searchRadius = 10;
    float rotationStepDeg = 0.0f;
    int downsample = 2;
    bool useEdgeMaskWeighting = true;

    QImage toGray(const QImage &img);
    double ncc(const QImage &a, const QImage &b, int dx, int dy,
               const QVector<double> *w = nullptr);
    QImage translate(const QImage &src, int dx, int dy);
    QImage rotate(const QImage &src, float degrees);
    QVector<double> computeEdgeWeights(const QImage &gray);
};

#endif // STACKALIGNER_H

