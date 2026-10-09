#!/usr/bin/env python3
"""
Export the SigLIP 2 (base, patch16, 256 px) IMAGE tower to ONNX for Audit Keywords and
verify parity. See ~/.claude/plans/i-suspect-that-many-sparkling-dove.md.

    input   "pixels"     float32 [16, 3, 256, 256]  RGB, squashed (no crop), bicubic,
                                                     (x / 255 - 0.5) / 0.5
    output  "embedding"  float32 [16, 768]          L2-normalised

Every choice below was measured on an M1 Ultra (ORT 1.30, CoreML EP):

  FIXED BATCH OF 16. A dynamic batch dimension makes the CoreML EP fail at run time,
      and batch 16 is where throughput levels off (CPU 16 img/s; CoreML ML Program on
      the GPU 84 / 187 / 203 / 214 img/s at batch 1 / 8 / 16 / 32). The caller pads the
      last batch.
  CONV WITHOUT auto_pad. The exporter writes the patch embedding as auto_pad="VALID",
      and ORT's ML Program builder then omits CoreML's required 'pad' parameter: "Unable
      to parse ML Program ... Required param 'pad' is missing". Explicit zero pads and no
      auto_pad mean the same thing and convert.
  FLOAT16 WEIGHTS, float32 I/O. Half the download (185 MB vs 372 MB), faster on the GPU
      (223 vs 193 img/s), a smaller CoreML compile cache (1.0 vs 1.4 GB); cosine to fp32
      0.99999 on CPU, 0.9996 under CoreML.
  COREML_CACHE_KEY in the metadata, so ORT's compiled-model cache is keyed on the
      export rather than the file path. Bump VERSION with the ModelStore catalog row.
  The GPU, NOT the Neural Engine: the ANE ran this graph at 46 img/s.

The L2 normalisation is inside the graph so the C++ caller cannot forget it, and so a
cosine similarity is a plain dot product.

Writes ReleaseExtras/siglip2_image.onnx (picked up by tools/gen_model_catalog.sh).
Run with the Phase 0 venv:  tools/_kwaudit_venv/bin/python tools/export_siglip.py
"""
import json
import os
import sys
import tempfile
import warnings
from io import BytesIO

import numpy as np
import torch

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from keyword_audit_proto import INDEX_DB, MODELS, OUT, connect_ro, thumb_jpg  # noqa: E402

NAME, SIZE = MODELS["base"]
BATCH = 16
VERSION = 1           # == ModelStore catalog version for siglip2_image.onnx
ONNX = os.path.join(HERE, "..", "ReleaseExtras", "siglip2_image.onnx")


class ImageTower(torch.nn.Module):
    def __init__(self, model):
        super().__init__()
        self.vision = model.vision_model

    def forward(self, pixels):
        f = self.vision(pixel_values=pixels).pooler_output
        return f / torch.sqrt((f * f).sum(-1, keepdim=True))


def prep(jpg):
    from PIL import Image
    im = Image.open(BytesIO(jpg)).convert("RGB").resize((SIZE, SIZE), Image.BICUBIC)
    a = np.asarray(im, dtype=np.float32) / 255.0
    return ((a - 0.5) / 0.5).transpose(2, 0, 1)


def export(tower, path):
    import onnx
    import onnxsim
    from onnx import helper
    from onnxconverter_common import float16

    with tempfile.TemporaryDirectory() as tmp:
        raw = os.path.join(tmp, "raw.onnx")
        torch.onnx.export(tower, (torch.randn(BATCH, 3, SIZE, SIZE),), raw,
                          input_names=["pixels"], output_names=["embedding"],
                          opset_version=17, dynamo=False)
        m, ok = onnxsim.simplify(onnx.load(raw))
        assert ok, "onnxsim failed"

    for n in m.graph.node:
        if n.op_type == "Conv":
            keep = [a for a in n.attribute if a.name not in ("auto_pad", "pads")]
            del n.attribute[:]
            n.attribute.extend(keep)
            n.attribute.append(helper.make_attribute("pads", [0, 0, 0, 0]))

    with warnings.catch_warnings():
        warnings.simplefilter("ignore")   # "truncated to 1e-07" for denormal weights
        m = float16.convert_float_to_float16(m, keep_io_types=True)

    meta = m.metadata_props.add()
    meta.key, meta.value = "COREML_CACHE_KEY", f"siglip2image{VERSION}"
    onnx.save(m, path)


def main():
    from transformers import AutoModel
    import onnxruntime as ort

    model = AutoModel.from_pretrained(NAME, dtype=torch.float32,
                                      attn_implementation="eager").eval()
    tower = ImageTower(model).eval()
    export(tower, ONNX)
    print(f"wrote {os.path.abspath(ONNX)}  {os.path.getsize(ONNX) / 1e6:.0f} MB")

    # Parity on real thumbnails: torch fp32 vs ORT, and vs the Phase 0 embeddings.
    keys = json.load(open(os.path.join(OUT, "emb-base.keys.json")))
    stored = np.load(os.path.join(OUT, "emb-base.npy"))
    pick = list(range(0, len(keys), max(1, len(keys) // BATCH)))[:BATCH]
    db = connect_ro(INDEX_DB)
    x = np.stack([prep(thumb_jpg(db, keys[i])) for i in pick])

    with torch.no_grad():
        ref = tower(torch.from_numpy(x)).numpy()
    so = ort.SessionOptions()
    so.log_severity_level = 3
    sess = ort.InferenceSession(ONNX, so, providers=["CPUExecutionProvider"])
    got = sess.run(["embedding"], {"pixels": x})[0]

    cos_ort = (ref * got).sum(1)
    cos_p0 = (stored[pick].astype(np.float32) * got).sum(1)
    print(f"torch fp32 vs ORT fp16  min cosine {cos_ort.min():.6f}")
    print(f"ORT fp16 vs Phase 0     min cosine {cos_p0.min():.6f}")
    ok = cos_ort.min() > 0.999 and cos_p0.min() > 0.999
    print("PARITY OK" if ok else "PARITY FAILED")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
