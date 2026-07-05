#!/usr/bin/env python3
"""
Produce the two ONNX files Winnow's Object Mask needs, into ReleaseExtras/:

    sam2_encoder.onnx   -- SAM 2 image encoder, shipped as-is (OpenCV DNN loads it directly)
    sam2_decoder.onnx   -- SAM 2 mask decoder, FIXED-SHAPE (pinned prompt dims + constant-folded)

Why the decoder needs fixing: OpenCV DNN's ONNX importer rejects the stock samexporter decoder's
dynamic num_labels/num_points dims ("parseShape: dynamic 'zero' shapes are not supported"). Pinning
them to what ObjectMaskPredictor::refine actually feeds (1 label, 2 points -- a box) and running
onnxsim's constant-folding removes the dynamic Shape/OneHot/Range nodes, after which cv::dnn loads and
runs it (validated by the sam_dnn spike). The pinned shapes MUST match objectmaskpredictor.cpp.

Build-time only: onnx / onnxsim / onnxruntime are used here to PRODUCE the files; nothing extra ships.

Usage:  export_sam2.py [variant]     variant in {tiny (default), small, base_plus, large}
        (tiny is fastest to encode and was the spike's reference; all variants share the interface.)
"""
import os
import shutil
import sys
import urllib.request
from collections import Counter

import onnx
from onnxsim import simplify

VARIANT = sys.argv[1] if len(sys.argv) > 1 else "tiny"
REPO = "https://huggingface.co/vietanhdev/segment-anything-2-onnx-models/resolve/main"
HERE = os.path.dirname(os.path.abspath(__file__))
RELEASE_EXTRAS = os.path.abspath(os.path.join(HERE, ".."))     # ReleaseExtras/
WORK = os.path.join(HERE, "_sam2_dl")                          # cached downloads (gitignored)

# Pinned decoder prompt shapes -- keep in lockstep with ObjectMaskPredictor::refine.
DECODER_INPUT_SHAPES = {
    "point_coords":   [1, 2, 2],       # 1 label, 2 points (the lasso-bbox box: TL + BR)
    "point_labels":   [1, 2],
    "mask_input":     [1, 1, 256, 256],
    "has_mask_input": [1],
}


def fetch(name: str) -> str:
    os.makedirs(WORK, exist_ok=True)
    dst = os.path.join(WORK, name)
    if not os.path.exists(dst):
        url = f"{REPO}/{name}"
        print(f"downloading {url}")
        urllib.request.urlretrieve(url, dst)
    else:
        print(f"using cached {dst}")
    return dst


def main() -> int:
    enc_src = fetch(f"sam2_hiera_{VARIANT}.encoder.onnx")
    dec_src = fetch(f"sam2_hiera_{VARIANT}.decoder.onnx")

    # Encoder: ship unchanged.
    enc_out = os.path.join(RELEASE_EXTRAS, "sam2_encoder.onnx")
    shutil.copyfile(enc_src, enc_out)
    print(f"wrote {enc_out}")

    # Decoder: pin the prompt dims + constant-fold, then verify it is cv::dnn-friendly.
    model = onnx.load(dec_src)
    simplified, ok = simplify(model, overwrite_input_shapes=DECODER_INPUT_SHAPES)
    if not ok:
        print("ERROR: onnxsim failed to simplify the decoder", file=sys.stderr)
        return 1
    onnx.checker.check_model(simplified)
    dec_out = os.path.join(RELEASE_EXTRAS, "sam2_decoder.onnx")
    onnx.save(simplified, dec_out)
    print(f"wrote {dec_out}")

    # Sanity report: Shape MUST be 0 (dynamic control flow folded away) or cv::dnn will reject it.
    counts = Counter(n.op_type for n in simplified.graph.node)
    shape_left = counts.get("Shape", 0)
    print(f"decoder: {sum(counts.values())} nodes, Shape={shape_left} "
          f"(must be 0), ScatterND={counts.get('ScatterND', 0)}")
    if shape_left:
        print("ERROR: dynamic Shape nodes remain -- cv::dnn will not load this decoder",
              file=sys.stderr)
        return 2

    print("done. Both files are in ReleaseExtras/; the CMake build stages them next to the binary.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
