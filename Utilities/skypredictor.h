#ifndef SKYPREDICTOR_H
#define SKYPREDICTOR_H

#include <QtWidgets>
#include <vector>
#include "Main/global.h"

// opencv
#include "opencv2/core.hpp"
#include "opencv2/dnn.hpp"

/*
    Sky segmentation for the Develop "Select Sky" mask. Runs a single-channel sky-segmentation ONNX
    model (skyseg.onnx) through OpenCV's DNN module -- the SAME runtime FocusPredictor / SubjectPredictor
    use, so no onnxruntime dependency. predict() returns a full-resolution 0..1 coverage of the sky,
    edge-refined against the photo via the shared SegRefine pipeline (identical matting to the Subject
    mask). See Utilities/segrefine.h.

    Preprocessing is the common segmentation-net convention (RGB, resize to inputSize, /255, per-
    channel ImageNet mean/std). If a given model wants something else, its output will look wrong and
    the normalization/input size (ctor arg) is the knob to change -- diagnose exactly as the Subject
    output-selection was (dump the raw map), don't guess.
*/
class SkyPredictor : public QObject
{
    Q_OBJECT
public:
    SkyPredictor(const QString& onnxPath, int inputSize = 320);
    bool isLoaded() const;
    /* Run the model on image; fill cov (row-major, size w*h) and its dims. False if not loaded or
       inference failed. */
    bool predict(const QImage& image, std::vector<float>& cov, int& w, int& h);

private:
    cv::dnn::Net net;
    int imageSize;
};

#endif // SKYPREDICTOR_H
