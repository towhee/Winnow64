# PMRID C++ integration — scope

PMRID as a **pre-demosaic raw denoiser** on `RawImage.cfa`, in the in-house decode path only,
driven by the Base "Denoise raw" sliders. Depends on the calibration capture (for the K/B(ISO)
coefficients); everything else can be built and unit-tested first with the single-frame estimate.

## 1. Insertion point (mechanical)

`ImageFormats/Raw/rawformat.cpp` `RawFormat::Decode`, **Engine B** (the `if (!decoded)` in-house
block). Insert **between `SubtractBlack(raw)` and `Demosaic::Run(...)`**:

```
UnpackCfa → SubtractBlack → [PMRID::Denoise(raw, isoK/B, amount)] → Demosaic::Run → RawColor::ToWorking
```

Gating is already correct: Engine A (Apple Core Image) bypasses this block entirely, and
Engine B runs only for `G::decodeRawEngine == winnowDecodeRawEngine` (or Apple-fallback). Add an
explicit `G::decodeRawEngine == winnowDecodeRawEngine` guard if we want to *skip* PMRID on
Apple-decode-failure fallbacks (recommended — otherwise a fallback silently denoises).

## 2. New module `ImageFormats/Raw/pmrid.{h,cpp}`

Mirror `rawdenoise.{h,cpp}` (same shape: `SharedSession()` + `Apply()`), but operating on the
mosaic. Steps (all validated in the oracle `tools/oracle_pmrid.py` — port that math 1:1):
- **Pack** `raw.cfa` (uint16) → 4-ch RGGB `[1,4,H/2,W/2]` float, normalized `(cfa−black)/(white−black)`.
  (Black already subtracted by `SubtractBlack`, so normalize by `white−blackMean`; reuse `raw.white`.)
- **KSigma VST** (linear affine noise-normalization to the anchor ISO) using the camera's
  `K(ISO)`/`B(ISO)` coefficients — **not** a sqrt/Anscombe transform. ×256 in.
- **Tiled ORT run** via the inference layer (512-packed tiles, 32-overlap, feathered), ÷256,
  inverse VST, **unpack** → denoised mosaic in place.
- Reuse `Utilities/inference/InferenceSession` exactly like `rawdenoise.cpp::SharedSession()`
  (load-once, warn-if-absent). Model file `pmrid.onnx` in `ReleaseExtras/`.
- **CoreML/ANE is usable here** (spike: 5 partitions, diff 1.3e-5) — use
  `InferenceDevice::Auto` (NPU), unlike TreeNet which was forced to `CPU`.

## 3. CFA pattern handling (RGGB now; other orientations in C++)

- `raw.pattern == RGGB`: pack directly.
- `BGGR / GRBG / GBRG`: rotate/roll the mosaic to RGGB phase before packing, invert after (the
  net is trained RGGB only). Small helper on `raw.cfa`.
- `XTrans` and unknown: **skip** (no-op) — PMRID is Bayer-only. Falls through to normal demosaic.

## 4. Calibration coefficients (depends on the capture)

- `calibrate_pmrid.py` (to be built after the frame capture) emits per-camera-model
  `K_coeff` / `B_coeff` / `anchor` / `V`. Ship them as a small table keyed by **camera model**
  (from metadata) → coefficients; look up by model, evaluate at `m.ISONum`.
- **Stopgap before calibration**: the single-frame-estimated Sony coefficients from the spike, or
  a conservative default; PMRID no-ops for models with no coefficients (safe).

## 5. ISO source — already correct

`imagedecoder.cpp:311` sets `rawMeta.ISONum` from `dm->sf ... G::ISOColumn`, so `m.ISONum` passed
to `Decode` is the datamodel's parsed ISO. **No change** — just pass `m.ISONum` to `PMRID::Apply`.

## 6. DESIGN DECISION — clean-cache invariant vs the denoise-amount slider  → **(B) CHOSEN**

Today the pre-develop `WorkingImage` is cached **clean** (no denoise), and the Base "Denoise raw"
amount is applied as a separate, amount-keyed, async step (`MW::developRawDenoisedBase` /
`ensureRawDenoise`), so moving the slider never forces a re-decode. PMRID is **pre-demosaic**, so
its result is baked into the demosaic → it can't be layered onto the cached clean WorkingImage the
way TreeNet was. Two options:

> **DECIDED (user, 2026-07-07): Option B (adjustable).** NOTE: the user may switch to **A (fixed
> strength)** later depending on real-world results — so keep PMRID's core (`PMRID::Apply` on the
> mosaic) cleanly separable from the blend/slider plumbing, so collapsing to A is a small change,
> not a rewrite.

- **(B) CHOSEN — clean cache + one cached full-strength PMRID base + cheap blend.**
  Main `Decode` stays clean (invariant preserved). When Denoise-raw > 0, `ensureRawDenoise`
  triggers **one** PMRID decode (full strength) and caches the resulting WorkingImage keyed by
  path. The two Base sliders then do a cheap **luma/chroma-split blend** clean↔PMRID at develop
  time — reuse the exact split math already in `rawdenoise.cpp::DenoiseRgb` (luma by denoiseLuma,
  chroma by max(luma,chroma)). PMRID runs **once per image** (re-runs only on image/engine change),
  never per slider tick. Requires: a decode entrypoint that returns the PMRID-denoised pre-develop
  WorkingImage (add a `denoiseMosaic` flag/param to `Decode`, or a sibling method), and repointing
  `ensureRawDenoise` to call it instead of copy+`RawDenoise::Apply`.
- **(A) Simpler — bake at fixed strength, drop the amount.** PMRID always full-strength inside the
  clean decode; the WorkingImage is denoised; no slider. Less code, but loses adjustability and
  changes the clean-cache meaning. Only if we decide raw denoise should be non-adjustable.

## 7. Repoint the Base sliders; relocate TreeNet (coordinate with Track 2)

`denoiseLuma` / `denoiseChroma` currently feed the post-demosaic `RawDenoise::Apply(developed,…)`
(TreeNet) at `rawformat.cpp` ~148. This scope **repoints them to PMRID** and **removes that Base
TreeNet call** — else the image would be denoised twice. TreeNet's post-demosaic role moves to the
Effects local masked "Denoise" (`localDenoiseLuma`) — that's the separate Track 2, but the two must
land together so no slider is left dangling.

## 8. Build / deploy

- `WINNOW_ENABLE_ORT` (already present for TreeNet) gates the module; PMRID no-ops when ORT off.
- Provision `ReleaseExtras/pmrid.onnx` (1.3 MB) + the coefficient table; deployment packaging
  (`packaging/`) copies it next to the executable. Warn-if-absent (raw decode still works, just no
  denoise).

## 9. Boundaries / caveats

- **Lossless-compressed Sony ARW** (incl. the A7R5 test files) can't be unpacked in-house yet →
  they route through Apple (no CFA), so PMRID won't apply until the in-house lossless-ARW decoder
  lands. **Uncompressed Sony and other in-house Bayer formats benefit immediately.**
- Non-raw, X-Trans, Apple-engine: no-op.

## 10. Verification

1. **Oracle parity:** a standalone/unit harness feeds a known RGGB mosaic through `PMRID::Apply`
   and matches `oracle_pmrid.py` output (same pack/VST/tile math) within tolerance.
2. **Unit test** (`tests/`): synthetic RGGB mosaic + a tiny fixed ONNX (or skip-if-absent) → shape
   and no-op-when-disabled behavior; pattern-rotation round-trip for BGGR/GRBG/GBRG.
3. **End-to-end:** decode an **uncompressed** Sony (or other in-house Bayer) high-ISO file with
   Denoise-raw up, Winnow engine → visibly denoised, slider adjusts, no re-decode per tick.

## Decisions (confirmed, user 2026-07-07)

1. **§6 → Option B (adjustable).** May switch to A (fixed) later depending on results — keep the
   core denoiser separable from the blend/slider plumbing so the collapse is cheap.
2. **§7 → yes, one change.** Repoint Base `denoiseLuma`/`denoiseChroma` to PMRID *and* move TreeNet
   to the Effects `localDenoiseLuma` in the same landing, so no slider is left dangling.
3. **§3 → all four Bayer orientations now** (RGGB direct; BGGR/GRBG/GBRG rotate→RGGB→invert).

Build is otherwise gated on the calibration capture (§4 K/B coefficients); the module + wiring can
be built and unit-tested first with the single-frame-estimated Sony coefficients.
