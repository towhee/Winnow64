# SAM 2 Object Mask models

The Develop **Object Mask** tool needs two ONNX files next to the other AI models in
`ReleaseExtras/` (they are not in git — large, like `u2net.onnx`):

- `sam2_encoder.onnx` — SAM 2 image encoder (shipped as-is)
- `sam2_decoder.onnx` — SAM 2 mask decoder, **fixed-shape** (pinned prompt dims + constant-folded)

The decoder **must** be the fixed-shape export: OpenCV DNN's importer rejects the stock decoder's
dynamic `num_labels`/`num_points` dims. `export_sam2.py` pins them to what `ObjectMaskPredictor::refine`
feeds (1 label, 2 points) and runs onnxsim's constant-folding, after which cv::dnn loads and runs it.

## Produce them

```bash
./export_sam2.sh            # tiny (default); or: small | base_plus | large
```

macOS/Linux: the wrapper makes a throwaway venv (`onnx`, `onnxsim`, `onnxruntime` — build-time only,
nothing ships) and runs `export_sam2.py`, writing both files into `ReleaseExtras/`.

Windows: run the Python directly —
```
python -m venv _sam2_venv
_sam2_venv\Scripts\pip install onnx onnxsim onnxruntime
_sam2_venv\Scripts\python export_sam2.py tiny
```

The script prints `Shape=0` for the decoder on success (all dynamic control flow folded away — the
condition for cv::dnn to load it). The CMake build then stages both files next to the binary
(bundled into the `.app` on macOS, copied beside `Winnow.exe` on Windows); absent, the Object Mask
tool warns and no-ops, like the other AI masks.

If you pick a non-`tiny` variant, keep the output filenames `sam2_encoder.onnx` / `sam2_decoder.onnx`
(the loader in `Utilities/objectmaskpredictor.cpp` looks for those names).

---

# Raw denoiser (`rawdenoise.onnx`)

The Base **Denoise raw** control needs `rawdenoise.onnx` in `ReleaseExtras/` (not in git). It runs
through Winnow's **unified inference layer** (`Utilities/inference/`) via **ONNX Runtime** — CoreML EP
on macOS (Apple Neural Engine), DirectML on Windows — NOT OpenCV DNN. So unlike the masks it needs ORT
vendored and the build configured with `-DWINNOW_ENABLE_ORT=ON` (default OFF; without it the inference
layer is a no-op stub and Denoise raw silently disables).

**Active model — RawRefinery "TreeNet"** (post-demosaic): `forward(input[1,3,H,W] linear-Rec2020 RGB,
cond[1,1]=ISO/6400) -> [1,3,H,W]`. The C++ engine (`ImageFormats/Raw/rawdenoise.cpp`) does the
scene-linear-sRGB ↔ Rec2020 conversion and ISO conditioning around it. Weights come from
[RawRefinery](https://github.com/rymuelle/RawRefinery) (repo MIT). **⚠ Confirm the release weights'
license before shipping** — they are separate signed release binaries.

> A **pre-demosaic (4-channel Bayer) NAFNet** is the better long-term design; that path lives in
> `export_nafnet_raw.py` (+ `.sh`) and is **not currently wired** (the C++ engine is post-demosaic RGB).

## Produce it

```bash
./export_treenet.sh            # super_light (default); or: light | standard | heavy
```

macOS/Linux: the wrapper makes a throwaway venv (`torch`, `onnx`, `onnxruntime`, `numpy` — build-time
only), downloads the chosen TreeNet checkpoint, and writes `ReleaseExtras/rawdenoise.onnx`. Sizes:
`super_light` ≈ 1.4 MB (fast, lower quality) → `standard` ≈ 500 MB (best). A/B them on real high-ISO
files and pick before shipping (fp16 can halve `standard`).

Windows: run the Python directly —
```
python -m venv _nafnet_venv
_nafnet_venv\Scripts\pip install torch onnx onnxruntime numpy
_nafnet_venv\Scripts\python export_treenet.py super_light
```

## ONNX Runtime

- **macOS:** `brew install onnxruntime` (Homebrew's build ships the CoreML EP). CMake finds it when
  `WINNOW_ENABLE_ORT=ON`.
- **Windows:** drop the DirectML ORT build under `Lib/onnxruntime/windows/` (`include/`, `lib/`, and
  the runtime DLLs `onnxruntime.dll` / `onnxruntime_providers_shared.dll` / `DirectML.dll`).

## Validate / preview

`validate_rawdenoise.py` reproduces the C++ wrapping (demosaic → linear Rec2020 → model → correction)
and writes before/after preview PNGs so you can judge the denoiser on a real raw, plus `.npy` dumps to
diff preprocessing against a C++ dump:

```bash
_nafnet_venv/bin/pip install rawpy pillow          # oracle-only extra deps
_nafnet_venv/bin/python validate_rawdenoise.py --model ../rawdenoise.onnx --raw file.ARW --crop 1024
```

