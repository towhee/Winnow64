# PMRID derived noise tables (photonstophotos / Bill Claff)

Drop one CSV per camera here, then run `tools/gen_pmrid_tables.py` to turn
published sensor data into PMRID `(k, b)` noise tables for
`ImageFormats/Raw/pmrid_calib_tables.h`.

These are the DERIVED tier (tier 2 fallback) of `PMRID::resolveKB`: used only on
an exact model match, so a rough table is never forced onto the wrong sensor.
Gold-standard MEASURED tables (from `calibrate_pmrid.py`) always take precedence
when both exist.

## Where the numbers come from

photonstophotos.net publishes, per ISO, the read noise in **both electrons and
DN** plus the sensor **saturation** (DN). See the "Read Noise" and "Sensor
Characteristics" data for a body. That is everything needed — gain falls out of
the two read-noise units:

    gain_e_per_dn = read_noise_e / read_noise_dn
    Wn            = saturation_dn - black_dn        # black-subtracted full scale
    k = read_noise_dn / (read_noise_e * Wn)
    b = (read_noise_dn / Wn)^2

(`x` is the black-subtracted, white-normalised signal in `[0,1]` — the domain
PMRID's KSigma step works in, matching `RawImage.white` after `SubtractBlack`.)

## CSV format

    # model: ILCE-7RM5
    iso,read_noise_e,read_noise_dn,saturation_dn,black_dn
    100,...,...,...,...
    200,...,...,...,...

- First non-blank line: `# model: <ImageMetadata::model substring>` (e.g.
  `ILCE-7RM5`, `NIKON Z 8`). This is what `resolveKB` substring-matches against.
- `black_dn` is optional (default 0); supply it when known for an accurate `Wn`.
- `#` lines are comments; column order does not matter (matched by header name).

## Validation first

Sony **ILCE-7RM5** and **ILCE-1M2** are on photonstophotos AND we have measured
sweeps for them (`_pmrid_out/calib_*.json`). Derive those two first: the script
prints a derived-vs-measured `k`/`b` ratio to stderr. A `k` ratio near 1.0
confirms the units/mapping before you trust derived tables for bodies we have
not measured. (`b`, the read floor, is noisier — that is expected; `k` is the
load-bearing term.)

## Run

Automated (scrapes the read-noise charts for you):

    # every body photonstophotos has (~400), straight into pmrid_calib_tables.h:
    python3 tools/fetch_photonstophotos.py --all

    # one body -> a CSV here (lets you hand-verify the model-match string):
    python3 tools/fetch_photonstophotos.py "Sony ILCE-7RM5:ILCE-7RM5"

Manual CSVs you dropped here (or the fetch script's CSV output):

    python3 tools/gen_pmrid_tables.py                 # all CSVs here
    python3 tools/gen_pmrid_tables.py claff/NIKON_Z8.csv

`--all` writes between the `DERIVED` / `DERIVED-REGISTRY` markers in
`pmrid_calib_tables.h` directly. For the CSV path, paste the emitted arrays into
the `DERIVED` block and the registry rows into the `DERIVED-REGISTRY` block.
