#ifndef OBJECTMASKPREDICTOR_H
#define OBJECTMASKPREDICTOR_H

#include <QtWidgets>
#include <vector>
#include "Main/global.h"

// opencv
#include "opencv2/core.hpp"
#include "opencv2/dnn.hpp"

/*
    Promptable object segmentation for the Develop "Object Mask" tool (brush a rough outline around
    an object; the filled silhouette is refined to the true edge). Runs a SAM 2 encoder + decoder
    ONNX pair through OpenCV's DNN module -- the SAME runtime FocusPredictor / SubjectPredictor use,
    so no onnxruntime dependency is introduced (feasibility proved by the sam_dnn spike: cv::dnn 4.11
    loads and runs both graphs; see notes/Documentation.txt "OBJECT MASK").

    Two-phase, because SAM splits a heavy image encoder from a light promptable decoder:

      setImage()  -- run ONCE per image (on entering Object Mask mode). Encodes the developed base
                     to image_embed + high_res_feats and caches them (+ the guide). ~1s CPU. This is
                     the expensive step; it does NOT depend on the brush, so it is not re-run per
                     stroke.
      refine()    -- run PER brush stroke. Feeds the user's painted-and-filled mask as the decoder's
                     dense mask_input prompt (has_mask_input=1) plus the stroke's bounding box, picks
                     the best of SAM's 3 candidate masks by predicted IoU, morphologically cleans the
                     result, then edge-refines it against the guide (shared SegRefine). ~40ms decode.

    Coverage output matches SubjectPredictor::predict: cov is row-major w*h, 0..1, output-oriented,
    so it drops straight into ObjectMask's ObjectRef store (Develop/objectmask.h) and is sampled by
    both the loupe overlay and buildMaskBuffer through the same normalized onx/ony space.

    NOTE (deployment): the decoder MUST be the fixed-shape export. cv::dnn's ONNX importer rejects
    the stock samexporter decoder's dynamic num_labels/num_points dims ("parseShape: dynamic 'zero'
    shapes are not supported"); pin them (1 label / 2 points) + onnxsim constant-fold offline. The
    encoder loads as-is. onnxsim is a build-time export tool only -- nothing extra ships.
*/
class ObjectMaskPredictor : public QObject
{
    Q_OBJECT
public:
    /* encoderPath/decoderPath: the two ONNX files (next to u2net.onnx in the executable dir).
       inputSize is the encoder's square input (SAM 2 = 1024). */
    ObjectMaskPredictor(const QString& encoderPath, const QString& decoderPath, int inputSize = 1024);

    bool isLoaded() const;            // both nets parsed
    bool hasImage() const;            // setImage() has produced a cached embedding

    /* Encode the developed base ONCE. image is the output-oriented guide (the same downscaled base
       the range/subject masks use). Caches image_embed + high_res_feats + the guide. Returns false
       if the encoder is not loaded or inference failed. */
    bool setImage(const QImage& image);

    /* Refine the rough brush mask to the object edge. brushCov: the painted-and-filled mask, row-
       major bw*bh, output-oriented, 0..1 (any resolution -- it is sampled by normalized coords, so
       it need not match the guide size). Fills cov (w*h, output-oriented, matching the cached guide)
       + dims. Returns false if no image is set or inference failed. */
    bool refine(const std::vector<float>& brushCov, int bw, int bh,
                std::vector<float>& cov, int& w, int& h);

    /* Point-prompt variant for the Brush tool's "AI" auto-mask mode: segment the object under a
       single click (output-normalized onx/ony) so a brush stroke can be confined to it. Uses the
       SAME fixed-shape 2-point decoder as refine() -- one positive point (label 1) + one padding
       point (label -1), has_mask_input=0. Fills cov (w*h, output-oriented, matching the cached
       guide). Requires setImage() first. Returns false if no image is set or inference failed. */
    bool refinePoint(double onx, double ony, std::vector<float>& cov, int& w, int& h);

private:
    /* SAM 2 encoder preprocessing: RGB, resize to inputSize^2, (px/255 - ImageNet mean)/std, NCHW. */
    cv::Mat preprocess(const QImage& image) const;
    /* Threshold logits at 0, remove speckle (open) + keep the largest connected component + fill
       holes -- kills the branch/occluder specks seen in the box-only spike before edge matting. */
    static void cleanupMask(cv::Mat& logits256);

    cv::dnn::Net encoder;
    cv::dnn::Net decoder;
    int inputSize;                    // 1024

    /* Cached per-image encoder outputs (cloned so they outlive the forward's scratch buffers). */
    cv::Mat imageEmbed;               // [1,256,64,64]
    cv::Mat highResFeats0;            // [1,32,256,256]
    cv::Mat highResFeats1;            // [1,64,128,128]
    QImage  guideImage;               // the base used for edge refinement (output-oriented)
    bool    imageSet = false;
};

#endif // OBJECTMASKPREDICTOR_H
