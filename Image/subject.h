#ifndef SUBJECT_H
#define SUBJECT_H

#include <QtWidgets>
#include <QObject>
#include "Main/global.h"
#include "opencv2/opencv.hpp"
#include <opencv2/dnn.hpp>
#include "opencv2/core/core.hpp"
#include "opencv2/highgui/highgui.hpp"
#include "opencv2/imgproc/imgproc.hpp"

#include <fstream>
#include <sstream>
#include <iostream>

struct Net_config
{
    float confThreshold; // Confidence threshold
    float nmsThreshold;  // Non-maximum suppression threshold
    std::string modelpath;
};

class Subject: public QObject
{
    Q_OBJECT
public:
    Subject();
    void eyes(QImage &image, QList<QPointF> &pts);
    void poi(QImage &qIm);
    QImage mat2QImage(const cv::Mat &m);
    void show(const QImage &image, QString title);
    //cv::Mat m;

    void configYolov7(Net_config);
    void detectYolov7(cv::Mat& frame);
    void testYolov7(QString imagePath);

private:
    int inpWidth;
    int inpHeight;
    std::vector<std::string> class_names;
    int num_class;

    float confThreshold;
    float nmsThreshold;
    cv::dnn::Net net;
    void drawYolov7Pred(float conf, int left, int top, int right, int bottom,
                        cv::Mat& frame, int classid);
};

#endif // SUBJECT_H
