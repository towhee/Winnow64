#ifndef SUBJECTPREDICTOR_H
#define SUBJECTPREDICTOR_H

#include <QtWidgets>
#include <vector>
#include "Main/global.h"

// opencv
#include "opencv2/core.hpp"
#include "opencv2/dnn.hpp"

/*
    Salient-object (subject) segmentation for the Develop "Select Subject" mask. Runs the U^2-Net
    ONNX model (u2net.onnx) through OpenCV's DNN module -- the SAME runtime FocusPredictor uses, so
    no onnxruntime dependency is introduced. predict() returns a single-channel coverage map (0..1,
    min-max normalized) at the model's native 320x320 resolution; the caller registers it (output-
    oriented) in SubjectMask's path store and samples it by normalized coords, so the input's aspect
    squish is irrelevant (preview and render sample the identical normalized space).
*/
class SubjectPredictor : public QObject
{
    Q_OBJECT
public:
    SubjectPredictor(const QString& onnxPath, int inputSize = 320);
    bool isLoaded() const;
    /* Run the model on image; fill cov (row-major, size w*h) and its dims. Returns false if the
       model is not loaded or inference failed. */
    bool predict(const QImage& image, std::vector<float>& cov, int& w, int& h);

private:
    cv::dnn::Net net;
    int imageSize;
};

#endif // SUBJECTPREDICTOR_H
