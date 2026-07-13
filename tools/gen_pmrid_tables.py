#!/usr/bin/env python3
"""
gen_pmrid_tables.py -- derive PMRID (k, b) noise tables from published sensor data.

PMRID needs, per ISO, the Poisson-Gaussian noise model in the white-normalised [0,1]
green-plane domain used by ImageFormats/Raw/pmrid.cpp:

    var(x) = k * x + b        x = (DN - black) / (white - black),  x in [0, 1]

Given a sensor characterisation (photonstophotos.net / Bill Claff publishes read noise in
both electrons and DN, plus saturation), the conversion is:

    gain_e_per_dn = read_noise_e / read_noise_dn      # electrons per DN at this ISO
    Wn            = saturation_dn - black_dn           # black-subtracted full scale (DN)
    k = 1 / (gain_e_per_dn * Wn) = read_noise_dn / (read_noise_e * Wn)
    b = (read_noise_dn / Wn)^2

Derivation: shot-noise variance in DN is D/gain_e_per_dn (D = signal in DN above black);
read noise adds read_noise_dn^2. Normalising by Wn^2 gives the two terms above.

INPUT  CSV per camera in tools/claff/ (one row per ISO). First non-blank line names the
       model; then a header row; then rows. Columns (order-independent, by header name):
           iso, read_noise_e, read_noise_dn, saturation_dn[, black_dn]
       black_dn is optional (default 0). Lines starting with '#' are comments.
       Example first line:  # model: ILCE-7RM5

OUTPUT C++ for ImageFormats/Raw/pmrid_calib_tables.h (stdout): a constexpr IsoKB array per
       camera plus its kCalibTables registry row. Paste the arrays into the DERIVED block
       and the rows into the DERIVED-REGISTRY block.

VALIDATION When _pmrid_out/calib_<model>.json (a MEASURED sweep) exists for a model, the
       derived (k, b) are compared against it per ISO and the ratio printed to stderr -- the
       trust check before deriving OTHER bodies (Sony A7R5 / A1 II are both on photonstophotos).
"""

import argparse
import csv
import glob
import json
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))


def cident(model):
    """C++ identifier for a model, e.g. ILCE-7RM5 -> kClaff_ILCE_7RM5."""
    return "kClaff_" + re.sub(r"[^A-Za-z0-9]", "_", model)


def read_csv(path):
    """Parse one camera CSV -> (model, [ {iso, k, b}, ... ] sorted by iso)."""
    model = None
    rows = []
    with open(path, newline="") as fp:
        lines = [ln for ln in fp]
    # Model from a "# model: X" line (fall back to filename stem).
    for ln in lines:
        m = re.match(r"\s*#\s*model\s*:\s*(\S+)", ln, re.I)
        if m:
            model = m.group(1)
            break
    if model is None:
        model = os.path.splitext(os.path.basename(path))[0]
    data = [ln for ln in lines if ln.strip() and not ln.lstrip().startswith("#")]
    if not data:
        raise ValueError(f"{path}: no data rows")
    reader = csv.DictReader(data)
    for r in reader:
        r = {k.strip().lower(): (v or "").strip() for k, v in r.items()}
        iso = int(float(r["iso"]))
        rn_e = float(r["read_noise_e"])
        rn_dn = float(r["read_noise_dn"])
        sat = float(r["saturation_dn"])
        black = float(r.get("black_dn") or 0.0)
        wn = sat - black
        if wn <= 0 or rn_e <= 0 or rn_dn <= 0:
            raise ValueError(f"{path}: bad row iso={iso} (Wn/read noise must be > 0)")
        k = rn_dn / (rn_e * wn)
        b = (rn_dn / wn) ** 2
        rows.append({"iso": iso, "k": k, "b": b})
    rows.sort(key=lambda r: r["iso"])
    return model, rows


def emit_cpp(model, rows):
    """C++ constexpr IsoKB array text for one camera."""
    name = cident(model)
    out = [f"constexpr IsoKB {name}[] = {{   // {model} (derived, photonstophotos)"]
    for r in rows:
        out.append(f"    {{ {r['iso']:6d}, {r['k']:.6e}, {r['b']:.6e} }},")
    out.append("};")
    return "\n".join(out), name


def validate(model, rows):
    """Compare derived (k,b) to a MEASURED calib_<model>.json, if present. Prints to stderr."""
    js = os.path.join(HERE, "_pmrid_out", f"calib_{model}.json")
    if not os.path.exists(js):
        return
    with open(js) as fp:
        meas = {int(r[0]): (r[1], r[2]) for r in json.load(fp)["iso_k_b"]}
    print(f"\n[validate] {model}: derived vs measured (k ratio, b ratio)", file=sys.stderr)
    for r in rows:
        if r["iso"] not in meas:
            continue
        mk, mb = meas[r["iso"]]
        kr = r["k"] / mk if mk else float("nan")
        br = r["b"] / mb if mb else float("nan")
        print(f"  iso {r['iso']:6d}:  k x{kr:5.2f}   b x{br:7.2f}", file=sys.stderr)
    print("  (k ratio near 1.0 => units/mapping correct; b is noisier)", file=sys.stderr)


def main():
    ap = argparse.ArgumentParser(description="Derive PMRID (k,b) tables from sensor data.")
    ap.add_argument("csv", nargs="*", help="camera CSV(s); default tools/claff/*.csv")
    args = ap.parse_args()

    paths = args.csv or sorted(glob.glob(os.path.join(HERE, "claff", "*.csv")))
    paths = [p for p in paths if os.path.basename(p).upper() != "EXAMPLE.CSV"]
    if not paths:
        print("no CSVs (put photonstophotos data in tools/claff/, see README.md)",
              file=sys.stderr)
        return 1

    arrays, regrows = [], []
    for p in paths:
        model, rows = read_csv(p)
        cpp, name = emit_cpp(model, rows)
        arrays.append(cpp)
        regrows.append(f'    {{ "{model}", {name}, {len(rows)} }},')
        validate(model, rows)

    print("/* ---- paste into pmrid_calib_tables.h DERIVED block ---- */")
    print("\n".join(arrays))
    print("\n/* ---- paste into pmrid_calib_tables.h DERIVED-REGISTRY block ---- */")
    print("\n".join(regrows))
    return 0


if __name__ == "__main__":
    sys.exit(main())
