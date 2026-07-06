#!/usr/bin/env python3
"""
Download a RawRefinery "TreeNet" checkpoint and export it to ReleaseExtras/rawdenoise.onnx --
the ACTIVE raw denoiser Winnow's "Denoise raw" uses.

TreeNet is a POST-demosaic denoiser:
    forward(inp[1,3,H,W] linear-Rec2020 RGB in [0,1], cond[1,1] = ISO/6400) -> [1,3,H,W]
Winnow runs it via the unified inference layer (Utilities/inference/, ANE on macOS through ORT's
CoreML EP); the C++ engine (ImageFormats/Raw/rawdenoise.cpp) does the scene-linear-sRGB <-> Rec2020
conversion and the ISO conditioning around it.

Weights come from github.com/rymuelle/RawRefinery releases (repo is MIT). NOTE: the model .pt files
are distributed as separate signed release binaries -- CONFIRM their license before redistributing
inside a shipped Winnow build. Build-time only: torch/onnx produce the .onnx, nothing extra ships.

Usage:  export_treenet.py [variant]     variant in {super_light (default), light, standard, heavy}
"""
import os
import sys
import urllib.request

import torch

VARIANTS = {
    "super_light": "ShadowWeightedL1_super_light.pt",   # ~1.4 MB onnx
    "light":       "ShadowWeightedL1_light.pt",
    "standard":    "ShadowWeightedL1.pt",               # ~500 MB
    "heavy":       "ShadowWeightedL1_24_deep_500.pt",
}
BASE = "https://github.com/rymuelle/RawRefinery/releases/download/v1.2.1-alpha"
HERE = os.path.dirname(os.path.abspath(__file__))
RELEASE_EXTRAS = os.path.abspath(os.path.join(HERE, ".."))
WORK = os.path.join(HERE, "_treenet_dl")


def main():
    variant = sys.argv[1] if len(sys.argv) > 1 else "super_light"
    if variant not in VARIANTS:
        raise SystemExit(f"unknown variant {variant!r}; choose from {list(VARIANTS)}")
    fn = VARIANTS[variant]
    os.makedirs(WORK, exist_ok=True)
    pt = os.path.join(WORK, fn)
    if not os.path.exists(pt):
        url = f"{BASE}/{fn}"
        print(f"downloading {url}")
        urllib.request.urlretrieve(url, pt)

    model = torch.jit.load(pt, map_location="cpu").eval()
    rgb = torch.rand(1, 3, 256, 256)
    cond = torch.tensor([[1.0]])                 # ISO/6400 placeholder for tracing
    with torch.no_grad():
        _ = model(rgb, cond)

    out = os.path.join(RELEASE_EXTRAS, "rawdenoise.onnx")
    kw = dict(input_names=["input", "cond"], output_names=["output"], opset_version=17,
              dynamic_axes={"input": {2: "h", 3: "w"}, "output": {2: "h", 3: "w"}})
    try:
        torch.onnx.export(model, (rgb, cond), out, dynamo=False, **kw)
    except TypeError:
        torch.onnx.export(model, (rgb, cond), out, **kw)   # older torch: no dynamo kwarg
    print(f"Wrote {out}  ({os.path.getsize(out)/1e6:.1f} MB, variant={variant})")

    try:
        import numpy as np
        import onnxruntime as ort
        sess = ort.InferenceSession(out, providers=["CPUExecutionProvider"])
        y = sess.run(None, {"input": rgb.numpy().astype(np.float32),
                            "cond": cond.numpy().astype(np.float32)})[0]
        print(f"onnxruntime OK: output {y.shape}")
    except Exception as e:   # noqa: BLE001
        print(f"[warn] onnxruntime verification skipped/failed: {e}")


if __name__ == "__main__":
    main()
