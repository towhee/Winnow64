#!/usr/bin/env python3
"""
Export Depth Anything V2 SMALL to ONNX for the Develop depth field (Depth Range mask and
the Lens panel's Lens Blur), and verify parity. Replaces MiDaS v2.1 small (midas.onnx).

    input   "pixels"  float32 [1, 3, 518, 518]  RGB 0..1, squashed (no crop), bicubic
    output  "depth"   float32 [1, 518, 518]     relative DISPARITY (high = near), unbounded

The ImageNet normalisation is inside the graph so the C++ caller cannot get it wrong.

Choices:
  SMALL ONLY. Depth-Anything-V2-Small is Apache-2.0; Base / Large are CC-BY-NC-4.0 and
      must not ship.
  FIXED 518 x 518 (37 x 37 patches of 14). The CoreML EP wants static shapes, and 518 is
      the resolution the model was trained at. The caller guided-upsamples the result
      against the photo, which is where the edge quality comes from.
  CONV WITHOUT auto_pad, as tools/export_siglip.py: the ML Program builder otherwise omits
      CoreML's required 'pad' parameter.
  FLOAT16 WEIGHTS, float32 I/O (the default; --fp32 for a reference export). 50 MB vs
      99 MB; within 0.4% of fp32 after min-max, on CPU and CoreML alike. CoreML GPU runs
      it in ~20 ms on an M1 Ultra, the CPU EP in ~225 ms.
  COREML_CACHE_KEY in the metadata, keyed on the export. Bump VERSION with the ModelStore
      catalog row.

Writes ReleaseExtras/depth_anything.onnx (picked up by tools/gen_model_catalog.sh).
Run with the Phase 0 venv:  tools/_kwaudit_venv/bin/python tools/export_depth_anything.py
                            [--fp32] [test images...]
"""
import os
import sys
import tempfile
import time
import warnings

import numpy as np
import torch

HERE = os.path.dirname(os.path.abspath(__file__))
NAME = "depth-anything/Depth-Anything-V2-Small-hf"
SIZE = 518
VERSION = 1           # == ModelStore catalog version for depth_anything.onnx
ONNX = os.path.join(HERE, "..", "ReleaseExtras", "depth_anything.onnx")


class Depth(torch.nn.Module):
    def __init__(self, model):
        super().__init__()
        self.model = model
        self.register_buffer("mean", torch.tensor([0.485, 0.456, 0.406]).view(1, 3, 1, 1))
        self.register_buffer("std", torch.tensor([0.229, 0.224, 0.225]).view(1, 3, 1, 1))

    def forward(self, pixels):
        return self.model(pixel_values=(pixels - self.mean) / self.std).predicted_depth


def prep(path):
    from PIL import Image
    im = Image.open(path).convert("RGB").resize((SIZE, SIZE), Image.BICUBIC)
    return (np.asarray(im, dtype=np.float32) / 255.0).transpose(2, 0, 1)[None]


def export(net, path, fp16):
    import onnx
    import onnxsim
    from onnx import helper

    with tempfile.TemporaryDirectory() as tmp:
        raw = os.path.join(tmp, "raw.onnx")
        torch.onnx.export(net, (torch.rand(1, 3, SIZE, SIZE),), raw,
                          input_names=["pixels"], output_names=["depth"],
                          opset_version=17, dynamo=False)
        m, ok = onnxsim.simplify(onnx.load(raw))
        assert ok, "onnxsim failed"

    for n in m.graph.node:
        if n.op_type == "Conv" and any(a.name == "auto_pad" for a in n.attribute):
            keep = [a for a in n.attribute if a.name not in ("auto_pad", "pads")]
            del n.attribute[:]
            n.attribute.extend(keep)
            n.attribute.append(helper.make_attribute("pads", [0, 0, 0, 0]))

    if fp16:
        from onnxconverter_common import float16
        with warnings.catch_warnings():
            warnings.simplefilter("ignore")
            m = float16.convert_float_to_float16(m, keep_io_types=True)

    meta = m.metadata_props.add()
    meta.key, meta.value = "COREML_CACHE_KEY", f"depthanything{VERSION}"
    onnx.save(m, path)


def rel(a, b):
    """Scale-free agreement of two disparity maps: correlation and max abs diff after
    min-max normalisation (what the C++ caller does)."""
    na = (a - a.min()) / max(1e-9, a.max() - a.min())
    nb = (b - b.min()) / max(1e-9, b.max() - b.min())
    return float(np.corrcoef(na.ravel(), nb.ravel())[0, 1]), float(np.abs(na - nb).max())


def main():
    from transformers import AutoModelForDepthEstimation
    import onnxruntime as ort

    args = sys.argv[1:]
    fp16 = "--fp32" not in args
    images = [a for a in args if not a.startswith("--")]

    model = AutoModelForDepthEstimation.from_pretrained(NAME).eval()
    net = Depth(model).eval()
    export(net, ONNX, fp16)
    print(f"wrote {os.path.abspath(ONNX)}  {os.path.getsize(ONNX) / 1e6:.0f} MB"
          f"  ({'fp16' if fp16 else 'fp32'} weights)")

    so = ort.SessionOptions()
    so.log_severity_level = 3
    cpu = ort.InferenceSession(ONNX, so, providers=["CPUExecutionProvider"])
    gpu = None
    if "CoreMLExecutionProvider" in ort.get_available_providers():
        gpu = ort.InferenceSession(
            ONNX, so, providers=[("CoreMLExecutionProvider",
                                  {"ModelFormat": "MLProgram", "MLComputeUnits": "CPUAndGPU"}),
                                 "CPUExecutionProvider"])

    xs = [prep(p) for p in images] or [np.random.rand(1, 3, SIZE, SIZE).astype(np.float32)]
    ok = True
    for i, x in enumerate(xs):
        with torch.no_grad():
            ref = net(torch.from_numpy(x)).numpy()
        t = time.time(); got = cpu.run(["depth"], {"pixels": x})[0]; tc = time.time() - t
        c, d = rel(ref, got)
        line = f"[{i}] torch vs ORT CPU  corr {c:.6f} maxdiff {d:.4f}  ({tc*1000:.0f} ms)"
        ok &= c > 0.9999 and d < 0.02
        if gpu:
            gpu.run(["depth"], {"pixels": x})            # warm up / compile
            t = time.time(); g = gpu.run(["depth"], {"pixels": x})[0]; tg = time.time() - t
            c2, d2 = rel(got, g)
            line += f" | CPU vs CoreML GPU corr {c2:.6f} maxdiff {d2:.4f} ({tg*1000:.0f} ms)"
            ok &= c2 > 0.999 and d2 < 0.05
        print(line, ref.shape)
    print("PARITY OK" if ok else "PARITY FAILED")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
