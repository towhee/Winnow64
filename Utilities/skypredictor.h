#ifndef SKYPREDICTOR_H
#define SKYPREDICTOR_H

#include <QtWidgets>
#include <vector>
#include "Main/global.h"

// opencv
#include "opencv2/core.hpp"
#include "opencv2/dnn.hpp"

/*
    Sky segmentation for the Develop "Select Sky" mask. Runs TWO models through OpenCV's
    DNN module -- the same runtime as FocusPredictor / SubjectPredictor, so Select Sky
    works in builds without ONNX Runtime -- and fuses them with the colour matte in
    Utilities/skyrefine.h:

      skyseg.onnx         U^2-Net sky net, 320x320 in, d0 = sigmoid sky probability.
                          Crisp edges; fooled by smooth bokeh and calm water.
      sky_segformer.onnx  SegFormer-B4 (ADE20K), 512x512 in, 150-class logits at
                          128x128; sky is class 2. Semantically better, edges are blobs.
                          Exported with a STATIC input shape and simplified (onnxsim) --
                          the stock dynamic-shape export fails OpenCV 4.13's importer
                          ("Shape ... !isDynamicShape"). Verified against ONNX Runtime
                          to 3e-5 logits.

    Both use the common segmentation convention: RGB, /255, ImageNet mean/std. Why the
    pair and how they are fused is in skyrefine.h and "SELECT SKY MASK" in
    notes/Documentation.txt.
*/
class SkyPredictor : public QObject
{
    Q_OBJECT
public:
    SkyPredictor(const QString &skysegPath, const QString &segformerPath);
    bool isLoaded() const;
    /* image: the developed, output-oriented photo, at the resolution the matte should
       have. Fills alpha (row-major w*h, 0..255) and its dims. False if a model is not
       loaded or inference failed. */
    bool predict(const QImage &image, std::vector<uint8_t> &alpha, int &w, int &h);

private:
    cv::dnn::Net skyNet;
    cv::dnn::Net segNet;
};

#endif // SKYPREDICTOR_H
