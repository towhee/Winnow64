#!/usr/bin/env python3
"""
Oracle + eyeball test for Winnow's raw denoiser (RawRefinery TreeNet). Reproduces the C++
wrapping in ImageFormats/Raw/rawdenoise.cpp: demosaic to linear RGB, convert scene-linear sRGB ->
linear Rec2020, clip to [0,1], run rawdenoise.onnx with the ISO conditioning (ISO/6400), convert
back, and apply the amount-scaled correction. Writes before/after preview PNGs so the model's
quality can be judged on a real raw, and .npy dumps to diff preprocessing against a C++ dump.

Deps (build-time only): rawpy, numpy, onnxruntime, pillow.

Usage:
    validate_rawdenoise.py --model ../rawdenoise.onnx --raw file.ARW
                           [--crop 1024] [--amount 1.0] [--out-prefix /tmp/rawnr]
"""
import argparse
import os

import numpy as np
import onnxruntime as ort
import rawpy
from PIL import Image

SRGB_TO_REC2020 = np.array([
    [0.62740, 0.32928, 0.04332],
    [0.06909, 0.91954, 0.01138],
    [0.01639, 0.08801, 0.89561]], np.float32)
REC2020_TO_SRGB = np.linalg.inv(SRGB_TO_REC2020).astype(np.float32)


def to_png(lin_srgb, path):
    """gamma-encode linear sRGB [0,1] -> 8-bit PNG for viewing."""
    g = np.clip(lin_srgb, 0, 1) ** (1 / 2.2)
    Image.fromarray((g * 255 + 0.5).astype(np.uint8)).save(path)


def main():
    ap = argparse.ArgumentParser(description="Validate/preview raw denoise vs the C++.")
    ap.add_argument("--model", required=True)
    ap.add_argument("--raw", required=True)
    ap.add_argument("--crop", type=int, default=1024, help="centre crop px (0=full; full can be slow on CPU)")
    ap.add_argument("--amount", type=float, default=1.0, help="denoise blend amount 0..1")
    ap.add_argument("--out-prefix", default="rawnr")
    args = ap.parse_args()

    # Demosaic to LINEAR sRGB (gamma 1, camera WB, no auto-bright), matching Winnow's scene-linear
    # WorkingImage (sRGB primaries) as closely as rawpy allows.
    raw = rawpy.imread(args.raw)
    rgb16 = raw.postprocess(gamma=(1, 1), no_auto_bright=True, output_bps=16,
                            use_camera_wb=True, output_color=rawpy.ColorSpace.sRGB)
    lin = rgb16.astype(np.float32) / 65535.0                      # (H,W,3) linear sRGB [0,1]
    H, W, _ = lin.shape
    if args.crop > 0:
        c = args.crop
        y0 = max(0, (H - c) // 2); x0 = max(0, (W - c) // 2)
        lin = lin[y0:y0 + c, x0:x0 + c]

    iso = 100
    try:
        iso = int(raw.other_settings.iso_speed)   # rawpy >= 0.19
    except Exception:
        pass
    cond = np.array([[iso / 6400.0]], np.float32)

    rec = lin @ SRGB_TO_REC2020.T                                 # linear Rec2020
    inp = np.clip(rec, 0, 1).transpose(2, 0, 1)[None].astype(np.float32)   # (1,3,h,w)

    sess = ort.InferenceSession(args.model, providers=["CPUExecutionProvider"])
    den = sess.run(None, {"input": inp, "cond": cond})[0][0].transpose(1, 2, 0)   # (h,w,3) rec2020

    inC = np.clip(rec, 0, 1)
    corrected = rec + args.amount * (den - inC)                   # highlight-preserving add
    out_srgb = corrected @ REC2020_TO_SRGB.T

    print(f"iso={iso} cond={cond[0,0]:.4f}  crop={lin.shape[:2]}")
    print(f"input  rec2020 min={inC.min():.4f} max={inC.max():.4f} sum={inC.astype(np.float64).sum():.2f}")
    print(f"denoise        min={den.min():.4f} max={den.max():.4f} sum={den.astype(np.float64).sum():.2f}")

    to_png(lin, args.out_prefix + "_before.png")
    to_png(out_srgb, args.out_prefix + "_after.png")
    np.save(args.out_prefix + "_input.npy", inp)
    np.save(args.out_prefix + "_denoise.npy", den)
    print(f"wrote {args.out_prefix}_before.png / _after.png / _input.npy / _denoise.npy")


if __name__ == "__main__":
    main()
