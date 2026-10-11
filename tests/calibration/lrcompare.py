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
in $LRREF/.lrcompare_cache (delete it after changing Develop code -- the key does not
include the binary).
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


def score_case(case, rnd):
    wkg = rnd.dump(case)
    l0, l1 = read16(P(case['lr_baseline'])), read16(P(case['lr_target']))
    t0, t1 = rnd.render(wkg, case['baseline']), rnd.render(wkg, case['target'])
    dy, dx = align(t0, l0)
    H, W = l0.shape[:2]
    t0, t1 = t0[dy:dy + H, dx:dx + W], t1[dy:dy + H, dx:dx + W]
    pw, pl = (lum(t0) - lum(t1)) * 255, (lum(l0) - lum(l1)) * 255
    err = pw - pl
    r = {'rms': float(np.sqrt((err ** 2).mean())), 'bias': float(err.mean()),
         'offset': (dy * SCALE, dx * SCALE)}
    if case.get('halo'):
        y = lum(l0).astype(np.float32)
        dark = (cv2.GaussianBlur(y, (0, 0), 2) < 0.05).astype(np.uint8)
        dist = cv2.distanceTransform(1 - dark, cv2.DIST_L2, 5)
        bright = y > 0.3
        near, far = bright & (dist >= 2) & (dist <= 10), bright & (dist >= 40)
        r['near'], r['far'] = float(err[near].mean()), float(err[far].mean())
        r['bands'] = [float(err[(y >= lo) & (y < hi)].mean())
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
        oy, ox = r['offset']
        a = (lr[y0:y1, x0:x1] >> 8).astype(np.uint8)
        b = (wn[oy + y0:oy + y1, ox + x0:ox + x1] >> 8).astype(np.uint8)
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
