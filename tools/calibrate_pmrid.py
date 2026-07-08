#!/usr/bin/env python3
"""
Fit PMRID KSigma noise coefficients from a calibration capture folder of defocused
flat-field frames (see tools/pmrid_calibration_capture.md).

Per ISO, each frame gives one (signal, noise-variance) point measured on the green plane
(single-frame spatial method: median variance over flat blocks in a central ROI, robust to
dust/gradient). Fit var01 = k*x01 + b in the WHITE-NORMALISED [0,1] domain (the same domain
as pmrid.cpp's kSonyK/kSonyB), per ISO. Output a per-ISO (k, b) table + plots.

Usage: calibrate_pmrid.py <capture_folder> [--inventory]
  --inventory : just print ISO/shutter/signal/noise per frame (validate the capture), no fit.
"""
import os
import sys
import glob
import json
import subprocess

import numpy as np
import rawpy
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "_pmrid_out")


def exif(path):
    r = subprocess.run(["exiftool", "-T", "-ISO", "-ExposureTime", "-Model", path],
                       capture_output=True, text=True).stdout.strip().split("\t")
    iso = float(r[0])
    et = r[1]
    shutter = (float(et.split("/")[0]) / float(et.split("/")[1])) if "/" in et else float(et)
    return iso, shutter, et, r[2]


def frame_point(path):
    """Return (mean_signal01, var01, clipped, white, black_g) on the green plane, central ROI."""
    with rawpy.imread(path) as raw:
        cfa = raw.raw_image_visible.astype(np.float64)
        black = np.array(raw.black_level_per_channel, dtype=np.float64)
        white = float(raw.white_level)
        pat = raw.raw_pattern
        desc = raw.color_desc.decode()
    # Green sub-plane: pick a G site from the 2x2 phase (works for any Bayer orientation).
    phase = [(0, 0), (0, 1), (1, 0), (1, 1)]
    gpos = next(((dy, dx) for (dy, dx), idx in zip(phase, [pat[0, 0], pat[0, 1], pat[1, 0], pat[1, 1]])
                 if desc[idx] == "G"), (0, 1))
    dy, dx = gpos
    g = cfa[dy::2, dx::2]
    # per-channel black for this green site
    black_g = black[pat[dy, dx]] if pat.shape == (2, 2) else black.mean()
    H, W = g.shape
    roi = g[int(H * 0.2):int(H * 0.8), int(W * 0.2):int(W * 0.8)]
    clipped = float(roi.max()) >= white * 0.98
    scale = white - black_g
    sig = (roi - black_g)
    # Robust local noise: variance within 16x16 blocks (gradient ~0 inside), take the median.
    bs = 16
    Hh, Ww = (roi.shape[0] // bs) * bs, (roi.shape[1] // bs) * bs
    blk = (sig[:Hh, :Ww].reshape(Hh // bs, bs, Ww // bs, bs)
           .transpose(0, 2, 1, 3).reshape(-1, bs * bs))
    var = float(np.median(blk.var(axis=1)))
    mean = float(np.median(sig))
    return mean / scale, var / (scale * scale), clipped, white, black_g


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return
    folder = os.path.expanduser(sys.argv[1])
    inventory_only = "--inventory" in sys.argv
    files = sorted(glob.glob(os.path.join(folder, "*.ARW")) + glob.glob(os.path.join(folder, "*.arw")))
    if not files:
        print("no ARW files in", folder)
        return
    os.makedirs(OUT, exist_ok=True)

    model = None
    pts = {}   # iso -> list of (x01, var01, clipped, shutter)
    for i, f in enumerate(files):
        iso, shutter, et, mdl = exif(f)
        model = model or mdl.replace(" ", "")
        x01, var01, clipped, white, black_g = frame_point(f)
        pts.setdefault(iso, []).append((x01, var01, clipped, shutter))
        print(f"[{i+1:3d}/{len(files)}] ISO {int(iso):5d}  {et:>7s}  "
              f"x={x01:.4f}  var={var01:.3e}  std={np.sqrt(var01):.4f}"
              f"{'  CLIPPED' if clipped else ''}")

    if inventory_only:
        return

    # Per-ISO linear fit var01 = k*x01 + b on the unclipped points.
    isos = sorted(pts)
    table = []
    fig, ax = plt.subplots(1, 2, figsize=(13, 5))
    for iso in isos:
        # keep all UNCLIPPED frames (incl. the lens-cap darks at x~=0, which anchor the read-noise
        # intercept b); clamp tiny-negative dark means to 0.
        rows = [(max(x, 0.0), v) for (x, v, clip, sh) in pts[iso] if not clip]
        darks = [v for (x, v, clip, sh) in pts[iso] if not clip and x < 0.005]
        if len(rows) < 3:
            print(f"ISO {int(iso)}: too few points ({len(rows)}), skipped")
            continue
        x = np.array([a for a, _ in rows]); v = np.array([b for _, b in rows])
        # Quadratic fit var = c2*x^2 + k*x + b: the x^2 term absorbs PRNU (fixed-pattern gain
        # noise, which curves var up at high signal) so k (shot) and b (read) match PMRID's LINEAR
        # noise model. We export only k and b.
        c2, k, b = np.polyfit(x, v, 2)
        resid = v - np.polyval([c2, k, b], x)
        ss = 1.0 - np.sum(resid**2) / max(np.sum((v - v.mean())**2), 1e-30)
        b_dark = float(np.median(darks)) if darks else float("nan")
        table.append([int(iso), float(k), float(max(b, 0.0)), int(len(rows)), float(ss)])
        print(f"ISO {int(iso):5d}: k={k:.4e}  b={max(b,0):.4e}  R2={ss:.4f}  n={len(rows)}"
              f"  (b_from_darks={b_dark:.4e}, {len(darks)} darks)")
        ax[0].scatter(x, v, s=12, label=f"{int(iso)}")
        xs = np.linspace(0, x.max(), 50)
        ax[0].plot(xs, k * xs + b, lw=0.8)

    ax[0].set_title(f"{model}: var vs signal (per ISO)")
    ax[0].set_xlabel("signal (white-normalised)"); ax[0].set_ylabel("noise variance")
    ax[0].legend(fontsize=7, ncol=2)
    tk = np.array(table)
    ax[1].plot(tk[:, 0], tk[:, 1], "o-", label="k (shot)")
    ax[1].plot(tk[:, 0], tk[:, 2], "s-", label="b (read)")
    ax[1].set_xscale("log"); ax[1].set_yscale("log")
    ax[1].set_title("k, b vs ISO"); ax[1].set_xlabel("ISO"); ax[1].legend()
    png = os.path.join(OUT, f"calib_{model}.png")
    fig.tight_layout(); fig.savefig(png, dpi=110)

    js = os.path.join(OUT, f"calib_{model}.json")
    with open(js, "w") as fp:
        json.dump({"model": model, "domain": "white-normalised [0,1], var=k*x+b, green plane",
                   "iso_k_b": [[r[0], r[1], r[2]] for r in table]}, fp, indent=2)
    print("\nwrote", png, "\nwrote", js)


if __name__ == "__main__":
    main()
