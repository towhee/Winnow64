#ifndef STACKALIGNER_H
#define STACKALIGNER_H

#include <QObject>
#include <QImage>
#include <QVector>
#include <QPainter>
#include <QTransform>
#include <QtMath>
#include "main/global.h"

// OpenCV
#include <opencv2/opencv.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/video.hpp>   // for findTransformECC

class StackAligner : public QObject
{
    Q_OBJECT
public:
    explicit StackAligner(QObject *parent = nullptr);

    // Align all images in list to the first image
    QList<QImage> align(const QList<QImage*> &images);
    // Align using OpenCV
    QList<QImage> alignECC(const QList<QImage*> &images);

    // Configuration
    void setSearchRadius(int px);
    void setRotationStep(float deg);
    void setDownsample(int factor);
    void setUseEdgeMaskWeighting(bool enabled);
    void setUseGpu(bool enabled); // placeholder for future GPU path

signals:
    void progress(QString msg, int current, int total);
    void updateStatus(bool keepBase, QString msg, QString src);

private:
    int searchRadius = 10;
    float rotationStepDeg = 0.0f;
    int downsample = 2;
    bool useEdgeMaskWeighting = true;
    bool useGpu = false;

    // helpers
    // Measure best full-res shift around (0,0) with a small search, used for residual check
    QPointF measureResidualShift(const QImage &refFullGray,
                                 const QImage &testFullGray,
                                 int search = 8,
                                 const QVector<double> *w = nullptr);
    // (optional) quickly grayify full-res for residual check
    QImage toGrayFast(const QImage &img);  // alias of toGray or a faster path
    double nccOverlap(const QImage &a, const QImage &b, int dx, int dy,
                      const QVector<double> *w = nullptr);
    QPointF refineSubpixelPeak(const QImage &a, const QImage &b, int dx, int dy,
                               const QVector<double> *w = nullptr);
    QImage translateF(const QImage &src, double dx, double dy);    QImage toGray(const QImage &img);
    double ncc(const QImage &a, const QImage &b, int dx, int dy,
               const QVector<double> *w = nullptr);
    QImage translate(const QImage &src, int dx, int dy);
    QImage rotate(const QImage &src, float degrees);
    QVector<double> computeEdgeWeights(const QImage &gray);
};

#endif // STACKALIGNER_H
