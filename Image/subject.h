#ifndef SUBJECT_H
#define SUBJECT_H

#include <QtWidgets>
#include <QObject>
#include "Main/global.h"
#include "opencv2/opencv.hpp"
#include "opencv2/core/core.hpp"
#include "opencv2/highgui/highgui.hpp"
#include "opencv2/imgproc/imgproc.hpp"

class Subject: public QObject
{
    Q_OBJECT
public:
    Subject();
    void eyes(QImage &image, QList<QPointF> &pts);
    QImage mat2QImage(const cv::Mat &m);
    void test(const QImage &image, QString title);
    //cv::Mat m;
};

#endif // SUBJECT_H
