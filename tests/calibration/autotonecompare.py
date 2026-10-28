#!/usr/bin/env python3
"""
autotonecompare.py -- measure Winnow's Auto tone (Develop/autotone.cpp) against Lightroom's.

Lightroom writes its Auto results into each export's XMP (crs:Exposure2012 ...), so a set
of LR exports of auto-toned images is a labelled set. For each export:

  1. The original is found from the export's crs:RawFileName (Spotlight, cached in
     <cache>/sources.json; --source NAME=PATH overrides) and dumped once with
     `Winnow --dumpworking` (raws, and JPEG / HEIC as Develop builds them).
  2. `developrender <wkg> <out> autotone long=N` runs AutoTone::Solve on the dump and
     renders with its values; a second render with no edits is the neutral baseline.
  3. Comparisons:
       SLIDERS  Winnow's eight values against Lightroom's.
       TONE     what each picture LOOKS like, as whole-frame statistics of the LR export,
                the Winnow Auto render and the Winnow neutral render: display-luma
                percentiles (levels of 255), luma std. dev. and mid-tone CIELAB chroma.
                Histogram statistics need no registration, so LR's lens correction and
                any crop do not matter.
       SPLIT    a third render, Winnow with LIGHTROOM'S eight values, splits the tone
                error in two: LR-values vs LR is what the sliders MEAN (Winnow's slider
                scales against Lightroom's), Auto vs LR-values is what the solver
                DECIDED. Only the second is the solver's to fix.

Exports whose eight sliders are all zero were not auto-toned and are listed, not scored.

Usage:
  autotonecompare.py [--dir DIR] [--only NAME ...] [--source NAME=PATH ...] [--jobs N]
                     [--env K=V ...] [--json OUT.json] [--exclude NAME ...]

  --dir      folder of LR exports (default $LRREF/Autotone)
  --env      extra environment for developrender (dev-only tunables while fitting)
  --exclude  names scored for SLIDERS only, e.g. exports LR rendered with AI Denoise
  --json     write every per-image number for fitting scripts

Cache: <dir>/../.autotone_cache (dumps ~0.6 GB per 61 MP raw; renders keyed by the
tool's mtime and --env). Needs numpy and opencv-python.
"""
import argparse, concurrent.futures as cf, hashlib, json, os, subprocess, sys
import numpy as np
import cv2

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
TOOL = os.path.join(REPO, 'build/mac-debug/tests/developrender')
WINNOW = os.path.join(REPO, 'build/mac-debug/Winnow.app/Contents/MacOS/Winnow')
LRREF = os.path.expanduser(os.environ.get('LRREF', '~/Pictures/_LRref'))
LONG = 1200

SLIDERS = ['exposure', 'contrast', 'highlights', 'shadows', 'whites', 'blacks',
           'vibrance', 'saturation']
CRS = {'exposure': 'Exposure2012', 'contrast': 'Contrast2012',
       'highlights': 'Highlights2012', 'shadows': 'Shadows2012',
       'whites': 'Whites2012', 'blacks': 'Blacks2012', 'vibrance': 'Vibrance',
       'saturation': 'Saturation'}
PCTS = [1, 5, 25, 50, 75, 95, 99]


def lr_labels(folder):
    tags = ['-XMP-crs:%s' % t for t in CRS.values()] + [
        '-XMP-crs:RawFileName', '-XMP-crs:CameraProfile', '-XMP-crs:ProcessVersion',
        '-Model']
    out = subprocess.run(['exiftool', '-json', '-n'] + tags +
                         ['-ext', 'jpg', '-ext', 'tif', folder],
                         capture_output=True, text=True, check=True).stdout
    rows = {}
    for r in json.loads(out):
        name = os.path.splitext(os.path.basename(r['SourceFile']))[0]
        rows[name] = {
            'export': r['SourceFile'],
            'raw': r.get('RawFileName', ''),
            'model': r.get('Model', ''),
            'profile': r.get('CameraProfile', ''),
            'pv': str(r.get('ProcessVersion', '')),
            'lr': {k: float(r.get(t, 0) or 0) for k, t in CRS.items()},
        }
    return rows


def find_source(raw, cache, overrides):
    if raw in overrides: return overrides[raw]
    if raw in cache and os.path.exists(cache[raw]): return cache[raw]
    hits = subprocess.run(['mdfind', '-name', raw], capture_output=True,
                          text=True).stdout.splitlines()
    hits = [h for h in hits if os.path.basename(h).lower() == raw.lower()
            and '/_LRref/' not in h]
    if not hits: return None
    cache[raw] = sorted(hits, key=len)[0]   # shortest = least likely a stray copy
    return cache[raw]


def dump(src, wkg):
    if os.path.exists(wkg) and os.path.getmtime(wkg) >= os.path.getmtime(src):
        return True
    r = subprocess.run([WINNOW, '--dumpworking', src, wkg + '.part'],
                       capture_output=True, text=True)
    if r.returncode != 0 or not os.path.exists(wkg + '.part'):
        sys.stderr.write('dump failed: %s\n%s%s' % (src, r.stdout, r.stderr))
        return False
    os.replace(wkg + '.part', wkg)
    return True


def render(wkg, out, args, env):
    """developrender; returns its stdout. Cached by tool mtime + args + env."""
    meta = out + '.txt'
    key = '%s|%s|%s' % (os.path.getmtime(TOOL), ' '.join(args), sorted(env.items()))
    if os.path.exists(out) and os.path.exists(meta):
        with open(meta) as f:
            k, _, text = f.read().partition('\n')
        if k == key: return text
    e = dict(os.environ); e.update(env)
    r = subprocess.run([TOOL, wkg, out] + args, capture_output=True, text=True, env=e)
    if r.returncode != 0:
        raise RuntimeError('developrender failed: %s %s\n%s' % (wkg, args, r.stderr))
    with open(meta, 'w') as f:
        f.write(key + '\n' + r.stdout)
    return r.stdout


def parse_auto(text):
    for line in text.splitlines():
        if line.startswith('autotone '):
            kv = dict(t.split('=') for t in line.split()[1:])
            return {k: float(kv[k]) for k in SLIDERS}
    raise RuntimeError('no autotone line in developrender output')


def tone_stats(path):
    """Whole-frame display statistics of an sRGB image, at LONG px."""
    im = cv2.imread(path, cv2.IMREAD_UNCHANGED | cv2.IMREAD_ANYDEPTH)
    if im is None: raise RuntimeError('cannot read ' + path)
    if im.ndim == 2: im = cv2.cvtColor(im, cv2.COLOR_GRAY2BGR)
    im = im[:, :, :3]
    s = LONG / max(im.shape[:2])
    if s < 1: im = cv2.resize(im, None, fx=s, fy=s, interpolation=cv2.INTER_AREA)
    f = im.astype(np.float32) / (65535.0 if im.dtype == np.uint16 else 255.0)
    b, g, r = f[:, :, 0], f[:, :, 1], f[:, :, 2]
    luma = (r * 54 + g * 183 + b * 19) / 256.0 * 255.0     # the histogram scope's luma
    lab = cv2.cvtColor(f[:, :, ::-1].copy(), cv2.COLOR_RGB2Lab)
    L = lab[:, :, 0]
    C = np.hypot(lab[:, :, 1], lab[:, :, 2])
    mid = (L > 10) & (L < 95)
    st = {'p%d' % p: float(np.percentile(luma, p)) for p in PCTS}
    st['mean'] = float(luma.mean())
    st['sd'] = float(luma.std())
    st['chroma'] = float(C[mid].mean()) if mid.any() else 0.0
    return st


def lr_args(lr):
    return ['%s=%g' % (k, lr[k]) for k in SLIDERS]


def process(name, row, cache_dir, env, sources):
    src = sources.get(row['raw'])
    base = os.path.join(cache_dir, name)
    wkg = base + '.wkg'
    if not src or not dump(src, wkg):
        return name, {'error': 'no source' if not src else 'dump failed'}
    envkey = hashlib.md5(json.dumps(sorted(env.items())).encode()).hexdigest()[:8]
    auto_tif = '%s.auto.%s.tif' % (base, envkey)
    neutral_tif = base + '.neutral.tif'
    auto = parse_auto(render(wkg, auto_tif, ['autotone', 'long=%d' % LONG], env))
    render(wkg, neutral_tif, ['long=%d' % LONG], {})
    lrv_tif = base + '.lrvalues.tif'
    render(wkg, lrv_tif, ['long=%d' % LONG] + lr_args(row['lr']), {})
    return name, {
        'source': src, 'winnow': auto,
        'tone_lr': tone_stats(row['export']),
        'tone_auto': tone_stats(auto_tif),
        'tone_neutral': tone_stats(neutral_tif),
        'tone_lrvalues': tone_stats(lrv_tif),
    }


def fmt(v, k):
    return ('%+.2f' % v) if k == 'exposure' else ('%+4.0f' % v)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--dir', default=os.path.join(LRREF, 'Autotone'))
    ap.add_argument('--only', nargs='*')
    ap.add_argument('--exclude', nargs='*', default=[])
    ap.add_argument('--source', nargs='*', default=[])
    ap.add_argument('--env', nargs='*', default=[])
    ap.add_argument('--jobs', type=int, default=6)
    ap.add_argument('--json')
    a = ap.parse_args()

    folder = os.path.expanduser(a.dir)
    cache_dir = os.path.join(os.path.dirname(folder.rstrip('/')), '.autotone_cache')
    os.makedirs(cache_dir, exist_ok=True)
    env = dict(kv.split('=', 1) for kv in a.env)
    overrides = dict(kv.split('=', 1) for kv in a.source)

    rows = lr_labels(folder)
    if a.only: rows = {k: v for k, v in rows.items() if k in a.only}
    untoned = sorted(k for k, v in rows.items() if not any(v['lr'].values()))
    rows = {k: v for k, v in rows.items() if k not in untoned}

    src_file = os.path.join(cache_dir, 'sources.json')
    srcs = json.load(open(src_file)) if os.path.exists(src_file) else {}
    sources = {r['raw']: find_source(r['raw'], srcs, overrides) for r in rows.values()}
    json.dump(srcs, open(src_file, 'w'), indent=1)

    res = {}
    with cf.ThreadPoolExecutor(a.jobs) as ex:
        futs = [ex.submit(process, n, r, cache_dir, env, sources) for n, r in rows.items()]
        for f in cf.as_completed(futs):
            n, d = f.result()
            res[n] = d
            print('  done %s' % n, file=sys.stderr)

    names = sorted(n for n in res if 'error' not in res[n])
    for n in sorted(set(res) - set(names)):
        print('SKIPPED %s: %s' % (n, res[n]['error']))
    if untoned:
        print('NOT AUTO-TONED in LR (all sliders 0), not scored: ' + ', '.join(untoned))

    # ---- sliders ----
    print('\nSLIDERS  (LR / Winnow)')
    print('%-17s' % 'image' + ''.join('%14s' % k[:10] for k in SLIDERS))
    for n in names:
        lr, w = rows[n]['lr'], res[n]['winnow']
        print('%-17s' % n + ''.join('%14s' % ('%s/%s' % (fmt(lr[k], k).strip(),
                                                        fmt(w[k], k).strip()))
                                     for k in SLIDERS))
    print('\n%-11s %8s %8s %8s %8s %8s' % ('slider', 'LR mean', 'W mean', 'bias',
                                            'MAE', 'r'))
    for k in SLIDERS:
        x = np.array([rows[n]['lr'][k] for n in names])
        y = np.array([res[n]['winnow'][k] for n in names])
        r = np.corrcoef(x, y)[0, 1] if x.std() > 0 and y.std() > 0 else float('nan')
        print('%-11s %8.2f %8.2f %+8.2f %8.2f %8.2f' % (k, x.mean(), y.mean(),
                                                        (y - x).mean(),
                                                        np.abs(y - x).mean(), r))

    # ---- tone ----
    tn = [n for n in names if n not in a.exclude]
    keys = ['p%d' % p for p in PCTS] + ['mean', 'sd', 'chroma']
    print('\nTONE  (Winnow Auto - LR Auto; luma in levels of 255, chroma in C*)')
    print('%-17s' % 'image' + ''.join('%7s' % k for k in keys))
    for n in tn:
        print('%-17s' % n + ''.join('%+7.1f' % (res[n]['tone_auto'][k] -
                                                res[n]['tone_lr'][k]) for k in keys))
    print('\n%-17s' % 'mean |diff|' + ''.join('%7s' % k for k in keys))
    for label, key, ref in (('neutral vs LR', 'tone_neutral', 'tone_lr'),
                            ('Auto vs LR', 'tone_auto', 'tone_lr'),
                            ('LRvals vs LR', 'tone_lrvalues', 'tone_lr'),
                            ('Auto vs LRvals', 'tone_auto', 'tone_lrvalues')):
        print('%-17s' % label + ''.join(
            '%7.1f' % np.mean([abs(res[n][key][k] - res[n][ref][k]) for n in tn])
            for k in keys))
    for label, key, ref in (('Auto bias', 'tone_auto', 'tone_lr'),
                            ('LRvals bias', 'tone_lrvalues', 'tone_lr')):
        print('%-17s' % label + ''.join(
            '%+7.1f' % np.mean([res[n][key][k] - res[n][ref][k] for n in tn])
            for k in keys))
    print('\nSPLIT per image, p50 / sd (levels): LRvals-LR = slider meaning, '
          'Auto-LRvals = decision')
    for n in tn:
        d = lambda a, b, k: res[n][a][k] - res[n][b][k]
        print('%-17s  meaning %+6.1f %+5.1f   decision %+6.1f %+5.1f' % (
            n, d('tone_lrvalues', 'tone_lr', 'p50'), d('tone_lrvalues', 'tone_lr', 'sd'),
            d('tone_auto', 'tone_lrvalues', 'p50'), d('tone_auto', 'tone_lrvalues', 'sd')))
    if a.exclude:
        print('(tone excludes: %s)' % ', '.join(a.exclude))

    if a.json:
        json.dump({'labels': rows, 'results': res}, open(a.json, 'w'), indent=1)


if __name__ == '__main__':
    main()
