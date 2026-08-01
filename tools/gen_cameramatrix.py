#!/usr/bin/env python3
"""
Regenerate ImageFormats/Raw/cameramatrix.cpp from libraw's adobe_coeff table.

cameramatrix.cpp holds the per-model XYZ(D65)->camera colour matrices RawColor needs.
It is a verbatim port of libraw's adobe_coeff table (src/tables/colordata.cpp), filtered
to the mainstream still-camera makers and to 3-colour (Bayer) sensors. Re-run this whenever
you bump the bundled libraw / want newly added camera bodies: it rebuilds the WHOLE table,
so new libraw entries and matrix corrections are picked up automatically.

Usage:
    python3 tools/gen_cameramatrix.py                 # fetch libraw master and regenerate
    python3 tools/gen_cameramatrix.py --ref 0.22.1    # pin a libraw tag/branch/commit
    python3 tools/gen_cameramatrix.py --source path/to/colordata.cpp   # use a local copy
    python3 tools/gen_cameramatrix.py --check         # regenerate to stdout, don't write

Only the 9-integer (3x3) matrix is emitted; black/white levels, white balance and CFA phase
are read from each file at decode time, not stored here. 4-colour (CMYG/RGBE) sensors and
phone/action/cinema/bare-sensor makers are skipped (Winnow's Bayer pipeline cannot use them).
"""

import argparse
import os
import re
import sys
import urllib.request

LIBRAW_URL = (
    "https://raw.githubusercontent.com/LibRaw/LibRaw/{ref}/src/tables/colordata.cpp"
)

# libraw maker enum (LIBRAW_CAMERAMAKER_<X>) -> key prefix. Matching is case-insensitive, so
# natural case is fine; the prefix must be the word Winnow's ImageMetadata::model begins with
# for that maker (e.g. Sony models are "Sony ILCE-9M2", Nikon are "NIKON D850").
PREFIX = {
    "Agfa": "Agfa", "Canon": "Canon", "Casio": "Casio", "Contax": "Contax", "DXO": "DxO",
    "Epson": "Epson", "Fujifilm": "Fujifilm", "Hasselblad": "Hasselblad", "Imacon": "Imacon",
    "Kodak": "Kodak", "Leaf": "Leaf", "Leica": "Leica", "Mamiya": "Mamiya", "Minolta": "Minolta",
    "Nikon": "Nikon", "Olympus": "Olympus", "Panasonic": "Panasonic", "Pentax": "Pentax",
    "PhaseOne": "Phase One", "Polaroid": "Polaroid", "Ricoh": "Ricoh", "Samsung": "Samsung",
    "Sigma": "Sigma", "Sinar": "Sinar", "Sony": "Sony",
}

# Display order / section grouping for the emitted table.
ORDER = [
    "Canon", "Nikon", "Sony", "Fujifilm", "Olympus", "Panasonic", "Pentax", "Ricoh", "Leica",
    "Sigma", "Samsung", "Hasselblad", "PhaseOne", "Leaf", "Mamiya", "Sinar", "Imacon", "Kodak",
    "Minolta", "Casio", "Contax", "Epson", "Agfa", "Polaroid", "DXO",
]
DISPLAY = {"PhaseOne": "Phase One", "DXO": "DxO"}

# { LIBRAW_CAMERAMAKER_<Maker>, "model", t_black, t_maximum, { ints... } }
ENTRY = re.compile(
    r'\{\s*LIBRAW_CAMERAMAKER_(\w+)\s*,\s*"([^"]*)"\s*,'
    r'\s*([^,{]+?)\s*,\s*([^,{]+?)\s*,\s*\{\s*([-0-9,\s]+?)\s*\}',
    re.DOTALL,
)

HEADER = '''#include "ImageFormats/Raw/cameramatrix.h"
#include <cstring>

/*
    Model -> XYZ(D65)->camera matrix, stored as nine integers = matrix * 10000, exactly as in
    libraw/dcraw's adobe_coeff table. This is a full port of that table for the mainstream
    still-camera makers (interchangeable-lens and compact raw cameras); phone, action-cam,
    cinema and bare-sensor entries are omitted, as are 4-colour (CMYG/RGBE) sensors Winnow's
    Bayer pipeline cannot demosaic. Each row also carries libraw's adobe_coeff t_black and
    t_maximum (the per-model black pedestal and sensor saturation), used as a fallback white/black
    by decoders that otherwise only guess (e.g. Canon, whose saturation is below the bit-depth
    max). White balance and CFA phase are still read from each file at decode time.

    Lookup is a case-insensitive LONGEST-PREFIX match: the table key is the maker-prefixed model,
    and the camera's model string matches the longest key that is a prefix of it. This mirrors
    libraw's prefix scheme (one "D800" row covers D800/D800E; "ILCE-9" covers ILCE-9/9M2) while
    being order-independent and picking the most specific entry, so a short key like "Nikon D3"
    never shadows "Nikon D300". xyzToCamForModel returns the matrix; cameraLevelsForModel returns
    black/maximum (0 = unset). An unmatched model returns false (caller falls back to identity /
    its own defaults).

    GENERATED FILE -- do not edit by hand. Regenerate with tools/gen_cameramatrix.py against a
    newer libraw colordata.cpp; that rebuilds the whole table so new bodies are picked up.
*/

namespace {

struct Entry {
    const char *model;      // maker-prefixed key, matched case-insensitively as a prefix
    int black;              // adobe_coeff t_black  (0 = unset; <0 = use only when file has none)
    int maximum;            // adobe_coeff t_maximum (sensor saturation; 0 = unset)
    int c[9];               // XYZ(D65)->cam, row-major, * 10000
};

const Entry kTable[] = {
'''

FOOTER = '''};

const Entry *findEntry(const QString &model)
{
    /* Case-insensitive longest-prefix match: the most specific key that prefixes the model wins. */
    const QString needle = model.trimmed();
    const Entry *best = nullptr;
    size_t bestLen = 0;
    for (const Entry &e : kTable) {
        const size_t l = std::strlen(e.model);
        if (l > bestLen && needle.startsWith(QLatin1String(e.model), Qt::CaseInsensitive)) {
            best = &e;
            bestLen = l;
        }
    }
    return best;
}

} // namespace

bool xyzToCamForModel(const QString &model, float m[3][3])
{
    const Entry *best = findEntry(model);
    if (!best) return false;
    for (int i = 0; i < 9; ++i)
        m[i / 3][i % 3] = best->c[i] / 10000.0f;
    return true;
}

bool cameraLevelsForModel(const QString &model, int &black, int &maximum)
{
    const Entry *best = findEntry(model);
    if (!best) return false;
    black = best->black;
    maximum = best->maximum;
    return true;
}
'''


def parse(src):
    """Return ordered list of (maker_enum, key, [9 ints]) from a colordata.cpp source string."""
    # Strip comments so inline "// same CMs:" notes between the scalars and the matrix brace
    # (and trailing /* DJC */ provenance tags) don't defeat the entry regex.
    src = re.sub(r"/\*.*?\*/", "", src, flags=re.DOTALL)
    src = re.sub(r"//[^\n]*", "", src)

    def num(s):
        try:
            return int(s.strip(), 0)             # handles decimal, 0x hex and negatives
        except ValueError:
            return 0                             # non-literal (expression) -> treat as unset

    seen = {}
    rows = []
    skipped_4colour = 0
    for m in ENTRY.finditer(src):
        maker, model = m.group(1), m.group(2).strip()
        black, maximum, ints = num(m.group(3)), num(m.group(4)), m.group(5)
        if maker not in PREFIX:
            continue
        vals = [int(x) for x in ints.split(",") if x.strip()]
        if len(vals) != 9:                       # 4-colour (CMYG/RGBE) sensor -> not usable
            skipped_4colour += 1
            continue
        key = f"{PREFIX[maker]} {model}"
        if '"' in key or "\\" in key:            # would break the C string literal
            raise SystemExit(f"model needs escaping, refusing: {key!r}")
        kl = key.lower()
        if kl in seen:
            if seen[kl] != (black, maximum, vals):
                sys.stderr.write(f"warning: conflicting data for {key!r}, keeping first\n")
            continue
        seen[kl] = (black, maximum, vals)
        rows.append((maker, key, black, maximum, vals))
    return rows, skipped_4colour


def emit(rows):
    out = [HEADER]
    by = {}
    for maker, key, black, maximum, vals in rows:
        by.setdefault(maker, []).append((key, black, maximum, vals))
    for maker in ORDER:
        items = by.get(maker)
        if not items:
            continue
        items.sort(key=lambda it: it[0].lower())
        out.append(f"    /* {DISPLAY.get(maker, maker)} ({len(items)}) */\n")
        for key, black, maximum, vals in items:
            m = ", ".join(str(v) for v in vals)
            out.append(f'    {{ "{key}", {black}, {maximum}, {{ {m} }} }},\n')
        out.append("\n")
    for maker in by:
        if maker not in ORDER:
            sys.stderr.write(f"warning: maker {maker!r} not in ORDER, omitted\n")
    out.append(FOOTER)
    return "".join(out)


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--ref", default="master",
                    help="libraw git ref to fetch (tag/branch/commit; default master)")
    ap.add_argument("--source", help="path to a local colordata.cpp instead of fetching")
    ap.add_argument("--check", action="store_true",
                    help="write the generated file to stdout instead of cameramatrix.cpp")
    args = ap.parse_args()

    if args.source:
        with open(args.source, encoding="utf-8") as f:
            src = f.read()
    else:
        url = LIBRAW_URL.format(ref=args.ref)
        sys.stderr.write(f"fetching {url}\n")
        with urllib.request.urlopen(url, timeout=60) as r:
            src = r.read().decode("utf-8")

    rows, skipped = parse(src)
    sys.stderr.write(f"parsed {len(rows)} models (skipped {skipped} 4-colour sensors)\n")
    text = emit(rows)

    if args.check:
        sys.stdout.write(text)
        return

    repo_root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    dest = os.path.join(repo_root, "ImageFormats", "Raw", "cameramatrix.cpp")
    with open(dest, "w", encoding="utf-8") as f:
        f.write(text)
    sys.stderr.write(f"wrote {dest}\n")


if __name__ == "__main__":
    main()
