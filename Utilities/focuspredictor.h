#ifndef FOCUSPREDICTOR_H
#define FOCUSPREDICTOR_H

#include <QtWidgets>
#include "Main/global.h"

// opencv version
#include "opencv2/core.hpp"
#include "opencv2/dnn.hpp"

class FocusPredictor : public QObject
{
    Q_OBJECT
public:
    FocusPredictor(const QString& onnxPath, int inputSize = 256);
    bool isLoaded() const;
    QPointF predict(const QImage& image);

private:
    cv::dnn::Net net;
    int imageSize;
};

// // onnxruntime version
// #include <onnxruntime_cxx_api.h>

// class FocusPredictor : public QObject
// {
//     Q_OBJECT
// public:
//     FocusPredictor(const QString& onnxPath, int inputSize = 256);
//     bool isLoaded() const;
//     QPointF predict(const QImage& image);

// private:
//     Ort::Env env;
//     Ort::Session session;
//     Ort::SessionOptions sessionOptions;
//     int imageSize;
//     bool loaded = false;
// };

#endif // FOCUSPREDICTOR_H
