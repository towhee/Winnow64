#ifndef DEPTHPREDICTOR_H
#define DEPTHPREDICTOR_H

#include <QtWidgets>
#include <vector>
#include "Main/global.h"

// opencv
#include "opencv2/core.hpp"
#include "opencv2/dnn.hpp"

/*
    Monocular depth estimation for the Develop "Depth Range" mask. Runs MiDaS v2.1 small (midas.onnx)
    through OpenCV's DNN module -- the SAME runtime the other predictors use (MiDaS is conv-based and
    is an official OpenCV DNN sample, unlike the ViT DPT / Depth-Anything models which will not load).
    predict() returns a full-resolution depth field normalized to 0..1 with 0 = NEAR and 1 = FAR
    (MiDaS emits inverse depth = high-is-near, so it is inverted), guided-upsampled against the photo
    (Utilities/segrefine.h guidedUpsample -- continuous, no binary contrast). The Depth Range mask then
    selects a [near,far] band of this field, exactly as Luminance Range selects a luminance band.

    Model I/O (validated): input [1,3,256,256]; preprocessing /255 + per-channel ImageNet mean/std;
    single output [1,256,256] inverse depth (unbounded, min-max normalized here).
*/
class DepthPredictor : public QObject
{
    Q_OBJECT
public:
    DepthPredictor(const QString& onnxPath, int inputSize = 256);
    bool isLoaded() const;
    /* Run the model on image; fill depth (row-major, size w*h, 0=near..1=far) + dims. */
    bool predict(const QImage& image, std::vector<float>& depth, int& w, int& h);

private:
    cv::dnn::Net net;
    int imageSize;
};

#endif // DEPTHPREDICTOR_H
