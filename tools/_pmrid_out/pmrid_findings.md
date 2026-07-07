# PMRID raw-denoise oracle spike — findings

**Verdict: GO.** PMRID (MegVii ECCV'20, Apache-2.0) cleanly denoises a real Sony A7R5
ISO-6400 mosaic despite being trained on an OPPO phone sensor. Clears license, quality,
speed, and — unlike TreeNet — the CoreML/ANE bar.

## Setup
- Model: `MegEngine/PMRID` `torch_pretrained.ckp` → `tools/pmrid.onnx` (dynamic H/W, opset 17).
  ORT-vs-torch parity 2.4e-6.
- Input contract (pinned for C++): pack mosaic → **4-ch RGGB** `[1,4,H/2,W/2]`, normalize
  `(raw−black)/(white−black)` → **KSigma linear noise-normalization to anchor ISO** → ×256 →
  net (residual) → ÷256 → inverse KSigma → unpack. Packed H/W padded to a multiple of 32.
- KSigma is **not** a sqrt/Anscombe VST — it is a *linear affine remap* of the signal that
  makes the shot-ISO noise statistics match a fixed anchor ISO (OPPO anchor = 1600, V=959) that
  the net was trained on.
- Test: `~/Pictures/_test/2026-07-06_0002.arw` (A7R5, ISO 6400, RGGB confirmed, 6376×9564).
  Handheld pair → reference-free metrics.

## Cross-sensor mapping
- A7R5 noise model estimated from the single ISO-6400 frame (per-intensity-bin noise floor):
  **var = k·x + b, k=1.05e-3, b=1.2e-6** (green sites, [0,1] domain).
- Config **(a) A7R5-mapped**: express (k,b) on the OPPO signal scale and affine-map to the OPPO
  anchor → cvt_k=0.959, cvt_b=−2.03.
- Config **(b) OPPO-naive** (baseline): apply OPPO coeffs at iso=6400 unchanged → cvt_k=0.252.

## Results
| crop | noisy σ | (a) σ | (a) reduction | (b) σ | (b) reduction |
|------|---------|-------|---------------|-------|---------------|
| flat   | 0.0016 | 0.0007 | **59%** | 0.0002 | 89% |
| detail | 0.0023 | 0.0011 | **51%** | 0.0003 | 87% |

Visual (`flat.png`, `detail.png` = noisy | a | b):
- **(a) is a clean, natural denoise** — noise gone from shadows/leather/dark regions while ornate
  lamp filigree, pillow-fabric texture, and tiny label text stay sharp. Tone preserved (inverse
  affine restores levels).
- **(b) over-smooths** (waxy) with a green cast — the expected naive-misuse failure. Its higher σ
  reduction is detail loss, not better denoising. Confirms proper sensor calibration matters.

Note (a) already looks great using only a **single-frame noise estimate** — no calibration
frames yet. Proper A7R5 calibration would only refine it.

## Speed / runtime
- **Full-frame 60MP denoise: 3.1s on CPU** (tiled 512-packed, feathered). Interactive-friendly.
- CPU 43 ms/512-tile.

## CoreML / ANE (the TreeNet caveat — resolved)
- CoreML EP: **5 partitions, 145/149 nodes supported** (TreeNet fragmented into ~44).
- **CoreML-vs-CPU max-abs-diff = 1.3e-5** → correct, no green/checkerboard miscompute.
- So PMRID is a viable **ANE** target via the ORT layer (CPU already fast enough as fallback).

## Recommended next steps
1. **Capture A7R5 calibration frames** (flat/dark fields per ISO) to fit proper K/B(ISO)
   polynomials — quantify the lift over the single-frame estimate; clean-licensed (own data).
2. C++ integration per the plan: PMRID pre-demosaic on `RawImage.cfa` in `rawformat.cpp`
   (between `UnpackCfa` and `Demosaic::Run`), gated to the Winnow engine, ISO from
   `G::ISOColumn`, via the ORT layer.
3. Blocker for these exact files: in-house lossless-ARW decoder (they currently route through
   Apple, no CFA). Uncompressed Sony + other in-house Bayer formats benefit immediately.
