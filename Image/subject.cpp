#include "subject.h"

Subject::Subject()
{

}

void Subject::eyes(QImage &image, QList<QPointF> &pts)
{
    if (G::isLogger) G::log("Subject::eyes");

    qDebug() << "Subject::eyes 1";
//    using namespace cv;

    // Convert the Qt image to an OpenCV Mat
    cv::Mat mat;
    mat = cv::Mat(image.height(), image.width(), CV_8UC4, (void*)image.bits(), image.bytesPerLine());

    // Convert the image to grayscale
    cv::Mat gray;
    cv::cvtColor(mat, gray, cv::COLOR_BGR2GRAY);

    // Apply the Canny edge detection algorithm
    cv::Mat edges;
    cv::Canny(gray, edges, 50, 150);

    // Find circles in the edge map using the Hough Circle Transform
    std::vector<cv::Vec3f> circles;
    cv::HoughCircles(edges, circles, cv::HOUGH_GRADIENT, 1, edges.rows / 8, 100, 30, 0, 0);

    qDebug() << "Subject::eyes 5";
    // Circles to QList points
    for (const auto& circle : circles) {
        qDebug() << "Subject::eyes  circle[0]" << circle[0];
        cv::Point center(cvRound(circle[0]), cvRound(circle[1]));
        QPointF pt;
//        pt.setX(center[0][0][0] / image.width());
//        pt.setY(center[0][1] / image.height());
//        pts.append(pt);
    }

//    // Draw the circles on the original image
//    for (const auto& circle : circles) {
//        cv::Point center(cvRound(circle[0]), cvRound(circle[1]));
//        int radius = cvRound(circle[2]);
//        cv::circle(mat, center, radius, cv::Scalar(0, 0, 255), 2, cv::LINE_AA);
//    }
}
