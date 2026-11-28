#!/usr/bin/env python3
"""
Generate ImageFormats/Raw/cameramatrixa.cpp: the StdA (tungsten, 2856 K) XYZ->camera
matrix for each body, the second illuminant Winnow Standard needs to interpolate between
two calibrations the way a DNG profile does (greycard trial C1, notes/GreycardPlan.md).

Source: rawler's camera definitions (dnglab, rawler/data/cameras/<maker>/*.toml,
LGPL-2.1), which carry Adobe's ColorMatrix for illuminant A and for D65.

KEYED BY THE LIBRAW KEY, NOT BY A NEW NAME. cameramatrix.cpp (libraw's adobe_coeff) is the
D65 table every render already uses, looked up by longest prefix. An A matrix is only
useful as the partner of THAT D65 matrix, so each rawler body is paired with the libraw
entry its name resolves to, and kept only when rawler's D65 equals libraw's (to the
table's 1/10000 rounding): then both matrices are Adobe's for the same body and version.
A body whose D65 differs is dropped and reported -- interpolating towards an A matrix
made for a different D65 would be worse than not interpolating.

Usage:
    python3 tools/gen_cameramatrix_a.py --rawler <dnglab checkout>   # write the table
    python3 tools/gen_cameramatrix_a.py --rawler <checkout> --check  # print, don't write

    A sparse checkout is enough:
      git clone --depth 1 --filter=blob:none --sparse https://github.com/dnglab/dnglab.git
      cd dnglab && git sparse-checkout set rawler/data/cameras
"""

import argparse
import glob
import os
import re
import subprocess
import sys
import tomllib

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
LIBRAW_TABLE = os.path.join(REPO, "ImageFormats/Raw/cameramatrix.cpp")
OUT = os.path.join(REPO, "ImageFormats/Raw/cameramatrixa.cpp")

# rawler clean_make -> the maker word cameramatrix.cpp's keys begin with.
MAKER = {
    "Canon": "Canon", "Nikon": "Nikon", "Sony": "Sony", "Fujifilm": "Fujifilm",
    "Olympus": "Olympus", "OM Digital Solutions": "Olympus", "OM System": "Olympus",
    "Panasonic": "Panasonic", "Pentax": "Pentax", "Ricoh": "Ricoh", "Leica": "Leica",
    "Sigma": "Sigma", "Samsung": "Samsung", "Hasselblad": "Hasselblad",
    "Phase One": "Phase One", "Leaf": "Leaf", "Mamiya": "Mamiya", "Minolta": "Minolta",
    "Konica Minolta": "Minolta", "Kodak": "Kodak", "Epson": "Epson", "Sinar": "Sinar",
}

ENTRY = re.compile(r'\{\s*"([^"]+)"\s*,\s*-?\d+\s*,\s*-?\d+\s*,\s*\{([-0-9,\s]+)\}\s*\}')


def libraw_entries():
    text = open(LIBRAW_TABLE).read()
    return [(m.group(1), [int(x) for x in m.group(2).split(",")]) for m in ENTRY.finditer(text)]


def find_entry(entries, model):
    """cameramatrix.cpp's findEntry: case-insensitive longest prefix."""
    best, best_len = None, 0
    low = model.strip().lower()
    for key, c in entries:
        if len(key) > best_len and low.startswith(key.lower()):
            best, best_len = (key, c), len(key)
    return best


def ints(m):
    return [int(round(v * 10000)) for v in m]


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[1])
    ap.add_argument("--rawler", required=True, help="dnglab checkout (or rawler/data/cameras)")
    ap.add_argument("--check", action="store_true", help="print to stdout, do not write")
    a = ap.parse_args()

    root = a.rawler
    if os.path.isdir(os.path.join(root, "rawler/data/cameras")):
        root = os.path.join(root, "rawler/data/cameras")
    try:
        commit = subprocess.run(["git", "-C", a.rawler, "rev-parse", "HEAD"],
                                capture_output=True, text=True).stdout.strip() or "unknown"
    except OSError:
        commit = "unknown"

    entries = libraw_entries()
    table = {}          # libraw key -> A ints
    dropped, unmatched, conflict = [], [], []
    for path in sorted(glob.glob(os.path.join(root, "*", "*.toml"))):
        d = tomllib.load(open(path, "rb"))
        cm = (d.get("cameras") or {}).get("color_matrix") or {}
        if "A" not in cm or "D65" not in cm:
            continue
        maker = MAKER.get(d.get("clean_make", ""))
        if not maker:
            continue
        names = [d.get("clean_model", "")] + [al[1] if isinstance(al, list) else str(al)
                                             for al in d.get("model_aliases", [])]
        a_ints, d65_ints = ints(cm["A"]), ints(cm["D65"])
        for name in names:
            hit = find_entry(entries, f"{maker} {name}")
            if not hit:
                unmatched.append(f"{maker} {name}")
                continue
            key, libraw_d65 = hit
            if max(abs(x - y) for x, y in zip(libraw_d65, d65_ints)) > 1:
                dropped.append(f"{maker} {name} -> {key}")
                continue
            if key in table and table[key] != a_ints:
                conflict.append(key)
                continue
            table[key] = a_ints

    for key in conflict:            # two rawler bodies disagree on one libraw key: drop it
        table.pop(key, None)

    rows = [f'    {{ "{k}", {{ {", ".join(str(v) for v in table[k])} }} }},'
            for k in sorted(table, key=str.lower)]
    out = TEMPLATE.format(commit=commit, count=len(rows), rows="\n".join(rows))
    if a.check:
        sys.stdout.write(out)
    else:
        open(OUT, "w").write(out)
    print(f"{len(rows)} bodies; {len(dropped)} dropped (D65 differs from libraw's); "
          f"{len(set(unmatched))} names with no libraw key; {len(set(conflict))} conflicts",
          file=sys.stderr)


TEMPLATE = '''#include "ImageFormats/Raw/cameramatrix.h"
#include <cstring>

/*
    Model -> XYZ->camera matrix for illuminant A (StdA, 2856 K), * 10000 -- the warm
    partner of cameramatrix.cpp's D65 matrix, for the greycard trial "dual-illuminant
    Winnow Standard" (C1 in notes/GreycardPlan.md).

    From rawler's camera definitions (dnglab, rawler/data/cameras, LGPL-2.1; Adobe's
    ColorMatrix values), commit {commit}. Keyed by the cameramatrix.cpp key each body
    resolves to, and kept only where rawler's D65 matrix equals that entry's, so the two
    matrices always belong together. {count} bodies.

    GENERATED FILE -- do not edit by hand. Regenerate with tools/gen_cameramatrix_a.py,
    and again whenever cameramatrix.cpp is regenerated (a changed D65 entry drops its
    partner until rawler carries the same version).
*/

namespace {{

struct EntryA {{
    const char *model;      // exactly a cameramatrix.cpp key
    int c[9];               // XYZ(A)->cam, row-major, * 10000
}};

const EntryA kTableA[] = {{
{rows}
}};

}} // namespace

bool xyzToCamStdAForModel(const QString &model, float m[3][3])
{{
    const char *key = cameraMatrixKey(model);
    if (!key) return false;
    for (const EntryA &e : kTableA) {{
        if (std::strcmp(e.model, key) != 0) continue;
        for (int i = 0; i < 9; ++i)
            m[i / 3][i % 3] = e.c[i] / 10000.0f;
        return true;
    }}
    return false;
}}
'''

if __name__ == "__main__":
    main()
