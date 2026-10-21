#ifndef DEPTHPREDICTOR_H
#define DEPTHPREDICTOR_H

#include <QtWidgets>
#include <memory>
#include <vector>
#include "Main/global.h"

class InferenceSession;

/*
    Monocular depth estimation: the depth field behind the Develop "Depth Range" mask and
    Lens > Lens Blur. Runs Depth Anything V2 Small (depth_anything.onnx, Apache-2.0)
    through the ONNX Runtime layer (Utilities/inference), on the CPU: ~0.25 s per image
    on an M1 Ultra, once per image, with no first-run compile (CoreML took 26 s to
    compile it once, for ~0.2 s saved per image). It replaced MiDaS v2.1 small
    (midas.onnx, cv::dnn), whose 256 px field lost every thin edge; a ViT, it needs ORT
    -- cv::dnn cannot load it.

    predict() returns a full-resolution depth field normalized to 0..1 with 0 = NEAR and
    1 = FAR (the model emits relative DISPARITY, high = near, so it is inverted),
    guided-upsampled against the photo (Utilities/segrefine.h guidedUpsample --
    continuous, no binary contrast). Being min-max normalized disparity, the field is
    linear in a lens's circle of confusion, which is what Lens Blur relies on.

    Model I/O (tools/export_depth_anything.py): input "pixels" [1,3,518,518] RGB 0..1,
    squashed (the ImageNet normalization is inside the graph); output "depth"
    [1,518,518] relative disparity, unbounded (min-max normalized here).
*/
class DepthPredictor
{
public:
    explicit DepthPredictor(const QString &onnxPath);
    ~DepthPredictor();
    bool isLoaded() const;
    /* Run the model on image; fill depth (row-major, size w*h, 0=near..1=far) + dims. */
    bool predict(const QImage &image, std::vector<float> &depth, int &w, int &h);

    static constexpr int kSize = 518;

private:
    std::unique_ptr<InferenceSession> session;
};

#endif // DEPTHPREDICTOR_H
