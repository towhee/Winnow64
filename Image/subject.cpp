#include "subject.h"
#include "imagedlg.h"

/*  NOTES

    OpenCV: https://docs.opencv.org/4.x/
    YOLOv7: https://github.com/WongKinYiu/yolov7

*/

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

void Subject::show(const QImage &image, QString title)
{
    ImageDlg imageDlg(image, QSize(1920,1080), title);
    imageDlg.exec();
}

void Subject::poi(QImage &qIm)
{
    // Convert the Qt image to an OpenCV Mat
    cv::Mat image;
    image = cv::Mat(qIm.height(), qIm.width(), CV_8UC4, (void*)qIm.bits(), qIm.bytesPerLine());

    // Convert the image to grayscale
    cv::Mat gray;
    cv::cvtColor(image, gray, cv::COLOR_BGR2GRAY);

    // Load a classifier from its XML description
    const QString classifierUrl = "/Users/roryhill/Projects/Winnow64/Lib/opencv-4.7.0/data/haarcascades/haarcascade_eye.xml";
    cv::CascadeClassifier classifier;
    classifier.load(classifierUrl.toStdString());

    // Prepare a vector where the detected features will be stored
    std::vector<cv::Rect> features;

    // Detect the features in the normalized, gray-scale version of the
    // image. You don't need to clear the previously-found features because the
    // detectMultiScale method will clear before adding new features.
    classifier.detectMultiScale(gray, features);
//    classifier.detectMultiScale(gray, features, 1.1, 2, cv::Size(30, 30));

    qDebug() << "Subject::poi  # features found:" << features.size();

    // Draw each feature as a separate green rectangle
    for (auto&& feature : features) {
        cv::rectangle(image, feature, cv::Scalar(0, 255, 0), 2);
    }

    show(mat2QImage(image), "Test");
}

void Subject::eyes(QImage &image, QList<QPointF> &pts)
{
    if (G::isLogger) G::log("Subject::eyes");

//    using namespace cv;

    // Convert the Qt image to an OpenCV Mat
    cv::Mat mat;
    mat = cv::Mat(image.height(), image.width(), CV_8UC4, (void*)image.bits(), image.bytesPerLine());

    show(mat2QImage(mat), "Original");

    // Convert the image to grayscale
    cv::Mat gray;
    cv::cvtColor(mat, gray, cv::COLOR_BGR2GRAY);

    /*
    cv::imshow("Image", gray);
    cv::waitKey(0);
    */
    show(mat2QImage(gray), "Gray Scale");

    // Apply the Canny edge detection algorithm
    cv::Mat edges;
    cv::Canny(gray, edges, 50, 150);

    show(mat2QImage(edges), "Edges");
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

void Subject::configYolov7(Net_config config)
{
    confThreshold = config.confThreshold;
    nmsThreshold = config.nmsThreshold;
//    class_names.push_back("Potholes");
    class_names.push_back("Animal");
    class_names.push_back("bbbbbb");
    class_names.push_back("Bird");
    class_names.push_back("Human");

//    this->net = cv::dnn::readNetFromONNX(config.modelpath);
    this->net = cv::dnn::readNetFromTorch(config.modelpath);
//    //ifstream ifs("coco.names");
//    std::ifstream ifs("X.names");
////    std::ifstream ifs("Potholes.names");
//    std::string line;
//    int counter = 0;
//    while (std::getline(ifs, line)) {
//        counter++;
//        this->class_names.push_back(line);
//    }
//    qDebug() << "counter =" << counter;
//    this->num_class = class_names.size();

    this->inpWidth = 640;
    this->inpHeight = 640;
}

void Subject::drawYolov7Pred(float conf, int left, int top, int right, int bottom,
                             cv::Mat& frame, int classid)
// Draw the predicted bounding box
{
    //Draw a rectangle displaying the bounding box
    cv::rectangle(frame, cv::Point(left, top), cv::Point(right, bottom),
                  cv::Scalar(255, 0, 255), 2);

    //Get the label for the class name and its confidence
    std::string label = cv::format("%.2f", conf);
    label = this->class_names[classid] + ":" + label;

    //Display the label at the top of the bounding box
    int baseLine;
    cv::Size labelSize = getTextSize(label, cv::FONT_HERSHEY_SIMPLEX, 0.5, 1, &baseLine);
    top = cv::max(top, labelSize.height);
    //rectangle(frame, Point(left, top - int(1.5 * labelSize.height)), Point(left + int(1.5 * labelSize.width), top + baseLine), Scalar(0, 255, 0), FILLED);
    cv::putText(frame, label, cv::Point(left, top), cv::FONT_HERSHEY_SIMPLEX, 0.75,
                cv::Scalar(0, 0, 0), 1);
}

void Subject::detectYolov7(cv::Mat& frame)
{
    cv::Mat blob = cv::dnn::blobFromImage(frame, 1 / 255.0, cv::Size(this->inpWidth, this->inpHeight),
                                     cv::Scalar(0, 0, 0), true, false);
    this->net.setInput(blob);
    std::vector<cv::Mat> outs;
    this->net.forward(outs, this->net.getUnconnectedOutLayersNames());

    int num_proposal = outs[0].size[0];
    int nout = outs[0].size[1];
    if (outs[0].dims > 2)
    {
        num_proposal = outs[0].size[1];
        nout = outs[0].size[2];
        outs[0] = outs[0].reshape(0, num_proposal);
    }
    /////generate proposals
    std::vector<float> confidences;
    std::vector<cv::Rect> boxes;
    std::vector<int> classIds;
    float ratioh = (float)frame.rows / this->inpHeight, ratiow = (float)frame.cols / this->inpWidth;
    int n = 0, row_ind = 0; ///cx,cy,w,h,box_score,class_score
    float* pdata = (float*)outs[0].data;
    for (n = 0; n < num_proposal; n++)   ///ÌØÕ÷Í¼³ß¶È
    {
        float box_score = pdata[4];
        if (box_score > this->confThreshold)
        {
            cv::Mat scores = outs[0].row(row_ind).colRange(5, nout);
            cv::Point classIdPoint;
            double max_class_socre;
            // Get the value and location of the maximum score
            cv::minMaxLoc(scores, 0, &max_class_socre, 0, &classIdPoint);
            max_class_socre *= box_score;
            if (max_class_socre > this->confThreshold)
            {
                const int class_idx = classIdPoint.x;
                float cx = pdata[0] * ratiow;
                float cy = pdata[1] * ratioh;
                float w = pdata[2] * ratiow;
                float h = pdata[3] * ratioh;

                int left = int(cx - 0.5 * w);
                int top = int(cy - 0.5 * h);

                confidences.push_back((float)max_class_socre);
                boxes.push_back(cv::Rect(left, top, (int)(w), (int)(h)));
                classIds.push_back(class_idx);
            }
        }
        row_ind++;
        pdata += nout;
    }

    // Perform non maximum suppression to eliminate redundant overlapping boxes with
    // lower confidences
    std::vector<int> indices;
    cv::dnn::NMSBoxes(boxes, confidences, this->confThreshold, this->nmsThreshold, indices);
    for (size_t i = 0; i < indices.size(); ++i)
    {
        int idx = indices[i];
        cv::Rect box = boxes[idx];
        this->drawYolov7Pred(confidences[idx], box.x, box.y,
                       box.x + box.width, box.y + box.height, frame, classIds[idx]);
    }
}

void Subject::testYolov7(QString imagePath)
{
    Net_config YOLOV7_nets = { 0.3, 0.5, "YOLO/best.onnx" };   ////choices=["models/yolov7_640x640.onnx", "models/yolov7-tiny_640x640.onnx", "models/yolov7_736x1280.onnx", "models/yolov7-tiny_384x640.onnx", "models/yolov7_480x640.onnx", "models/yolov7_384x640.onnx", "models/yolov7-tiny_256x480.onnx", "models/yolov7-tiny_256x320.onnx", "models/yolov7_256x320.onnx", "models/yolov7-tiny_256x640.onnx", "models/yolov7_256x640.onnx", "models/yolov7-tiny_480x640.onnx", "models/yolov7-tiny_736x1280.onnx", "models/yolov7_256x480.onnx"]
//    Net_config YOLOV7_nets = { 0.3, 0.5, "YOLO/best.pt" };   ////choices=["models/yolov7_640x640.onnx", "models/yolov7-tiny_640x640.onnx", "models/yolov7_736x1280.onnx", "models/yolov7-tiny_384x640.onnx", "models/yolov7_480x640.onnx", "models/yolov7_384x640.onnx", "models/yolov7-tiny_256x480.onnx", "models/yolov7-tiny_256x320.onnx", "models/yolov7_256x320.onnx", "models/yolov7-tiny_256x640.onnx", "models/yolov7_256x640.onnx", "models/yolov7-tiny_480x640.onnx", "models/yolov7-tiny_736x1280.onnx", "models/yolov7_256x480.onnx"]
//    Net_config YOLOV7_nets = { 0.3, 0.5, "YOLO/wildeyes.onnx" };   ////choices=["models/yolov7_640x640.onnx", "models/yolov7-tiny_640x640.onnx", "models/yolov7_736x1280.onnx", "models/yolov7-tiny_384x640.onnx", "models/yolov7_480x640.onnx", "models/yolov7_384x640.onnx", "models/yolov7-tiny_256x480.onnx", "models/yolov7-tiny_256x320.onnx", "models/yolov7_256x320.onnx", "models/yolov7-tiny_256x640.onnx", "models/yolov7_256x640.onnx", "models/yolov7-tiny_480x640.onnx", "models/yolov7-tiny_736x1280.onnx", "models/yolov7_256x480.onnx"]
//    Net_config YOLOV7_nets = { 0.3, 0.5, "YOLO/potholes.onnx" };   ////choices=["models/yolov7_640x640.onnx", "models/yolov7-tiny_640x640.onnx", "models/yolov7_736x1280.onnx", "models/yolov7-tiny_384x640.onnx", "models/yolov7_480x640.onnx", "models/yolov7_384x640.onnx", "models/yolov7-tiny_256x480.onnx", "models/yolov7-tiny_256x320.onnx", "models/yolov7_256x320.onnx", "models/yolov7-tiny_256x640.onnx", "models/yolov7_256x640.onnx", "models/yolov7-tiny_480x640.onnx", "models/yolov7-tiny_736x1280.onnx", "models/yolov7_256x480.onnx"]
    configYolov7(YOLOV7_nets);

//    while (img_index <= 822)
//    {
        std::string imgPath = imagePath.toStdString();
        cv::Mat srcimg = cv::imread(imgPath);
        detectYolov7(srcimg);
        show(mat2QImage(srcimg), "WildEyes Test");
//        show(mat2QImage(srcimg), "YOLOv7 Test");
//    }
}
