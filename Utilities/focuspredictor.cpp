#include "focuspredictor.h"

/*
    Documentation: see FOCUS PREDICTOR at top of mainwindow.cpp
*/

FocusPredictor::FocusPredictor(const QString& onnxPath, int inputSize)
    : imageSize(inputSize)
{
    if (G::isLogger)
        G::log("FocusPredictor::FocusPredictor", onnxPath);
    try {
        net = cv::dnn::readNetFromONNX(onnxPath.toStdString());
        net.setPreferableBackend(cv::dnn::DNN_BACKEND_DEFAULT);
        net.setPreferableTarget(cv::dnn::DNN_TARGET_CPU);  // or DNN_TARGET_OPENCL
    } catch (const cv::Exception& e) {
        qWarning("Failed to load ONNX model: %s", e.what());
    }
}

bool FocusPredictor::isLoaded() const {
    return !net.empty();
}

QPointF FocusPredictor::predict(const QImage& image)
{
    if (net.empty()) return QPointF(-1, -1);

    // Convert QImage → cv::Mat in RGB
    QImage resized = image.scaled(imageSize, imageSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    // qDebug() << "FocusPredictor::predict" << resized.size();
    QImage rgbImage = resized.convertToFormat(QImage::Format_RGB888);
    cv::Mat img(rgbImage.height(), rgbImage.width(), CV_8UC3, const_cast<uchar*>(rgbImage.bits()), rgbImage.bytesPerLine());

    // Convert to float tensor [1, 3, H, W]
    cv::Mat blob = cv::dnn::blobFromImage(img, 1.0 / 255.0, cv::Size(imageSize, imageSize), cv::Scalar(), true, false);

    net.setInput(blob);
    cv::Mat output = net.forward();  // Output: [1, 2]

    float x = output.at<float>(0, 0);
    float y = output.at<float>(0, 1);

    return QPointF(x, y);  // normalized coordinates
}
