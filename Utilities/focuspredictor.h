#ifndef FOCUSPREDICTOR_H
#define FOCUSPREDICTOR_H

#include <QtWidgets>
#include "Main/global.h"
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

#endif // FOCUSPREDICTOR_H
