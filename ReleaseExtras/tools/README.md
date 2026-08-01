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

# Raw denoiser (`pmrid.onnx`)

The Base **Denoise raw** control uses **PMRID** (MegVii, Apache-2.0), a *pre-demosaic*
sensor-domain denoiser that runs on the CFA mosaic before demosaic. Model file
`pmrid.onnx` lives in `ReleaseExtras/` (committed). Unlike the cv::dnn mask models it runs
through Winnow's **unified inference layer** (`Utilities/inference/`) via **ONNX Runtime**
— CoreML EP on macOS (ANE), DirectML on Windows — so it needs ORT vendored and a
`-DWINNOW_ENABLE_ORT=ON` build (default OFF → the layer is a no-op stub and Denoise raw
silently disables). The C++ engine is `ImageFormats/Raw/pmrid.{h,cpp}`; wiring is in
`rawformat.cpp` + `MW::ensureRawDenoise`. See `tools/pmrid_integration_scope.md` for
provisioning/coeffs.

> The earlier post-demosaic **RawRefinery "TreeNet"** path (`rawdenoise.*` + the ORT
> inference layer) and the unwired **NAFNet** export were removed 2026-07-12; PMRID
> superseded both. Recover them from git history if a post-demosaic engine is wanted.

