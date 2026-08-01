#!/usr/bin/env python3
"""
PMRID raw-denoise oracle spike (standalone; touches no Winnow code).
Plan: ~/.claude/plans/moonlit-dancing-peach.md

Runs PMRID (Apache-2.0) on a real Sony A7R5 ISO-6400 mosaic and asks: does it
meaningfully + cleanly denoise a full-frame sensor it was NOT trained on (PMRID
was trained on an OPPO phone)? Frames are handheld -> reference-free metrics only.

PMRID's "KSigma" is a LINEAR renormalization that maps the noise at the shot ISO
onto the noise statistics of a fixed anchor ISO (the level the net was trained on),
in the sensor's linear ADU domain -- NOT a sqrt/Anscombe VST. So to reuse the net
cross-sensor we:
  (a) estimate the A7R5 noise model var = k*x + b from THIS frame, express it on the
      OPPO signal scale (V=959), and affine-map it to the OPPO anchor (ISO 1600).
  (b) [baseline] apply OPPO's own coefficients at iso=6400 unchanged (naive misuse).
"""
import os
import sys
import json

import numpy as np
import rawpy
import onnxruntime as ort

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "_pmrid_out")
ONNX = os.path.join(HERE, "pmrid.onnx")
ISO100 = os.path.expanduser("~/Pictures/_test/2026-07-06_0001.arw")
ISO6400 = os.path.expanduser("~/Pictures/_test/2026-07-06_0002.arw")

# OPPO calibration from PMRID run_benchmark.py (the anchor the net was trained for).
K_COEFF = [0.0005995267, 0.00868861]
B_COEFF = [7.11772e-7, 6.514934e-4, 0.11492713]
ANCHOR = 1600.0
V = 959.0
INP_SCALE = 256.0


# ---------------------------------------------------------------- raw I/O + pack
def load_raw(path):
    with rawpy.imread(path) as raw:
        cfa = raw.raw_image_visible.astype(np.float32)
        black = np.array(raw.black_level_per_channel, dtype=np.float32)
        white = float(raw.white_level)
        pat = raw.raw_pattern            # 2x2 indices into color_desc
        desc = raw.color_desc.decode()   # e.g. 'RGBG'
        wb = np.array(raw.camera_whitebalance, dtype=np.float32)
    # Per-2x2-position black (indices 0..3 = TL,TR,BL,BR)
    blk = black[[pat[0, 0], pat[0, 1], pat[1, 0], pat[1, 1]]]
    H, W = cfa.shape
    H -= H % 2
    W -= W % 2
    cfa = cfa[:H, :W]
    m01 = np.empty_like(cfa)
    m01[0::2, 0::2] = (cfa[0::2, 0::2] - blk[0])
    m01[0::2, 1::2] = (cfa[0::2, 1::2] - blk[1])
    m01[1::2, 0::2] = (cfa[1::2, 0::2] - blk[2])
    m01[1::2, 1::2] = (cfa[1::2, 1::2] - blk[3])
    scale = white - black.mean()
    m01 = np.clip(m01 / scale, 0.0, 1.0)
    # Colours of the 2x2 phase, to confirm RGGB (desc[pat]).
    phase = "".join(desc[i] for i in [pat[0, 0], pat[0, 1], pat[1, 0], pat[1, 1]])
    return dict(m=m01, phase=phase, wb=wb, white=white, black=black)


def bayer2rggb(m):
    H, W = m.shape
    return m.reshape(H // 2, 2, W // 2, 2).transpose(0, 2, 1, 3).reshape(H // 2, W // 2, 4)


def rggb2bayer(r):
    H, W, _ = r.shape
    return r.reshape(H, W, 2, 2).transpose(0, 2, 1, 3).reshape(H * 2, W * 2)


# ------------------------------------------------------------- noise estimation
def estimate_noise(m01, bs=16):
    """Fit var = k*mean + b on the green sites via the per-intensity-bin noise floor
    (low-percentile variance rejects textured blocks)."""
    rggb = bayer2rggb(m01)
    g = 0.5 * (rggb[..., 1] + rggb[..., 2])
    H, W = g.shape
    H -= H % bs
    W -= W % bs
    blk = (g[:H, :W].reshape(H // bs, bs, W // bs, bs)
           .transpose(0, 2, 1, 3).reshape(-1, bs * bs))
    mu, var = blk.mean(1), blk.var(1)
    bins = np.linspace(0, 1, 41)
    idx = np.digitize(mu, bins)
    xs, ys = [], []
    for bi in range(1, 41):
        s = idx == bi
        if s.sum() < 30:
            continue
        xs.append(float(mu[s].mean()))
        ys.append(float(np.percentile(var[s], 5)))  # noise floor for this intensity
    xs, ys = np.array(xs), np.array(ys)
    A = np.vstack([xs, np.ones_like(xs)]).T
    k, b = np.linalg.lstsq(A, ys, rcond=None)[0]
    return max(float(k), 1e-8), max(float(b), 1e-10)


# ------------------------------------------------------------------- ksigma VST
def ksigma_affine(x01, cvt_k, cvt_b, inverse=False):
    img = x01 * V
    img = (img - cvt_b) / cvt_k if inverse else img * cvt_k + cvt_b
    return img / V


def cvt_coeffs_a7r5(k, b):
    """A7R5 measured (k,b) in the [0,1] domain -> OPPO-anchor affine coeffs."""
    k_s, sig_s = V * k, V * V * b                  # express on OPPO signal scale
    k_a = np.polyval(K_COEFF, ANCHOR)
    sig_a = np.polyval(B_COEFF, ANCHOR)
    cvt_k = k_a / k_s
    cvt_b = (sig_s / k_s ** 2 - sig_a / k_a ** 2) * k_a
    return cvt_k, cvt_b


def cvt_coeffs_oppo(iso):
    k, sig = np.polyval(K_COEFF, iso), np.polyval(B_COEFF, iso)
    k_a, sig_a = np.polyval(K_COEFF, ANCHOR), np.polyval(B_COEFF, ANCHOR)
    return k_a / k, (sig / k ** 2 - sig_a / k_a ** 2) * k_a


# ---------------------------------------------------------------- PMRID (tiled)
_SESS = None


def sess():
    global _SESS
    if _SESS is None:
        _SESS = ort.InferenceSession(ONNX, providers=["CPUExecutionProvider"])
    return _SESS


def _run_packed(inp_rggb01, cvt_k, cvt_b):
    """inp_rggb01: [4,h,w] in [0,1]; h,w padded to mult of 32 by caller-agnostic pad here."""
    _, h, w = inp_rggb01.shape
    ph, pw = (32 - h % 32) % 32, (32 - w % 32) % 32
    x = np.pad(inp_rggb01, [(0, 0), (0, ph), (0, pw)], mode="reflect")
    x = (ksigma_affine(x, cvt_k, cvt_b) * INP_SCALE)[None].astype(np.float32)
    y = sess().run(["output"], {"input": x})[0][0] / INP_SCALE
    y = ksigma_affine(y, cvt_k, cvt_b, inverse=True)
    return y[:, :h, :w]


def denoise(m01, cvt_k, cvt_b, tile=512, ov=32):
    """Denoise a mosaic (any size) tiled on the PACKED grid, feather-blended."""
    rggb = bayer2rggb(m01).transpose(2, 0, 1)   # [4,H,W] packed
    _, H, W = rggb.shape
    acc = np.zeros((4, H, W), np.float32)
    wsum = np.zeros((H, W), np.float32)
    ys = list(range(0, max(1, H - ov), tile - ov))
    xs = list(range(0, max(1, W - ov), tile - ov))
    for y0 in ys:
        for x0 in xs:
            y1, x1 = min(y0 + tile, H), min(x0 + tile, W)
            out = _run_packed(np.clip(rggb[:, y0:y1, x0:x1], 0, 1), cvt_k, cvt_b)
            th, tw = y1 - y0, x1 - x0
            fy = np.minimum(np.arange(th) + 1, th - np.arange(th)).astype(np.float32)
            fx = np.minimum(np.arange(tw) + 1, tw - np.arange(tw)).astype(np.float32)
            wt = np.clip(np.outer(fy, fx) / (ov + 1), 1e-3, 1.0)
            acc[:, y0:y1, x0:x1] += out * wt
            wsum[y0:y1, x0:x1] += wt
    rggb_d = acc / np.maximum(wsum, 1e-6)
    return rggb2bayer(rggb_d.transpose(1, 2, 0))


# ------------------------------------------------------------- preview + metrics
def demosaic_preview(m01, wb):
    """Minimal RGGB bilinear + white balance + gamma. Colours approximate (no CCM);
    the point is a fair noisy-vs-denoised visual, same pipeline both sides."""
    import cv2
    r = bayer2rggb(m01)
    R, G, B = r[..., 0], 0.5 * (r[..., 1] + r[..., 2]), r[..., 3]
    rgb = np.stack([R, G, B], -1)                         # half-res, quick
    g = wb[1] if wb[1] > 0 else 1.0
    gains = np.array([wb[0] / g, 1.0, wb[2] / g], np.float32)
    rgb = np.clip(rgb * gains, 0, 1) ** (1 / 2.2)
    return (rgb * 255).astype(np.uint8)


def flat_sigma(m01, n=40, ps=32):
    """Mean noise std over the flattest patches (green sites), reference-free."""
    g = 0.5 * (bayer2rggb(m01)[..., 1] + bayer2rggb(m01)[..., 2])
    H, W = g.shape
    H -= H % ps
    W -= W % ps
    blk = (g[:H, :W].reshape(H // ps, ps, W // ps, ps)
           .transpose(0, 2, 1, 3).reshape(-1, ps * ps))
    var = blk.var(1)
    flat = np.argsort(var)[:n]        # flattest = least variance
    return float(np.sqrt(var[flat].mean())), flat, (H // ps, W // ps, ps)


def main():
    os.makedirs(OUT, exist_ok=True)
    import cv2

    d = load_raw(ISO6400)
    m = d["m"]
    print(f"ISO6400  phase={d['phase']}  mosaic={m.shape}  wb={d['wb']}")

    k, b = estimate_noise(m)
    cvt_k_a, cvt_b_a = cvt_coeffs_a7r5(k, b)
    cvt_k_b, cvt_b_b = cvt_coeffs_oppo(6400.0)
    print(f"noise model (var=k*x+b):  k={k:.3e}  b={b:.3e}")
    print(f"cvt (a)A7R5-mapped: k={cvt_k_a:.4f} b={cvt_b_a:.4f} | "
          f"(b)OPPO-naive: k={cvt_k_b:.4f} b={cvt_b_b:.4f}")

    # Pick a flat crop and a detail crop (mosaic coords, even-aligned, 1024 px).
    _, flat_idx, (nby, nbx, ps) = flat_sigma(m)
    gfull = 0.5 * (bayer2rggb(m)[..., 1] + bayer2rggb(m)[..., 2])
    from scipy.ndimage import uniform_filter
    def crop_at(cy, cx, s=1024):
        cy = min(max(cy, 0), m.shape[0] - s) & ~1
        cx = min(max(cx, 0), m.shape[1] - s) & ~1
        return cy, cx
    fb = flat_idx[0]
    fcy, fcx = crop_at((fb // nbx) * ps * 2 - 512, (fb % nbx) * ps * 2 - 512)
    gy, gx2 = np.unravel_index(np.argmax(uniform_filter(np.abs(np.gradient(gfull)[0]) +
                                                        np.abs(np.gradient(gfull)[1]), 32)),
                               gfull.shape)
    dcy, dcx = crop_at(gy * 2 - 512, gx2 * 2 - 512)

    results = {"noise_k": k, "noise_b": b,
               "cvt_a": [cvt_k_a, cvt_b_a], "cvt_b": [cvt_k_b, cvt_b_b], "crops": {}}

    for name, (cy, cx) in {"flat": (fcy, fcx), "detail": (dcy, dcx)}.items():
        sub = m[cy:cy + 1024, cx:cx + 1024]
        den_a = denoise(sub, cvt_k_a, cvt_b_a)
        den_b = denoise(sub, cvt_k_b, cvt_b_b)
        s0, _, _ = flat_sigma(sub, n=8, ps=32)
        sa, _, _ = flat_sigma(den_a, n=8, ps=32)
        sb, _, _ = flat_sigma(den_b, n=8, ps=32)
        # visual strip: noisy | PMRID(a) | PMRID(b)
        strip = np.concatenate([demosaic_preview(sub, d["wb"]),
                                demosaic_preview(den_a, d["wb"]),
                                demosaic_preview(den_b, d["wb"])], axis=1)
        cv2.imwrite(os.path.join(OUT, f"{name}.png"), cv2.cvtColor(strip, cv2.COLOR_RGB2BGR))
        results["crops"][name] = dict(at=[int(cy), int(cx)],
                                      sigma_noisy=s0, sigma_a=sa, sigma_b=sb,
                                      reduction_a_pct=100 * (1 - sa / s0),
                                      reduction_b_pct=100 * (1 - sb / s0))
        print(f"[{name}] sigma noisy={s0:.4f} -> (a)={sa:.4f} ({100*(1-sa/s0):.0f}%)  "
              f"(b)={sb:.4f} ({100*(1-sb/s0):.0f}%)")

    with open(os.path.join(OUT, "metrics.json"), "w") as f:
        json.dump(results, f, indent=2)
    print("wrote", OUT)


if __name__ == "__main__":
    main()
