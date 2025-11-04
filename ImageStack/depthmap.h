#ifndef DEPTHMAP_H
#define DEPTHMAP_H

#include <QObject>
#include <QDir>
#include <QImage>
#include <QDebug>
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>

class DepthMap : public QObject
{
    Q_OBJECT
public:
    explicit DepthMap(QObject *parent = nullptr);

    bool generate(const QString &alignedFolderPath, bool smooth = true);

private:
    cv::Mat computeFocusMeasure(const cv::Mat &img);
    cv::Mat anisotropicSmooth(const cv::Mat &depth, int iterations = 25, float k = 0.3f);
};

#endif // DEPTHMAP_H
