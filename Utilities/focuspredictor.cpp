#include "focuspredictor.h"

/*
    Documentation: see FOCUS PREDICTOR at top of mainwindow.cpp
*/

// opencv version
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

// // onnxruntime version
// FocusPredictor::FocusPredictor(const QString& onnxPath, int inputSize)
//     : env(ORT_LOGGING_LEVEL_WARNING, "FocusPredictor"),
//     session(nullptr), // temporary placeholder, will reassign after init
//     sessionOptions(),
//     imageSize(inputSize)
// {
//     try {
//         sessionOptions.SetIntraOpNumThreads(1);
//         sessionOptions.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_EXTENDED);

//         // Now assign session properly (can only assign after options are configured)
//         session = Ort::Session(env, onnxPath.toStdString().c_str(), sessionOptions);
//         loaded = true;

//         if (G::isLogger)
//             G::log("FocusPredictor::FocusPredictor", "Loaded model " + onnxPath);
//     } catch (const Ort::Exception& e) {
//         qWarning("Failed to load ONNX model: %s", e.what());
//         loaded = false;
//     }
// }

// bool FocusPredictor::isLoaded() const {
//     return loaded;
// }

// QPointF FocusPredictor::predict(const QImage& image)
// {
//     if (!loaded) return QPointF(-1, -1);

//     QImage rgbImage = image.convertToFormat(QImage::Format_RGB888);
//     QImage scaled = rgbImage.scaled(imageSize, imageSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);

//     // Pad to imageSize × imageSize if needed
//     QImage final(imageSize, imageSize, QImage::Format_RGB888);
//     final.fill(Qt::black);
//     QPainter p(&final);
//     p.drawImage(QPoint((imageSize - scaled.width()) / 2, (imageSize - scaled.height()) / 2), scaled);
//     p.end();

//     // Normalize to float32 [0,1], shape [1, 3, H, W]
//     std::vector<float> inputTensorValues(3 * imageSize * imageSize);
//     const uchar* pixels = final.constBits();

//     for (int y = 0; y < imageSize; ++y) {
//         for (int x = 0; x < imageSize; ++x) {
//             int idx = y * imageSize + x;
//             inputTensorValues[idx] = pixels[3 * idx + 0] / 255.0f; // R
//             inputTensorValues[imageSize * imageSize + idx] = pixels[3 * idx + 1] / 255.0f; // G
//             inputTensorValues[2 * imageSize * imageSize + idx] = pixels[3 * idx + 2] / 255.0f; // B
//         }
//     }

//     std::array<int64_t, 4> inputShape = {1, 3, imageSize, imageSize};

//     Ort::MemoryInfo memoryInfo = Ort::MemoryInfo::CreateCpu(
//         OrtDeviceAllocator, OrtMemTypeCPU);
//     Ort::Value inputTensor = Ort::Value::CreateTensor<float>(
//         memoryInfo,
//         inputTensorValues.data(),
//         inputTensorValues.size(),
//         inputShape.data(),
//         inputShape.size()
//         );

//     const char* inputNames[] = {"input"};    // Adjust if your model input name differs
//     const char* outputNames[] = {"output"};  // Adjust if your model output name differs

//     auto outputTensors = session.Run(Ort::RunOptions{nullptr},
//                                      inputNames, &inputTensor, 1,
//                                      outputNames, 1);

//     float* output = outputTensors.front().GetTensorMutableData<float>();
//     return QPointF(output[0], output[1]);  // Assuming model output is normalized (x, y)
// }
