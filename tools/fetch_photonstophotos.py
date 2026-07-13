#!/usr/bin/env python3
"""
fetch_photonstophotos.py -- scrape photonstophotos.net read-noise data into PMRID tables.

Bill Claff's "Read Noise" charts embed all data inline as Highcharts series. Two charts give
everything a PMRID (k, b) table needs:
    RN_ADU.htm : read noise in DN     (y = log2(DN))
    RN_e.htm   : read noise in e-     (y = log2(e-)), and per-series fwc = full-well (e-)
Both share x with ISO = round(3.125 * 2^x). Gain = RN_e / RN_dn, white level Wn(DN) =
fwc / gain(base ISO), and then (validated against our measured Sony tables, k within ~1-11%):
    k = read_noise_dn / (read_noise_e * Wn)      b = (read_noise_dn / Wn)^2

TWO MODES
  Targeted -> tools/claff/<model>.csv (for curated / hand-checked additions; feed to
              gen_pmrid_tables.py):
      python3 fetch_photonstophotos.py "Sony ILCE-7RM5:ILCE-7RM5" "Nikon Z8:Z 8"
  --all    -> derive EVERY body photonstophotos has (~400+) straight into the DERIVED and
              DERIVED-REGISTRY blocks of ImageFormats/Raw/pmrid_calib_tables.h, ordered by
              match specificity (longest first) so substrings never shadow:
      python3 fetch_photonstophotos.py --all

MODEL MATCH is a SUBSTRING of ImageMetadata::model (from the raw's Make/Model, NOT the chart
name). The maker prefix is stripped (Sony/Canon/Nikon/Fuji-X are exact; Olympus/Fuji-GFX
spacing may differ -> that table simply never matches and tier-3 blind estimation covers the
body, which is harmless). Override a specific one with the targeted "name:match" form.

Charts are cached in the OS temp dir; pass --refresh to re-download.
"""

import argparse
import os
import re
import sys
import tempfile
import urllib.request

ADU_URL = "https://www.photonstophotos.net/Charts/RN_ADU.htm"
ELE_URL = "https://www.photonstophotos.net/Charts/RN_e.htm"
HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "claff")
HEADER = os.path.join(HERE, "..", "ImageFormats", "Raw", "pmrid_calib_tables.h")
STOPS = list(range(4, 17))                       # x=4..16 -> ISO 50..204800 (full stops)
MEASURED = {"ILCE-7RM5", "ILCE-1M2"}             # keep the measured tables, skip derived dupes
MAKERS = ["Panasonic Lumix", "Phase One", "Konica Minolta", "Nikon", "Canon", "Sony",
          "FujiFilm", "Olympus", "Panasonic", "Leica", "Pentax", "Samsung", "Hasselblad",
          "Ricoh", "Apple", "Google", "Sigma", "Kodak", "DJI", "Zeiss", "Konica",
          "Pixii", "Nokia", "Phase"]


def cached(url, refresh):
    path = os.path.join(tempfile.gettempdir(), "p2p_" + os.path.basename(url))
    if refresh or not os.path.exists(path):
        print(f"downloading {url}", file=sys.stderr)
        urllib.request.urlretrieve(url, path)
    return open(path, encoding="utf-8", errors="replace").read()


def all_names(html):
    """Base body names (bit suffix stripped). Drop shooting-mode/sample variants -- any
       parenthetical, e.g. (ES), (EFCS), (HR), (Pixel Shift) -- since those never match a
       real EXIF model; but KEEP Leica '(Typ NNN)', which is part of the model name."""
    out = []
    for n in re.findall(r"name: '([^']+)'", html):
        if re.search(r"\((?!Typ )", n):
            continue
        out.append(re.sub(r"_(1[0-9]|[89])$", "", n))
    return list(dict.fromkeys(out))


def series(html, base_name):
    """({x: log2(value)}, fwc_or_None) for a base name, trying bit suffixes."""
    for suffix in ("_14", "_16", "_12", ""):
        name = base_name + suffix
        m = re.search(r"name: '" + re.escape(name) + r"'(?!\(ES\)).*?data: (\[.*?\])\}", html)
        if m:
            fwc = re.search(r"fwc: (\d+)", html[m.start():m.start() + 200])
            pts = {}
            for a, b in re.findall(r"\[(-?\d+\.?\d*),(-?\d+\.?\d*)\]", m.group(1)):
                pts[round(float(a), 2)] = float(b)
            for a, b in re.findall(r"\{x:(-?\d+\.?\d*),y:(-?\d+\.?\d*)", m.group(1)):
                pts[round(float(a), 2)] = float(b)
            return pts, (int(fwc.group(1)) if fwc else None)
    return {}, None


def model_match(name):
    for pfx in MAKERS:
        if name.startswith(pfx + " "):
            return name[len(pfx) + 1:].strip()
    return name


def cident(match):
    return "kClaff_" + re.sub(r"[^A-Za-z0-9]+", "_", match).strip("_")


def kb_rows(base_name, adu, ele):
    """[(iso, k, b), ...] for a body, or None if not derivable."""
    dn, _ = series(adu, base_name)
    en, fwc = series(ele, base_name)
    if not dn or not en or not fwc:
        return None
    common = sorted(x for x in STOPS if x in dn and x in en)
    if not common:
        return None
    xb = common[0]                               # base ISO = lowest available full stop
    wn = fwc * (2 ** dn[xb]) / (2 ** en[xb])     # fwc / gain(base)
    rows = []
    for x in common:
        rn_dn, rn_e = 2 ** dn[x], 2 ** en[x]
        rows.append((round(3.125 * 2 ** x), rn_dn / (rn_e * wn), (rn_dn / wn) ** 2))
    return rows


def splice(text, begin, end, body):
    """Replace text between the begin/end marker lines (markers kept)."""
    pat = re.compile(re.escape(begin) + r".*?" + re.escape(end), re.S)
    return pat.sub(begin + "\n" + body + "\n" + end, text, count=1)


def emit_all(refresh):
    adu, ele = cached(ADU_URL, refresh), cached(ELE_URL, refresh)
    tables, regs, skipped = [], [], 0
    for name in all_names(ele):
        match = model_match(name)
        if len(match) < 2 or match in MEASURED:
            continue
        rows = kb_rows(name, adu, ele)
        if not rows:
            skipped += 1
            continue
        ident = cident(match)
        body = [f"/* {name} (derived, photonstophotos) */",
                f"constexpr IsoKB {ident}[] = {{"]
        for iso, k, b in rows:
            body.append(f"    {{ {iso:6d}, {k:.6e}, {b:.6e} }},")
        body.append("};")
        tables.append(("\n".join(body), match, ident, len(rows)))
    # longest match first so a substring can never shadow a more specific body
    tables.sort(key=lambda t: (-len(t[1]), t[1]))
    arrays = "\n".join(t[0] for t in tables)
    regs = "\n".join(f"    {{ \"{m}\", {i}, {n} }}," for _, m, i, n in tables)

    hdr = open(HEADER, encoding="utf-8").read()
    hdr = splice(hdr, "/* @@ DERIVED-BEGIN @@ (paste gen_pmrid_tables.py output here) */",
                 "/* @@ DERIVED-END @@ */", arrays)
    hdr = splice(hdr, "    /* @@ DERIVED-REGISTRY-BEGIN @@ */",
                 "    /* @@ DERIVED-REGISTRY-END @@ */", regs)
    open(HEADER, "w").write(hdr)
    print(f"wrote {len(tables)} derived tables into {os.path.relpath(HEADER, HERE)} "
          f"({skipped} bodies skipped: missing data)", file=sys.stderr)


def targeted(specs, refresh):
    adu, ele = cached(ADU_URL, refresh), cached(ELE_URL, refresh)
    os.makedirs(OUT, exist_ok=True)
    for spec in specs:
        name, _, match = spec.partition(":")
        name, match = name.strip(), (match.strip() or model_match(name.strip()))
        dn, _ = series(adu, name)
        en, fwc = series(ele, name)
        rows = kb_rows(name, adu, ele)
        if not rows or not fwc:
            print(f"  SKIP {name}: missing data", file=sys.stderr)
            continue
        common = sorted(x for x in STOPS if x in dn and x in en)
        wn = round(fwc * (2 ** dn[common[0]]) / (2 ** en[common[0]]))
        lines = [f"# model: {match}",
                 f"# photonstophotos {name}; fwc={fwc}e-, Wn={wn}DN",
                 "iso,read_noise_e,read_noise_dn,saturation_dn,black_dn"]
        for x in common:
            lines.append(f"{round(3.125*2**x)},{2**en[x]:.3f},{2**dn[x]:.4f},{wn},0")
        fn = re.sub(r"[^A-Za-z0-9]+", "_", name).strip("_") + ".csv"
        open(os.path.join(OUT, fn), "w").write("\n".join(lines) + "\n")
        print(f"  wrote claff/{fn} (match '{match}')", file=sys.stderr)
    print("\nNow run: python3 tools/gen_pmrid_tables.py", file=sys.stderr)


def main():
    ap = argparse.ArgumentParser(description="Scrape photonstophotos read noise -> PMRID tables.")
    ap.add_argument("cameras", nargs="*", help='"<p2p name>:<model-match>" (targeted mode)')
    ap.add_argument("--all", action="store_true", help="derive every body into the header")
    ap.add_argument("--refresh", action="store_true", help="re-download charts")
    args = ap.parse_args()
    if args.all:
        emit_all(args.refresh)
    elif args.cameras:
        targeted(args.cameras, args.refresh)
    else:
        ap.error("give cameras, or --all")


if __name__ == "__main__":
    sys.exit(main())
