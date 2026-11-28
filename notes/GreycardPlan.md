# Greycard Review: a Plan for Winnow Develop (2026-11-28)

Review of [jessolmstead/greycard](https://github.com/jessolmstead/greycard) at commit
`6986f89a` (2026-11-26, v0.13+), a Rust/Slint raw editor, GPL-3.0-or-later. Read for
three things: Develop features Winnow lacks, better algorithms, and better or different
masking/replacement models. Plus the question the review was asked for: greycard is
praised for accurate color, so how does Winnow do?

Every change below goes in behind **`G::greycard`** (default off) until a final
decision. Where a change replaces existing behavior, it ships as an A/B choice, not a
replacement, so the two can be compared on the same frame.

**Decided 2026-11-28:**
- Reading and porting GPL code is fine (the old "do not read RawTherapee" rule is lifted;
  Documentation.md §11.6.5 updated). Every port still gets its notice entries.
- `G::greycard` exists only for `G::isRory`: all of this is pre-release.
- Camera Match (C4) is wanted; how to build it, and what a LUT would cost, is §4.1.1.
- Camera Match takes the DCP form (form A of §4.1.1): a synthesized profile, no LUT
  stage. No Strength slider in the first version.
- Start with C1 + C2.
- Masks: build ALL of M1–M4 as options so each can be tested. For mask models, quality
  matters much more than size. But a model downloaded just in time must ASK first,
  stating its size, before the download starts (§4.4).

**Status 2026-11-28:** the infrastructure (flag, Preferences group, render-key fold,
hold-to-compare key, headless tool keys) and C1 + C2 are built and unit-tested, but not
yet tried in the app. The camera-JPEG harness is still to do. See
Documentation.md §11.6.10. Everything else below is still to do.

## 1. Summary

- **Color: Winnow's pipeline already has most of what greycard uses for accuracy.**
  Matrices, Bradford, linear Rec.2020, DCP with HueSatMap/LookTable/tone curve, AgX,
  and a Filmic refitted to Lightroom. The gaps that measure are narrow:
  1. The default profile, Winnow Standard, uses ONE matrix (libraw's D65
     `adobe_coeff`). Greycard interpolates two (StdA and D65) by scene temperature, as
     the DNG spec does. Modelled cost under tungsten: 0.022–0.034 mean Oklab ΔE (just
     visible is ~0.02), p90 0.04–0.07. Zero in daylight (§2.2).
  2. A Winnow Standard white-balance change is a per-channel gain in the WORKING space
     (`diag(channelGain)` left of the matrix in `preMat`), not in camera space. That is
     harmless at As Shot and small for small moves, but it is not the DNG model (§2.3).
  3. Out-of-gamut colors on raws are clamped per channel at the output (Filmic clamps,
     then the bleed). Greycard rolls them in along the Oklab ray at held hue (§2.4).
  4. Against the camera's own JPEG (27 Sony/Nikon frames, daylight), Winnow's default
     develop scores 0.007–0.015 mean Oklab ΔE: as close as greycard's FITTED camera
     look gets on Canon (0.011–0.014), and inside greycard's own no-look baseline
     (0.020–0.023). What is left is the camera's style (§2.1).
- **Missing Develop functions:** soft proofing with a gamut warning, waveform and RGB
  parade scopes, scopes limited to a mask, a clipping overlay, and focus peaking.
- **Better algorithms:** RCD/AMaZE/dual demosaic, opposed-inpaint and learned highlight
  reconstruction, greycard's learned raw denoiser (GPL, trained on real camera raws,
  Bayer and X-Trans), and a grey-edge auto white balance.
- **Masking/replacement models:** BiRefNet-lite for Subject, EoMT-S (COCO panoptic) for
  sky, water, foliage, mountain, building and people from ONE model (it also removes the
  NON-COMMERCIAL SegFormer-B4 from the sky mask), SAM 2.1 small for Object, and SAM 3
  for people parts. RORem (SDXL removal) is noted but not recommended.

## 2. Color Accuracy: How Winnow Is Doing

### 2.0 What greycard means by "accurate"

- **Colorimetric by default.** Camera matrices from rawler (Adobe's, per illuminant),
  the pair nearest the scene interpolated in mired by the white balance's temperature,
  white balance as camera-space gains, Bradford adaptation, linear Rec.2020 working
  space. A DNG's ForwardMatrix is used when present. (`greycard-core/src/color.rs`,
  via the `rawcolor` crate.)
- **No look baked in.** A per-channel display curve (or AgX) and nothing else. Looks are
  opt-in: a DCP, or a "camera match".
- **Brightness per body, measured against the camera's JPEG** (`greycard-edit/src/
  baseline.rs`): 0.8 EV for every body except where a library survey put the body's
  median apart by more than its spread (only the GFX 100S II so far).
- **An Oklab gamut op at the output** (`greycard-render/src/gamut.rs`, v0.13): a rail on
  channels over white, a bound on far-out colors along the Oklab ray (Ottosson 2021), and
  ACES 1.3's reference gamut compression curve on chroma at held hue and lightness.
- **Camera match** (`greycard-match`, `docs/camera-match.md`): from 20–40 of the
  user's own frames per body and style, a 3x3 matrix, three curves and a 33^3 LUT are
  fitted from the embedded JPEG against the colorimetric develop. Their result on
  Canon and GFX: 0.020–0.023 mean ΔE with no look, 0.011–0.014 with the fit (held
  out: 0.012–0.019), chroma alone 0.0015–0.0020 after the fit.

### 2.1 Winnow's default develop against the camera's JPEG (measured)

Method, mirroring greycard's: `Winnow --dumpworking` + `developrender` (mac-release
build of 2026-11-24, default params, no lens correction) at 2048 px; the raw's embedded
full-size JPEG (sRGB only); ECC homography registration; 32 px blocks with texture,
clipped and border blocks dropped; one exposure offset per frame (five renders,
interpolated to land the median Oklab L on the camera's); error in Oklab. Scripts in
the session scratchpad (`camjpeg/measure.py`, `analyze.py`).

33 frames sampled at random from the 2023–2025 library; 27 measured (6 skipped: too
few flat unclipped blocks, or registration under 0.85). All are daylight or overcast.
Sony's Creative Style was Standard on every frame; DRO was Off on the A7R V group (the
clean group, as greycard insists on adaptive settings off) and Auto on the others.

| Group | Frames | Mean ΔE | Lightness | Chroma (a,b) | Chroma ratio | EV to match |
|---|---|---|---|---|---|---|
| A7R V, DRO Off | 15 | 0.0108 | 0.0094 | 0.0044 | 0.94 | −0.10 |
| A1 II, DRO Auto | 6 | 0.0072 | 0.0046 | 0.0041 | 0.94 | +0.10 |
| A1, DRO Auto | 5 | 0.0149 | 0.0128 | 0.0043 | 0.95 | +0.05 |
| Z7 (Neutral) | 1 | 0.0132 | 0.0123 | 0.0035 | 1.04 | +0.18 |

(Per-group medians of each frame's block mean. Pooled over all 23,000 blocks: ΔE
0.0129, chroma alone 0.0043, chroma on neutral blocks 0.0026.)

By the camera's hue, on blocks with camera chroma over 0.04:

| Hue | Blocks | Chroma ratio | Hue shift | Lightness |
|---|---|---|---|---|
| Red/pink | 149 | 0.97 | +4.7° | +0.001 |
| Orange/skin | 964 | 0.95 | +6.7° | −0.010 |
| Yellow/olive | 3934 | 0.92 | −0.8° | +0.007 |
| Green | 1455 | 0.95 | −1.8° | +0.002 |
| Cyan/sky | 333 | 0.97 | +2.7° | −0.013 |
| Blue | 113 | 1.07 | −3.0° | −0.027 |
| Violet | 165 | 1.09 | +4.6° | +0.005 |

Reading it:

- **Winnow's default is as close to Sony's Standard JPEG (0.007–0.015) as greycard's
  FITTED camera look is to Canon's (0.011–0.014)**, and well inside greycard's own
  no-look baseline (0.020–0.023). Different bodies, so this is not a head-to-head, but
  it says the default develop is not the weak point. Neutral greys agree to 0.0026.
- **Brightness at Exposure 0 matches the camera within ~0.1 EV** on these bodies, so
  greycard's per-body baseline (C6) has nothing to add here: Adobe's per-camera
  BaselineExposure table is doing that job.
- **What remains is the camera's style, not an error:** the JPEG is ~5% more
  saturated, puts orange and skin ~7° redder (Winnow's is yellower), and holds blues
  brighter. That is exactly what a Camera Match fit (C4) would absorb; it is also what
  Adobe's "Camera Standard" DCP approximates today.
- **Not covered:** tungsten and mixed light (§2.2 says that is where the gap is),
  saturated out-of-gamut colors (the texture and clip cuts drop most of them), and
  non-Sony bodies beyond one Z7 frame. An honest next run: 20 frames of one tungsten
  session, and the Canon/Fuji samples in `_ThumbTest`.

### 2.2 One matrix vs two under tungsten (modelled)

No spectral data, so this is a model: a surface's D65 color, its appearance under the
light by Bradford adaptation, the sensor's response through Adobe's matrix for that
light (rawler's `A` and `D65` tables, mired-interpolated), then each pipeline renders it
back. Truth is the surface's D65 color. Greycard's pipeline is exact in this model by
construction (it uses the same matrices), so the numbers measure **the size of the gap
between the two models**, not the absolute error of either against reality.

Mean (p90) Oklab ΔE over an sRGB lattice of mid-to-saturated colors. Columns:
(1) a 2856 K scene at As Shot; (2) a 3200 K scene, as-shot 5500 K, the user dials
3200 K; (3) a 6500 K scene, as-shot 5200 K, the user dials 6500 K.

| Camera | (1) tungsten | (2) warmed by hand | (3) cooled by hand |
|---|---|---|---|
| A1 | 0.026 (0.052) | 0.010 (0.018) | 0.008 (0.017) |
| A1 II | 0.023 (0.045) | 0.012 (0.022) | 0.008 (0.016) |
| A7R V | 0.024 (0.045) | 0.013 (0.021) | 0.009 (0.018) |
| A9 II | 0.034 (0.074) | 0.010 (0.016) | 0.009 (0.019) |
| Z7 | 0.024 (0.038) | 0.014 (0.022) | 0.008 (0.016) |
| D850 | 0.024 (0.044) | 0.011 (0.018) | 0.006 (0.012) |
| R5 | 0.022 (0.042) | 0.016 (0.028) | 0.008 (0.016) |

Reading it:

- **Tungsten at As Shot** is the real gap: the D65 matrix applied to a tungsten frame
  is off by a just-visible amount on average and clearly visible on saturated colors
  (worst single colors 0.09–0.33). Hue error on saturated colors 2–5°. Skin
  0.006–0.011, under the threshold.
- **Working-space WB (§2.3) partly hides it** when the user warms a daylight as-shot:
  the working-space gains happen to correct some of the matrix error (0.010–0.016,
  versus 0.020–0.029 for camera-space gains with the D65 matrix). Fixing §2.2 without
  §2.3, or §2.3 without §2.2, can make things worse; they go together.
- **The opposite move** (a slightly warm as-shot cooled to daylight, last column) costs
  Winnow 0.006–0.009 where the camera-space model costs 0.002. Under threshold.
- Picking **Adobe Standard** in the Profile row already gets the dual-illuminant
  behavior, since Winnow's DCP path interpolates in mired. The gap is only the default.

### 2.3 Where Winnow's white balance goes

`Develop::buildPointCoeffs` folds `diag(channelGain) · camToWorking · diag(asShotMul)`
into `preMat`, where `channelGain` carries `WhiteBalance::relativeGains` (a ratio of
rendered neutrals in the working space). On the DCP path the white balance is inside
the matrix (DNG-correct). Greycard does camera-space gains plus the matrix at that white
everywhere, and keeps a working-space `white_shift` only as a preview stand-in until the
develop at the new white lands.

### 2.4 Out-of-gamut colors on raws

Filmic clamps each channel at 0 before the curve, and `HighlightRolloff` bleeds the
over-range part into the other channels. A color outside sRGB (a saturated flower, a
stage light, a blue sky in P3) therefore shifts hue as it is cut. Greycard's gamut op
holds Oklab hue and lightness and gives up only chroma, with a rail for over-white
channels so lights still whiten. Winnow's own notes (outputtransform.cpp) show the bleed
was matched to Lightroom on Winnow's curves, so this is an A/B, not a known win.

### 2.5 Verdict

Winnow is doing well. In daylight its default develop lands as close to the camera's
JPEG as greycard's fitted look does, its brightness matches the camera, and it already
matches Lightroom to 1.3–2.2 levels RMS (the 2026-10-04 refit). Greycard's color
advantage is in three corners, each fixable and each measurable:

1. Tungsten and warm light, from the single D65 matrix (C1), together with where the
   white balance is applied (C2). Modelled at a just-visible 0.022–0.034 mean.
2. Saturated colors past the output gamut, clamped per channel (C3). Not yet measured
   on Winnow; the A/B will say.
3. "Looks like my camera": greycard fits it from the user's own JPEGs (C4); Winnow
   relies on Adobe's Camera profiles, which do not exist on Linux or for many Fuji
   looks.

## 3. The `G::greycard` Flag and the A/B Mechanism

**One master switch, isRory only.** BUILT. `G::greycard` is a `G::Greycard` struct in
`Develop/greycard.h`, defined in `Main/global.cpp`. It holds `enabled`, one field per
trial, `active()` (= `enabled && G::isRory`) and `key()`. It is persisted in QSettings
(`Greycard/`) and set from Preferences > Greycard trials, a group built only for isRory.
A stray QSettings value on another machine does nothing. The struct lives in its own
header, not in global.h, so a trial that adds a field rebuilds only what includes it.

**Per-trial selectors.** Each trial below has its own A/B choice, shown only while
`G::greycard` is on: in the panel where the setting lives (the Demosaic combo gains
"RCD (greycard)", the Denoise engine gains "Greycard learned", and so on), or for trials
with no natural home, in the Preferences group. Off, every trial is invisible and every
render is byte-identical to today: `tst_outputtransform::outputBytesAreUnchanged` is the
gate.

**Not in the sidecar, until decided.** A trial's choice is global state, not an
EditParams key, because sidecar keys are a published format. A trial that wins and
needs per-image state gets its key when it is integrated, with a schema step in
`developschema.h` if it changes an existing key's meaning.

**Every render key folds the trial state.** BUILT for develop-stage trials.
`Metadata::renderKey` folds `G::greycard.key()`, which is empty while inactive, so
devPreviews and sidecar thumbnails miss on a toggle. The Preferences toggle calls
`MW::invalidateDevelopRenderer`, which drops Develop's in-memory stack intermediates and
frames. A trial that changes the DECODE (A1 demosaics, A2 highlights, A3 denoise) must
also go into the WorkingImageCache tag (`RawRender::Choice::tag`). The memory notes
record the two traps already seen here: a field missed in `copyMetadata`, and a render
key that did not carry a decode choice.

**A hold-to-compare key.** BUILT. Holding `` ` `` in Develop (trials on) shows every
trial as A, the shipping render; releasing shows B. Handled in `MW::eventFilter` ahead
of the Develop arbiter, and only for isRory with trials on, so `` ` `` stays Toggle Pick
for everyone else. It does NOT use the previewStack override as first proposed: that
override swaps the RECIPE, and a trial changes the RENDERER. Instead it flips
`G::Greycard::holdA` and re-renders through `MW::invalidateDevelopRenderer`, the
Preferences toggle's path, so it is still not a second preview path. The A frame is
kept out of the devPreview cache. A status-bar badge reads "greycard A" or "greycard B",
so a screenshot records the side.

**Headless scoring.** BUILT (the harness is not). Both tools take `greycard=1` and
`gc.<trial>=<value>`, parsed by `G::parseToolArg`. An input-stage trial (C1/C2, `gc.sc`)
must go to `--dumpworking`, because a .wkg is written after the matrix and the white
balance. developrender refuses it and says so. `--dumpworking` also gained `temp=` and
`tint=`, so A and B can be dumped at the same white. A trial at the develop or output
stage (C3, C4, F5) goes to developrender. Still to do: the camera-JPEG harness of §2.1
(promoted from the scratchpad to `tests/calibration/camjpeg.py`), which with
`lrcompare.py` scores each trial.

## 4. The Trials

Each: what greycard does, what Winnow does, evidence, source and license, the A/B, the
effort, and the risk. Ranked within each group by expected value.

**License note for all code ports.** Greycard is GPL-3.0-or-later, as are the darktable
and RawTherapee algorithms it ports; all are compatible with Winnow's GPL-3.0-only, and
reading them is allowed (decided 2026-11-28). Each port gets its file-header attribution
and its notice entry (THIRD_PARTY_NOTICES.md, licenses/, the About dialog) in the same
pass.

### 4.1 Color

**C1 + C2 BUILT 2026-11-28** (untested in-app). Preferences selector "Winnow Standard
color": Off / Camera-space WB, D65 matrix (C2 alone) / Dual illuminant, StdA + D65
(C1 + C2). Built as a synthesized ColorMatrix-only DNG profile
(`CameraProfileStore::winnowStandard`), so it uses the tested DCP path: no third color
path, and the as-shot re-solve, gain skipping, mask-scope WB and Temp readout all follow
from that path. The StdA table is `ImageFormats/Raw/cameramatrixa.cpp` (497 bodies; 54
dropped where rawler's D65 differs from libraw's), generated by
`tools/gen_cameramatrix_a.py`. With trials on, the neutral WB dropper solves against
the profile in use. Not yet: Auto WB and the skin dropper on the profile chain.

**C1. Dual-illuminant Winnow Standard.** (High value; small.)
- Greycard: the two calibrations nearest the scene, mired-interpolated at the white
  balance's temperature; a DNG's own ColorMatrix1/2 and ForwardMatrix when present.
- Winnow: `cameramatrix.cpp` (libraw adobe_coeff, D65 only). Winnow's DCP path already
  has the interpolation (`Develop/cameraprofile.*`, mired, warm first).
- Source: rawler's `data/cameras/*.toml` carries `A` and `D65` for most bodies
  (LGPL-2.1, Adobe's numbers); a generator beside `tools/gen_*` builds a second table.
  DNGs use their own tags.
- A/B: Winnow Standard with dual matrices (B) vs D65 only (A). Must ship with C2.
- Risk: the exact As Shot identity (`relativeGains`) must survive; the matrix now
  depends on the Kelvin, so a WB drag changes the matrix too (cost: one 3×3 per render,
  nothing per pixel).

**C2. White balance in camera space for Winnow Standard.** (Goes with C1.)
- Make the Winnow Standard path what the DCP path already is: gains in camera space and
  the matrix at that white inside `InputMatrix`, `channelGain` carrying exposure and the
  RGB sliders only.
- A/B: together with C1, one selector "Camera-space white (greycard)".
- Risk: every edited Winnow Standard image with a non-As-Shot WB renders differently
  under B. If adopted, it is a schema step (old edits keep A).

**C3. Oklab gamut op at the output.** (Medium value; medium.)
- Port `greycard-render/src/gamut.rs` (~700 lines + 1300 of tests) into
  `OutputTransform` as a replacement for clamp + `HighlightRolloff` on scene-referred
  data, for every output space (sRGB, P3, Rec.2020, Adobe RGB).
- A/B: output gamut handling "Per-channel (Winnow)" vs "Hue-held (greycard)".
- Evidence to gather: `lrcompare.py` (Lightroom clips), the camera-JPEG harness on
  saturated frames, and by eye on flowers, stage lights and skies.
- Risk: the per-pixel cost on the proxy; Winnow's bleed was fitted to Lightroom.

**C4. Camera Match: the maker's look fitted from embedded JPEGs.** (Wanted, decided
2026-11-28; large.) Greycard fits a matrix, curves and a 33^3 RGB lattice per body and
style from the user's own frames. How Winnow should build it, and what a LUT would
cost, is §4.1.1.

**C5. Grey-edge auto white balance.** (Medium; small.)
- Greycard: in camera space, the mean of channel gradients (grey-edge) over unclipped
  pixels, weighted by closeness to the as-shot white (a prior), then refined toward the
  greyest samples. A large sky or sea does not pull it.
- A/B: Auto (Winnow) vs Auto (greycard), same button, selector in the WB dropdown.
- Score against Lightroom's Auto WB on the autotone set (`_LRref/Autotone`, which
  carries LR's chosen Temp/Tint in XMP).

**C6. Per-body baseline from the camera's JPEG.** (Low; small.) Winnow already applies
Adobe's per-camera BaselineExposure table, which greycard does not have. Only useful for
bodies Adobe lacks; the harness's `ev_match` per body is the measurement.

### 4.1.1 Camera Match in Winnow: Design and the LUT Question

**The recommendation in one line:** fit the same thing greycard fits, but WRITE IT AS A
DCP (a ForwardMatrix, a ProfileLookTable and a ProfileToneCurve) into Winnow's own
CameraProfiles folder, and add no LUT stage at all.

**Two forms the fitted look can take.**

- **Form A, a synthesized DCP (recommended).** The matrix stage becomes the profile's
  ForwardMatrix (composed with the body's own ColorMatrix1/2, so it inherits C1's two
  illuminants); the curve stage becomes ProfileToneCurve; the residual becomes a
  ProfileLookTable, DNG's 3-D hue/saturation/value table (hue shift, saturation scale,
  value scale per node). Winnow already applies every part of it in DNG SDK order
  (Develop/huesatmap.h, outputlook.h; HueSatMap, then exposure, LookTable, RGBTone
  curve), so there is no new render code.
- **Form B, greycard's RGB lattice.** A 33^3 RGB table applied after the view transform
  on display-referred sRGB, which is the LUT stage built and removed on 2026-09-14
  (commit `4491ed31`), revived as an internal stage with no file import.

**Does the DNG table form lose accuracy? Measured: no.** Offline experiment on the 15
A7R V frames (DRO Off) of §2.1, 11,374 block pairs. Shared stages: a 3x3 in linear
light with a ridge toward identity, then a shared 16-knot curve. Then either an RGB
17^3 lattice (greycard's form) or an HSV 36x8x8 table (the DCP form, which cannot tint
a grey), both trilinear, smoothed and pulled toward identity. Mean Oklab ΔE:

| Stage | In-sample: total / chroma | Held out, 5 folds: total / chroma |
|---|---|---|
| Winnow default (no look) | 0.0154 / 0.0048 | 0.0158 / 0.0049 |
| matrix + curve | 0.0135 / 0.0045 | 0.0152 / 0.0047 |
| + RGB lattice (form B) | 0.0128 / 0.0039 | 0.0151 / 0.0044 |
| + HSV table (form A) | 0.0122 / 0.0032 | 0.0147 / 0.0037 |

The DCP form is at least as good, held out and in-sample. Two cautions:
- **On these frames the gain is small.** Held out, chroma error falls 25% but total ΔE
  only 7%, because Winnow's default is already close to Sony Standard and what is left
  is lightness (0.013–0.014) that varies by scene. Greycard found the same: "what
  remains is lightness, and it is spatial". No per-pixel look can fit the camera's
  local tone mapping.
- **One body, one style, 15 daylight frames from a few sessions,** and simplified fits
  (17^3, no Oklab residual, none of greycard's grey hold or chroma-flat terms). The win
  should be larger for styles far from Winnow's default: Fuji film simulations,
  Vivid/Landscape styles, monochrome styles, and Canon. The fit's own held-out score
  (shown to the user, as greycard does) is what decides per group.

**What a LUT (form B) would mean for Winnow.** The 2026-09-14 design record
(Documentation.md §11.7) already measured most of this:

1. **Speed:** +18 ms on every interactive tick of a 5.9 MP proxy (2.4 to 20.8 ms), 13
   of it the sRGB sandwich, unless the "fused" fourth path is built.
2. **Wide gamut is lost:** the table is defined in sRGB, so the sandwich clips every
   color outside sRGB before the look. A P3 or Rec.2020 export with a camera look
   would come out with sRGB's gamut. Form A runs in linear ProPhoto and loses
   nothing.
3. **Tied to one view transform:** the table is fitted on Standard roll-off's output;
   under AgX, Camera contrast or a profile curve it is simply wrong. Greycard keeps one
   version per display curve and offers "Refit under AgX". Form A carries its own tone
   curve, which replaces the view transform the way Adobe's Camera profiles already do
   in Winnow (`profileToneMapped`), so there is one version.
4. **Five call sites** must pass the look, and a missed one renders without it
   silently (the record's warning).
5. **A new kind of thing in the Profile row,** with its own store, key and
   mutual-exclusion rules. Form A is an ordinary profile: the Profile combo, hover
   preview, presets, Copy/Paste, multi-image edit and the `cameraProfile` sidecar key
   (stored by name) all work today. It can even be copied into Lightroom or
   RawTherapee as a real `.dcp`.
6. **What form B can do that A cannot:** tint a neutral (a style with a cast in its
   greys; Sony Standard's greys agree with Winnow's to 0.0026, so none seen yet), and a
   Strength slider by plain blending. Strength in form A means scaling the table and
   curve toward identity: possible, but a new profile-amount parameter, so it waits.
7. **The 2026-09-14 decision stands either way:** no `.cube` or HaldCLUT import, no
   user tables. Form A does not even add a 3-D RGB stage.

**Staleness applies to both forms.** A fit is against Winnow's base develop, so any
change to the matrices (C1), the gamut handling (C3), Filmic or the highlight path moves
what a fitted look was fitted against. Each fitted profile records the develop revision
it was fitted under (developrevision.h), and the Profile row marks it "fitted under an
older develop: refit". That is also the reason to run C1–C3 before fitting widely.

**The fit, step by step, with what Winnow already has.**

1. **Survey:** body (`canonicalCameraModel`) and picture style for each raw, plus the
   adaptive settings (Sony DRO, Nikon Active D-Lighting, Canon ALO/HTP, Fuji DR and
   tone shifts, OM gradation). These come from MakerNotes through new C++ readers (no
   exiftool at runtime). Sony first, since those are your bodies; tag numbers to be
   verified against real files. Stored as additive CacheDb columns so a library-wide
   survey is a query.
2. **Groups:** body x style, 20+ frames with the adaptive settings off, sampled to ~40
   across folders (one folder alone gives a narrow fit, and the sheet says so).
3. **Develop each frame** at 2048 px through the real pipeline (in-house decode,
   WorkingImageCache, Develop::Apply at defaults with the lens profile ON because the
   camera's JPEG has lens corrections, OutputTransform). In process on a worker,
   ~3–5 s a frame on 60 MP: a 40-frame group takes about 2–3 minutes, with a
   status-bar progress row.
4. **The camera's JPEG:** Winnow's own embedded-preview extraction, full size.
5. **Register** (OpenCV ECC, reject under 0.9 correlation), **pairs** (24–32 px blocks,
   texture and clipped cuts), **per-frame exposure** (exposure is a scene-linear gain,
   so two renders and an interpolation suffice).
6. **Fit,** in the order the DCP applies: matrix in linear light (ridge toward
   identity), tone curve on the neutrals, then the LookTable on pre-curve values
   against the curve's inverse of the camera's. Greycard's regularization terms carry
   over: smoothness, pull to identity where there are no samples, flat lightness across
   chroma (its §260, which stopped chroma noise turning into blotches), and the hold on
   greys no frame showed. Monochrome sets are detected and fitted as grey. The radial
   lightness term is measured and reported per lens, never fitted into the look.
7. **Verify through the real render:** write the DCP, render the held-out frames
   through it, and report THAT ΔE (greycard checked its editor against its script to
   0.001). The curve is applied with Winnow's knee and roll-off, so only the real render
   tells the truth about highlights.
8. **Write:** `AppData/CameraProfiles/Winnow Camera Match/<Body> <Style>.dcp`, with a
   JSON beside it (frames, held-out ΔE, fit version, develop revision). ProfileName
   "Camera Match: <Style>", UniqueCameraModel the canonical model, so
   CameraProfileStore lists it under the body like any profile. A group too small
   borrows another body's fit of the same style (greycard measured about twice the
   native error across Canon bodies) and says so.

**UI.** "Fit camera look…" at the foot of the Profile row's dropdown, behind
`G::greycard`. It opens a sheet listing the groups (frames, adaptive off, fit / borrow /
skip), Fit, progress, and one result line a group ("fitted on 38 frames, ΔE 0.013 held
out"). The A/B is the Profile row itself: Winnow Standard vs Camera Match vs Adobe's
Camera Standard on the same frame, by hover.

**Steps.**

1. The camera-JPEG harness in `tests/calibration` (§3), with form A and form B both
   fitted offline from the same pairs, so the form question stays measured as more
   bodies and styles come in.
2. MakerNote style readers (Sony, then Nikon and OM; Canon and Fuji when there are
   frames to test).
3. The fitter as a C++ module (`Develop/CameraMatch/`), held to the harness's Python
   as its oracle, as greycard's crate is held to fit.py.
4. The DCP writer (TiffWalk has the reader; the writer is new) and the round trip:
   tst_dcp reads back what was written.
5. The sheet and the in-process runner.
6. Only if form A falls short on some style, form B behind the flag for an in-app A/B.

### 4.2 Missing Develop functions

**F1. Soft proofing + gamut warning.** Greycard: S toggles a proof against sRGB, P3,
Rec.2020, Adobe RGB or an ICC file (perceptual or relative), with an optional grey
paint where colors fall outside; a badge names the proof. Winnow has lcms2 vendored.
Medium effort. Not an A/B: a new feature under the flag.

**F2. Waveform and RGB parade scopes; scopes limited to a mask.** Greycard: RGB / Wave /
Parade / Vector, a pointer readout (R G B L), and "Selection", which counts only the
chosen mask's pixels (a skin mask on the vectorscope against the skin line). Winnow has
histogram and vectorscope (`Develop/Scopes`); the scopes plan already lists the next
steps. Medium effort.

**F3. Clipping lamps and overlay.** Two corner lamps colored by which channels clip,
click or J to paint black clipping blue and white clipping red over the picture, after
the tone curve. Small.

**F4. Focus peaking in the loupe** (Shift+J): paint the sharpest parts, measured on the
picture as shown so 1:1 finds missed focus. Winnow's wildlife and macro library makes
this valuable. Small. (Check first whether Browse has a peaking path to reuse.)

**F5. Vignetting held at the calibration's corner.** Greycard evaluates lensfun's `pa`
polynomial at min(r, 1); Winnow's `vignettingGain` extrapolates (floored at 0.05). Only
matters when a lens was calibrated on a smaller sensor than the body's (a full-frame
lens on a GFX, an APS-C lens in full-frame mode): 10,198 of lensfun's 29,594 `pa`
entries rise past r = 1. One line plus a test; A/B under the flag.

### 4.3 Better algorithms

**A1. Demosaic: RCD, AMaZE, and dual (detail + VNG4).** Winnow: Bilinear, PPG, AHD
(LightCraft ports). Greycard: RCD (darktable's default), AMaZE (RawTherapee), VNG4, and
"dual" (RawTherapee's blend: the detail result where local contrast is high, VNG4 where
flat, so noise does not turn to mazes), each a Rust port of GPL code. Add to the
Demosaic combo as "(greycard)" entries. Score: tst_demosaic synthetic mosaics, plus a
crop of fine texture and a flat high-ISO patch. Medium effort; mechanical port.

**A2. Highlight reconstruction: opposed inpaint, and the learned fill.**
- Opposed inpaint (darktable 4.x, Hanno Schwalm et al.): a clipped photosite takes the
  cube-root mean of the other two colors in its 3×3, with a chrominance offset measured
  next to the clip. Small, fast, robust. A/B vs Winnow's LightCraft reconstruction.
- Greycard's learned highlight fill (GPL weights built into the binary, trained only on
  the author's photographs): fields made in the mosaic's frame, two rounds (picture
  first, fill after). Larger; second step.

**A3. Learned raw denoise (greycard's own network).** (High value; large.)
- A joint denoise + demosaic network on the variance-stabilized mosaic, three tiers
  (1.2 M / 2.3 M / 5.3 M parameters, 5 / 9 / 20 MB, about 1–3 s on 24 MP), Bayer and
  X-Trans, trained on real raws from Canon, Fujifilm, Nikon, Sony and Panasonic. GPL.
- Winnow: PMRID, Apache-2.0, trained on one phone sensor, twice broken by its noise
  model (the magenta casts), no X-Trans.
- Port: greycard's noise model and VST (`develop/noise.rs`, `develop/denoise.rs`'s
  `Vst`) and the tiling (`greycard-ai/src/denoise.rs`); the models run through Winnow's
  existing ORT layer. Replaces the demosaic when on, so it sits on the Winnow engine
  path only (as PMRID does).
- A/B: Denoise engine "PMRID" vs "Greycard fast/balanced/best", on the high-ISO pair
  (A7R V ISO 100 vs 6400) already used for PMRID.
- Risk: memory (greycard's notes: 2–5 GB at a 1024 tile on GPU); CoreML EP
  miscompute (validate CPU vs CoreML, as for PMRID).

**A4. Capture sharpen on a pedestal.** Greycard found its RL capture sharpen turned
near-black noise into magenta/blue/white dots at high ISO, because linear Rec.2020 goes
negative after the matrix. Winnow's sharpen is an unsharp mask on perceptual luminance,
so the same failure is less likely, but the test (count pixels with a channel > 0.8
whose 5×5 luma mean is < 0.08, at ISO 6400+) is cheap and worth running once.

### 4.4 Masking and replacement models

All run through Winnow's ORT layer (`Utilities/inference`), are downloaded through
ModelStore, and each gets a license entry. Sizes are greycard's files.

**Decided 2026-11-28: build all of M1–M4 as options**, so each can be tested against
Winnow's current model. Quality matters much more than size for a mask model, so size
does not rule a model out. **Just-in-time download asks first.** When a mask needs a
model that is not on disk, Winnow asks before downloading, naming the model and its
download size ("Download BiRefNet-lite (114 MB) for the Subject mask?"). The download
starts only on Yes. No leaves the mask on the current model and says so. ModelStore is
the one place that asks, so every model gets the prompt.

**M1. Subject: BiRefNet-lite (MIT, fp16, 114 MB) vs U²-Net (176 MB).** Winnow rejected
BiRefNet only because cv::dnn could not load it; ORT can. Better edges (hair, fur,
feathers) are the claim. A/B: Subject model selector. Small–medium.

**M2. Sky and landscape from EoMT-S, COCO panoptic (MIT, 96 MB).** (High value.)
- One run labels every pixel among 133 classes; greycard builds Sky (with a strict gate:
  a sure sky query, a core, touching the frame's edge, then SAM-seeded outline and a
  color-line matte), and Water, Foliage, Mountain and Building masks with per-group
  confidence bars, all from the same run.
- For Winnow: (a) it can replace SegFormer-B4, whose NVIDIA license is NON-COMMERCIAL,
  as the second opinion in the skyseg fusion; (b) it adds four landscape masks and a
  People prior for free. Winnow's library is heavy on bokeh, the known false-sky case:
  greycard's gate was tuned against a "defocused bluish snow slope", the same failure.
- A/B: Sky model "skyseg + SegFormer (Winnow)" vs "skyseg + EoMT" vs "EoMT (greycard
  pipeline)", scored on the 28-frame "Sky Mask Test Images" collection.

**M3. SAM 2.1 Hiera small (Apache-2.0) vs SAM 2 tiny** for Object mask and the Brush's
AI edge. A/B: SAM size selector. Small (same contract; re-export the decoder with fixed
prompt dims only if staying on cv::dnn, which ORT does not need).

**M4. People parts: SAM 3 with a phrase table (SAM License, ~1 GB fp16).** Face, skin,
hair, eyes and iris, lips, teeth, clothing; a cascade of crops for the small parts; a
person chosen by a color signature in a group. Winnow has these only for iPhone ProRAW
(the embedded mattes). Large, and the model is big; the license permits redistribution
with trade-control terms. A/B is not needed (new feature); behind the flag.

**M5. Replacement: RORem (SDXL removal LoRA, CreativeML Open RAIL++-M, ~5 GB, GPU
only).** Winnow decided on 2026-07-17 that large-area fill is not worth the size. Not
recommended. Two cheap ideas from greycard's LaMa path ARE worth an A/B in Winnow's
LaMa: filling at the frame's own resolution up to a hole depth, then scaling (§382), and
the chroma-mottle and "middle vs surround" corrections (§446, §448).

## 5. Order

1. **Infrastructure (§3):** the flag struct, Preferences group, render-key folding,
   hold-to-compare key and tool keys are DONE. Still to do: the camera-JPEG harness in
   `tests/calibration`.
2. **C1 + C2** (dual matrices, camera-space WB): DONE 2026-11-28, untested in-app.
3. **C3** (gamut op) and **F3** (clipping overlay) – both about what happens at the edge
   of the gamut, judged together.
4. **C4** (Camera Match), steps 1–5 of §4.1.1. After C1–C3 so fits are not made
   against a develop that is about to change; the harness (its step 1) can start with
   the infrastructure, since it measures C1–C3 too.
5. **M2** (EoMT sky/landscape; also clears the SegFormer license problem), **M1**
   (BiRefNet), **M3** (SAM 2.1).
6. **A1** (demosaics), **A2** (opposed inpaint), **F5**, **C5**.
7. **A3** (learned denoise): the largest port, the largest payoff for high ISO.
8. **F1, F2, F4** (proofing, scopes, peaking).
9. **M4** (SAM 3 people): in (decided 2026-11-28), with the download prompt.

## 6. Decisions

Settled 2026-11-28:
- GPL sources may be read and ported (A1, A2, A3 and C3 are unblocked).
- `G::greycard` is `G::isRory` only.
- Camera Match is wanted, as form A: a synthesized DCP, no LUT stage (§4.1.1). Form B
  (an internal RGB lattice) only if A measurably falls short on a style.
- No Camera Match Strength slider in the first version.
- Start with C1 + C2 (done).
- M1–M4 are all in, as options. For masks, quality beats size, and a just-in-time
  download asks first with the size (§4.4).

Still open: none.

## 7. Out of Scope, Worth a Separate Review

Greycard's focus stacking (`greycard-core/src/stack*`, ~9,000 lines: focus breathing,
halo and spill handling, faint-texture preservation, highlight headroom, §739–§747) is
outside Develop but is Winnow's core goal. Its HDR and panorama merges write linear
DNGs, which is the plan Winnow parked for Photo Merge.
