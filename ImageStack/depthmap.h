#ifndef DEPTHMAP_H
#define DEPTHMAP_H

#include <QObject>
#include <QDir>
#include <QImage>
#include <QDebug>
#include <opencv2/opencv.hpp>
// #include <opencv2/core.hpp>
// #include <opencv2/imgproc.hpp>
// #include <opencv2/imgcodecs.hpp>

class DepthMap : public QObject
{
    Q_OBJECT
public:
    explicit DepthMap(QObject *parent = nullptr);

    enum Method {
        Winnow,
        Petteri
    };
    Q_ENUM(Method)

    struct Options {
        Method method = Winnow;
        int threshold  = 10;
        int smoothXY   = 20;
        int smoothZ    = 40;
        int haloRadius = 20;
        bool saveSteps = false;
        bool smooth    = true;
    };

    bool generate(const QString &alignedFolderPath, const Options &opt);

signals:
    void updateStatus(bool keepBase, QString msg, QString src);

private:
    cv::Mat computeFocusMeasure(const cv::Mat &img);
    cv::Mat anisotropicSmooth(const cv::Mat &depth, int iterations = 25,
                              float k = 0.3f);
    bool generateWinnow(const QString &alignedFolderPath, const Options &opt);
    bool generatePetteri(const QString &alignedFolderPath, const Options &opt);
    // void convertZereneDepthMap();
    void fitGauss3points(const std::vector<cv::Mat> &focusMaps,
                         const cv::Mat &indexMap,
                         cv::Mat &mu,
                         cv::Mat &sigma,
                         cv::Mat &amp);};

#endif // DEPTHMAP_H
