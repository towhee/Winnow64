#!/usr/bin/env python3
"""
lrcompare.py -- render Winnow against Lightroom reference exports and score the difference.

The calibration loop for Develop operators (see notes/Documentation.txt, "CALIBRATION LOOP
FOR PUSHED HIGHLIGHTS"), runnable without the Winnow UI:

  1. Each raw is dumped once to a .wkg (Develop/workingdump.h) with
     `Winnow --dumpworking <raw> <out.wkg>` -- done automatically when a case names a raw
     whose dump is missing.
  2. build/mac-debug/tests/developrender renders the dump through the real Develop::Apply
     with the case's BASELINE and TARGET recipes (16-bit sRGB TIFFs).
  3. Each app's change is measured against its OWN baseline -- pull = Y(baseline) -
     Y(target), in levels of 255 on the display axis -- so the two apps' different
     profiles and colour drop out, and the Winnow pull is compared with Lightroom's,
     pixel for pixel after alignment, at 1/4 resolution.

Exact for a profile with no look (Winnow Standard). Lightroom exports: 16-bit sRGB TIFF,
same raw, no crop; the baseline export differs from the target only in the control
under test.

Usage:
  lrcompare.py [--cases FILE] [--only NAME ...] [--sheet OUT.jpg] [--env K=V ...]

  --cases  JSON list of cases (default tests/calibration/cases.json). Each case:
             name         label
             raw          path to the raw (used to make the dump if it is missing)
             wkg          path to the dump (default: <LRREF>/<raw base name>.wkg)
             baseline     Winnow recipe for the reference render, developrender keys,
                          e.g. {"exposure": 2.5}
             target       Winnow recipe under test, e.g. {"exposure": 2.5,
                          "highlights": -100}
             lr_baseline  Lightroom export matching baseline (file in LRREF or a path)
             lr_target    Lightroom export matching target
             halo         optional: also score the halo term (pull on bright pixels 2-10
                          px from dark objects vs far from them, minus Lightroom's) and
                          the error in three bright bands
             crop         optional [y0, y1, x0, x1] in full-resolution Lightroom pixels,
                          for --sheet
           Paths may use ~ and $LRREF (default ~/Pictures/_LRref).
  --env    extra environment for developrender (dev-only tunables while fitting).
  --sheet  write a contact sheet of each case's crop: Lightroom target | Winnow target.

Needs numpy and opencv-python (Homebrew python has both). Renders are cached by recipe
and developrender's mtime in $LRREF/.lrcompare_cache, with each pair's registration.
"""
import argparse, hashlib, json, os, subprocess, sys
import numpy as np
import cv2

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
TOOL = os.path.join(REPO, 'build/mac-debug/tests/developrender')
WINNOW = os.path.join(REPO, 'build/mac-debug/Winnow.app/Contents/MacOS/Winnow')
LRREF = os.path.expanduser(os.environ.get('LRREF', '~/Pictures/_LRref'))
SCALE = 4


def P(path):
    path = os.path.expanduser(path.replace('$LRREF', LRREF))
    return path if os.path.isabs(path) else os.path.join(LRREF, path)


def lum(x):
    return 0.2126 * x[..., 0] + 0.7152 * x[..., 1] + 0.0722 * x[..., 2]


def read16(path):
    a = cv2.imread(path, cv2.IMREAD_UNCHANGED)
    if a is None:
        sys.exit('cannot read %s' % path)
    a = a[..., :3][..., ::-1].astype(np.float32) / (65535.0 if a.dtype == np.uint16 else 255.0)
    return cv2.resize(a, (a.shape[1] // SCALE, a.shape[0] // SCALE), interpolation=cv2.INTER_AREA)


class Renderer:
    def __init__(self, env):
        self.env = env
        self.cache = os.path.join(LRREF, '.lrcompare_cache')
        os.makedirs(self.cache, exist_ok=True)

    def dump(self, case):
        wkg = P(case['wkg']) if 'wkg' in case else os.path.join(
            LRREF, os.path.splitext(os.path.basename(case['raw']))[0] + '.wkg')
        if not os.path.exists(wkg):
            if 'raw' not in case:
                sys.exit('%s: no dump and no raw to make one from' % case['name'])
            subprocess.run([WINNOW, '--dumpworking', P(case['raw']), wkg], check=True)
        return wkg

    def render(self, wkg, recipe, full=False):
        args = ['%s=%s' % (k, v) for k, v in sorted(recipe.items())]
        key = hashlib.md5(json.dumps([wkg, args, sorted(self.env.items()),
                                      os.path.getmtime(TOOL)]).encode()).hexdigest()
        tif = os.path.join(self.cache, key + '.tif')
        npy = os.path.join(self.cache, key + '.npy')
        if full:
            if not os.path.exists(tif):
                self._run(wkg, args, tif)
            return tif
        if os.path.exists(npy):
            return np.load(npy)
        self._run(wkg, args, tif)
        a = read16(tif)
        np.save(npy, a)
        return a

    def _run(self, wkg, args, out):
        env = dict(os.environ)
        env.update(self.env)
        r = subprocess.run([TOOL, wkg, out] + args, env=env, capture_output=True, text=True)
        if r.returncode != 0:
            sys.exit('developrender failed: %s' % r.stderr)


def align(tool, ref):
    """Integer offset (1/4-res px) of the Lightroom frame inside the Winnow render."""
    H, W = ref.shape[:2]
    best = None
    for dy in range(0, tool.shape[0] - H + 1):
        for dx in range(0, tool.shape[1] - W + 1):
            d = np.abs(lum(tool[dy:dy + H:4, dx:dx + W:4]) - lum(ref[::4, ::4])).mean()
            if best is None or d < best[0]:
                best = (d, dy, dx)
    return best[1], best[2]


def register(tool, ref, key, cache):
    """Homography taking Lightroom pixels to Winnow render pixels (1/4 res), refined by ECC
    on log luminance from the integer offset -- Lightroom applies the lens profile's
    distortion correction, which no offset can match. Cached per (render, export)."""
    f = os.path.join(cache, 'reg_' + key + '.npy')
    if os.path.exists(f):
        return np.load(f)
    dy, dx = align(tool, ref)
    M = np.array([[1, 0, dx], [0, 1, dy], [0, 0, 1]], np.float32)
    a = np.log2(np.maximum(lum(ref), 1e-4)).astype(np.float32)
    b = np.log2(np.maximum(lum(tool), 1e-4)).astype(np.float32)
    for lvl in (4, 2, 1):
        sa = cv2.resize(a, None, fx=1 / lvl, fy=1 / lvl, interpolation=cv2.INTER_AREA)
        sb = cv2.resize(b, None, fx=1 / lvl, fy=1 / lvl, interpolation=cv2.INTER_AREA)
        S = np.diag([lvl, lvl, 1]).astype(np.float32)
        m = np.linalg.inv(S) @ M @ S
        try:
            _, m = cv2.findTransformECC(sa, sb, m.astype(np.float32), cv2.MOTION_HOMOGRAPHY,
                                        (cv2.TERM_CRITERIA_EPS | cv2.TERM_CRITERIA_COUNT, 100, 1e-6),
                                        None, 5)
            M = S @ m @ np.linalg.inv(S)
        except cv2.error:
            pass
    np.save(f, M)
    return M


def flowfield(tool, ref, M, key, cache):
    """What the homography leaves -- the radial part of the lens correction -- as a
    smooth dense flow (DIS optical flow on log luminance, Gaussian-smoothed so it carries
    geometry, not content). Returns absolute sampling maps into the Winnow render."""
    f = os.path.join(cache, 'flow_' + key + '.npy')
    H, W = ref.shape[:2]
    if os.path.exists(f):
        fl = np.load(f)
    else:
        a = np.log2(np.maximum(lum(ref), 1e-4))
        b = np.log2(np.maximum(lum(warp_h(tool, M, ref.shape)), 1e-4))
        b = np.nan_to_num(b, nan=float(np.nanmean(b)))
        u8 = lambda x: np.clip((x + 12) * 20, 0, 255).astype(np.uint8)
        dis = cv2.DISOpticalFlow_create(cv2.DISOPTICAL_FLOW_PRESET_MEDIUM)
        fl = dis.calc(u8(a), u8(b), None)
        for k in range(2):
            fl[..., k] = cv2.GaussianBlur(fl[..., k], (0, 0), 25)
        np.save(f, fl)
    gx, gy = np.meshgrid(np.arange(W, dtype=np.float32), np.arange(H, dtype=np.float32))
    x, y = gx + fl[..., 0], gy + fl[..., 1]           # points in the homography-warped frame
    Mi = M.astype(np.float64)
    den = Mi[2, 0] * x + Mi[2, 1] * y + Mi[2, 2]
    mx = (Mi[0, 0] * x + Mi[0, 1] * y + Mi[0, 2]) / den
    my = (Mi[1, 0] * x + Mi[1, 1] * y + Mi[1, 2]) / den
    return mx.astype(np.float32), my.astype(np.float32)


def warp_h(img, M, shape):
    return cv2.warpPerspective(img, M.astype(np.float64), (shape[1], shape[0]),
                               flags=cv2.INTER_LINEAR | cv2.WARP_INVERSE_MAP,
                               borderMode=cv2.BORDER_CONSTANT, borderValue=np.nan)


def warp(img, maps):
    return cv2.remap(img, maps[0], maps[1], cv2.INTER_LINEAR,
                     borderMode=cv2.BORDER_CONSTANT, borderValue=np.nan)


def score_case(case, rnd):
    wkg = rnd.dump(case)
    l0, l1 = read16(P(case['lr_baseline'])), read16(P(case['lr_target']))
    t0, t1 = rnd.render(wkg, case['baseline']), rnd.render(wkg, case['target'])
    key = hashlib.md5(json.dumps([wkg, P(case['lr_baseline']),
                                  os.path.getmtime(P(case['lr_baseline']))]).encode()).hexdigest()
    M = register(t0, l0, key, rnd.cache)
    maps = flowfield(t0, l0, M, key, rnd.cache)
    t0, t1 = warp(t0, maps), warp(t1, maps)
    pw, pl = (lum(t0) - lum(t1)) * 255, (lum(l0) - lum(l1)) * 255
    ok = np.isfinite(pw)
    ok[:8], ok[-8:], ok[:, :8], ok[:, -8:] = False, False, False, False
    err = np.where(ok, pw - pl, 0)
    r = {'rms': float(np.sqrt((err[ok] ** 2).mean())), 'bias': float(err[ok].mean()), 'maps': maps}
    if case.get('halo'):
        y = lum(l0).astype(np.float32)
        dark = (cv2.GaussianBlur(y, (0, 0), 2) < 0.05).astype(np.uint8)
        dist = cv2.distanceTransform(1 - dark, cv2.DIST_L2, 5)
        bright = (y > 0.3) & ok
        near, far = bright & (dist >= 2) & (dist <= 10), bright & (dist >= 40)
        r['near'], r['far'] = float(err[near].mean()), float(err[far].mean())
        r['bands'] = [float(err[(y >= lo) & (y < hi) & ok].mean())
                      for lo, hi in ((0.55, 0.75), (0.75, 0.9), (0.9, 1.01))]
    return r


def sheet(cases, rnd, out):
    rows = []
    for case in cases:
        if 'crop' not in case:
            continue
        y0, y1, x0, x1 = case['crop']
        wkg = rnd.dump(case)
        lr = cv2.imread(P(case['lr_target']), cv2.IMREAD_UNCHANGED)[..., :3]
        wn = cv2.imread(rnd.render(wkg, case['target'], full=True), cv2.IMREAD_UNCHANGED)[..., :3]
        r = score_case(case, rnd)
        mx, my = (cv2.resize(m, (lr.shape[1], lr.shape[0])) * SCALE for m in r['maps'])
        wn = cv2.remap(wn, mx, my, cv2.INTER_LINEAR)
        a = (lr[y0:y1, x0:x1] >> 8).astype(np.uint8)
        b = (wn[y0:y1, x0:x1] >> 8).astype(np.uint8)
        for im, label in ((a, 'Lightroom ' + case['name']), (b, 'Winnow')):
            cv2.putText(im, label, (12, 40), cv2.FONT_HERSHEY_SIMPLEX, 1.2, (255, 255, 255), 3)
        rows.append(np.concatenate([a, b], 1))
    if rows:
        w = max(r.shape[1] for r in rows)
        rows = [np.pad(r, ((0, 0), (0, w - r.shape[1]), (0, 0))) for r in rows]
        img = np.concatenate(rows, 0)
        cv2.imwrite(out, cv2.resize(img, None, fx=0.5, fy=0.5, interpolation=cv2.INTER_AREA),
                    [cv2.IMWRITE_JPEG_QUALITY, 90])
        print('sheet:', out)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--cases', default=os.path.join(os.path.dirname(__file__), 'cases.json'))
    ap.add_argument('--only', nargs='*')
    ap.add_argument('--sheet')
    ap.add_argument('--env', nargs='*', default=[])
    a = ap.parse_args()
    cases = json.load(open(a.cases))
    if a.only:
        cases = [c for c in cases if c['name'] in a.only]
    rnd = Renderer(dict(kv.split('=', 1) for kv in a.env))
    total = []
    for case in cases:
        r = score_case(case, rnd)
        total.append(r['rms'])
        line = '%-24s rms %6.2f  bias %+6.2f' % (case['name'], r['rms'], r['bias'])
        if 'near' in r:
            line += '  halo near %+6.1f far %+6.1f  bright bands %s' % (
                r['near'], r['far'], ' '.join('%+.1f' % b for b in r['bands']))
        print(line, flush=True)
    print('%-24s rms %6.2f' % ('MEAN', float(np.mean(total))))
    if a.sheet:
        sheet(cases, rnd, a.sheet)


if __name__ == '__main__':
    main()
