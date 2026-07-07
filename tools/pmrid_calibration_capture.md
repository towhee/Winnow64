# A7R5 noise calibration — capture procedure (for PMRID KSigma)

## What we're measuring and why

PMRID normalizes noise with a **linear model in the sensor's linear signal domain**:

```
var(pixel) = k(ISO) · signal + b(ISO)
```

- **k** = shot-noise (Poisson) term — grows with signal; sets the per-ISO gain.
- **b** = read-noise + fixed floor — signal-independent intercept.

The spike used a *single-frame estimate* of (k, b) at ISO 6400 and already denoised cleanly.
This procedure measures (k, b) properly **at each ISO** via a photon-transfer curve (PTC), then
fits polynomials **K(ISO)** and **B(ISO)** — the two arrays PMRID's `KSigma` needs
(`K_coeff`, `B_coeff`). Result: correct denoising strength at every ISO, not just 6400.

> **ADU** = *Analog-to-Digital Unit* (a.k.a. DN, "digital number") — the raw integer value stored
> per pixel, before any brightness/color processing. On the A7R5 these run from the **black level
> ≈ 512** (a pedestal; "no light" ≠ 0) up to the **white/clip level = 15360** (full well). So e.g.
> "13,000 ADU" = a raw pixel value of 13,000 ≈ 88% of the way to clipping. "signal" below means
> ADU above black.

The whole thing is your own data → cleanly licensed, ships with Winnow.

---

## Gear & environment

- **Tripod** (frames within an ISO must be still) + 2-sec timer or remote.
- **A uniform, flicker-free light source**, in rough order of preference:
  1. An LED tracing/light pad or lightbox, lens defocused, filling the frame.
  2. Clear **blue sky** with a diffuser (opal/white acrylic or a couple of layers of white
     plastic) held over the lens.
  3. An evenly, softly lit **white wall**, fully defocused.
- **Avoid flicker sources**: no fluorescent, no PWM-dimmed LED, no screen. Flicker makes the
  mean jump frame-to-frame and corrupts the variance. Daylight / good continuous LED only.
- **Cool room, one session, short breaks** — read/dark noise rises as the sensor heats.

## Camera settings (A7R5)

- **RAW: Uncompressed** (or Lossless Compressed — both bit-exact; **not** lossy "Compressed").
- **Manual mode**, **manual focus fully defocused** (kills scene detail so the field is flat).
  *Fully defocused* = rack MF to the extreme **opposite** the target's distance (source close to
  the lens → focus to infinity; wall/sky far → focus to minimum) until the EVF shows **no
  resolvable texture, edge, or dust** — just smooth tone. It doesn't change exposure, only blurs.
- **Aperture fixed** at a mid value (~f/5.6–f/8) for the whole run — do **not** change it (it
  changes vignetting). We sweep exposure with **shutter speed only**.
- **High ISO NR: OFF. Long Exposure NR: OFF.** (Long-exp NR would subtract a dark frame for us.)
- Electronic/silent shutter is fine and avoids shake.
- Fixed WB (irrelevant to raw noise, just keep it constant).
- We only analyze the **central ~60%** of the frame in processing, so mild vignetting is OK.

---

## The shot list

At **each ISO**, capture **two sets** of frames:

### 1. Flat-field exposure sweep (the PTC)
- Point at the uniform source. Sweep **shutter speed across the full range**, from
  *just-clipping-white* down to *near-black*, in **~1-stop steps** (≈ 10–13 steps).
- **2 frames at each shutter speed** (a pair — used to cancel fixed-pattern noise by
  differencing; temporal noise = var(F1−F2)/2).
- Don't worry about hitting exact signal levels — over-shoot the range; processing auto-selects
  the unclipped, usable levels.

**Sweep bounds (A7R5 raw levels: black 512, white 15360, so ~14,848 ADU of usable well):**
- **Just-clipping (top):** brightest exposure with **no raw saturation** — target a flat-field
  **mean ≈ 85–90% of white (~13,000–13,900 ADU)**, with even the bright noise tail (99.9th pct)
  still under ~15,000. A flat field is mean + noise, so if the mean sits too near white the noisy
  tail clips and corrupts the variance.
- **Near-black (bottom):** dimmest exposure whose mean is only just above black but still clearly
  above the read floor — target **mean ≈ black + 1–2% of white (~700–800 ADU)**, i.e. a few
  hundred ADU above 512, still visibly brighter than a lens-cap dark. (No need to reach true
  black — the dark frames pin the intercept.)
- **In practice — count stops, don't read meters.** Sony's histogram/zebras come from the JPEG
  preview (tone curve + WB applied), so they don't show the true raw clip point. You don't need
  it. The sweep only has to *span* clip-to-black; the ends get culled. Per ISO:
  1. Expose the field **clearly bright** — as a rough "bright enough?" check set **Zebra to 100+**
     and expose until zebras cover the whole field (JPEG-clipped ⇒ raw at/near clip). Shoot the pair.
  2. **Step shutter down exactly 1 stop = 3 clicks** of the dial (it moves in ⅓-stops),
     e.g. 1/1000 → 1/2000 → 1/4000. Shoot the pair.
  3. **Repeat ~12–13 times** until the frame is essentially black in playback (histogram piled
     hard on the left). One or two extra dark steps for insurance.

  `calibrate_pmrid.py` then rejects raw-saturated frames (top) and dark-indistinguishable ones
  (bottom), keeping the clean ~1%→90% middle.
- **Expect fewer usable steps as ISO rises.** The sweep spans clip (fixed 15360 ADU) down to the
  read-noise floor — i.e. the sensor's dynamic range — which *shrinks* with ISO as gain lifts the
  floor: ~13 steps at ISO 100, ~10–11 at 6400, ~8 at 25600. That's fine: the fit's slope **k**
  comes from the bright points and the intercept **b** from the dark frames, so a shorter
  high-ISO sweep is fully determined. Keep the routine mechanical (step to black every time; the
  extra dark frames are culled) or stop high-ISO sweeps early to save time — either works.
- **Setup so clipping is reachable:** choose the fixed aperture + source brightness so the field
  clips around **1/1000–1/2000 at ISO 100**; stepping down ~13 stops then reaches black around a
  few seconds (fine on a tripod). At high ISO the field clips faster than 1/8000 — if the fastest
  shutter still isn't quite white, just start there and step down (or dim the source a little for
  the high-ISO blocks).

### 2. Bias / dark frames
- **Lens cap on**, 3 frames at the **fastest** shutter (≈ 1/8000). Gives the read-noise floor
  and verifies the black level at that ISO.

### Then repeat sets 1 & 2 for every ISO in the series
- Native range, ~1-stop steps: **100, 200, 400, 800, 1600, 3200, 6400, 12800, 25600**
  (add 51200 if you shoot there). Skip *extended* ISOs (50, 51200+ pull/push) — they're
  derived digitally and we can extrapolate.

**Volume:** ~9 ISO × (≈11 sweep steps × 2 + 3 dark) ≈ **~225 frames**, ~30–45 min. It's
repetitive but simple: set ISO → sweep shutter twice per step → 3 caps-on → next ISO.

---

## Folder layout (naming doesn't matter — I read EXIF)

Just drop everything under one folder; I group by EXIF **ISO + shutter** automatically. Only
help I need: put the lens-cap frames in a `dark/` subfolder (or I auto-detect near-black means).

```
~/Pictures/pmrid_calib/
├── <all flat-field frames, any names>
└── dark/
    └── <lens-cap frames>
```

---

## Minimal variant (first pass, ~15 min)

If you want a quick check before the full run: ISOs **800, 3200, 6400, 12800** only, sweep in
**1.5-stop steps** (~7 usable levels), 2 frames each, 3 darks each (~70 frames). Enough to fit a
usable K/B(ISO) and measure the lift over the single-frame estimate; do the full series later.

---

## What happens next (I'll build `calibrate_pmrid.py`)

Once frames exist, I'll write a processing script that:
1. rawpy-reads each frame, subtracts per-channel black, splits the 4 Bayer channels, crops the
   central ROI.
2. Per ISO, per shutter: differences the pair → temporal variance = var(F1−F2)/2; mean signal =
   (mean(F1)+mean(F2))/2 − black. Rejects clipped/near-black levels.
3. Robust-fits **var = k·μ + b** per ISO (green sites primary; reports R/B too).
4. Fits **K(ISO)** and **B(ISO)** polynomials (linear-ish k, quadratic b, matching PMRID's form),
   picks an anchor ISO, and writes the coefficients + PTC/fit plots.
5. Re-runs `oracle_pmrid.py` with the fitted coeffs vs the single-frame estimate → quantifies the
   quality lift, and pins the final numbers for the C++ integration.

Deliverable: the `K_coeff` / `B_coeff` / anchor / scale that Winnow will bake in for Sony bodies
(and the recipe to repeat for other in-house-decoded sensors).
