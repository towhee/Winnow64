#include "subject.h"
#include "imagedlg.h"

Subject::Subject()
{

}

QImage Subject::mat2QImage(const cv::Mat &src)
{
    static cv::Mat m;
    if (src.channels() == 1)
        cv::cvtColor(src, m, cv::COLOR_GRAY2RGB);
    else
        cv::cvtColor(src, m, cv::COLOR_BGR2RGB);
    return QImage((uchar*) m.data, m.cols, m.rows, m.step, QImage::Format_RGB888);
    //return QImage((uchar*) m.data, m.cols, m.rows, m.step, QImage::Format_RGB32);
}

void Subject::test(const QImage &image, QString title)
{
    ImageDlg imageDlg(image, title);
    imageDlg.exec();
}

void Subject::eyes(QImage &image, QList<QPointF> &pts)
{
    if (G::isLogger) G::log("Subject::eyes");

//    using namespace cv;

    // Convert the Qt image to an OpenCV Mat
    cv::Mat mat;
    mat = cv::Mat(image.height(), image.width(), CV_8UC4, (void*)image.bits(), image.bytesPerLine());

    test(mat2QImage(mat), "Original");

    // Convert the image to grayscale
    cv::Mat gray;
    cv::cvtColor(mat, gray, cv::COLOR_BGR2GRAY);

    /*
    cv::imshow("Image", gray);
    cv::waitKey(0);
    */
    test(mat2QImage(gray), "Gray Scale");

    // Apply the Canny edge detection algorithm
    cv::Mat edges;
    cv::Canny(gray, edges, 50, 150);

    test(mat2QImage(edges), "Edges");
    return;

    const cv::String matWin = "Original";
    const cv::String grayWin = "Gray Scale";
    const cv::String edgeWin = "Edges";
    cv::namedWindow(matWin,cv::WINDOW_AUTOSIZE);
    cv::namedWindow(grayWin, cv::WINDOW_AUTOSIZE);
    cv::namedWindow(edgeWin, cv::WINDOW_AUTOSIZE);
    cv::imshow(matWin, mat);
    cv::imshow(grayWin, gray);
    cv::imshow(edgeWin, edges);
    cv::waitKey(0);
    cv::destroyAllWindows();
    return;

    // Find circles in the edge map using the Hough Circle Transform
    std::vector<cv::Vec3f> circles;
    cv::HoughCircles(edges, circles, cv::HOUGH_GRADIENT, 1, edges.rows / 8, 100, 30, 0, 0);

    int circleCount = circles.size();
    qDebug() << "Subject::eyes count =" << circleCount;
    return;

//    // Draw the circles on the original image
//    for (const auto& circle : circles) {
//        cv::Point center(cvRound(circle[0]), cvRound(circle[1]));
//        int radius = cvRound(circle[2]);
//        cv::circle(mat, center, radius, cv::Scalar(0, 0, 255), 2, cv::LINE_AA);
//    }

    cv::imshow("Image", edges);
    cv::waitKey(0);
    return;

//    qDebug() << "Subject::eyes 5";
    // Circles to QList points
    for (const auto& circle : circles) {
        qDebug() << "Subject::eyes  circle[0]" << circle[0];
        cv::Point center(cvRound(circle[0]), cvRound(circle[1]));
        QPointF pt;
//        pt.setX(center[0][0][0] / image.width());
//        pt.setY(center[0][1] / image.height());
//        pts.append(pt);
    }
}
