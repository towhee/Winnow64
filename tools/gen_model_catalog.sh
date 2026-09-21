#!/bin/bash
#
# Regenerate the ModelStore catalog rows (Utilities/modelstore.cpp) and manifest.json
# from the .onnx files in ReleaseExtras/.
#
# Winnow downloads on-demand models from an APPEND-ONLY server directory:
#
#     https://winnow.ca/winnow/models/<stem>-<version>.onnx   ->   <Models>/<stem>.onnx
#
# A published <stem>-<n>.onnx is NEVER overwritten or deleted -- every Winnow already
# installed fetches exactly the bytes whose SHA-256 it was compiled with. Re-exporting a
# model means uploading <stem>-<n+1>.onnx alongside the old one and bumping the version in
# the catalog.
#
# This script exists to close the "I forgot to bump the version" hole: it diffs each local
# model against the committed catalog and FAILS if bytes changed while the version did not.
#
# Usage:
#   tools/gen_model_catalog.sh            # check + print rows for anything that changed
#   tools/gen_model_catalog.sh --all      # print every row (first-time bootstrap)
#
set -uo pipefail

cd "$(dirname "$0")/.." || exit 1

CATALOG="Utilities/modelstore.cpp"
EXTRAS="ReleaseExtras"
OUT="tools/_model_catalog_out"
mkdir -p "$OUT"

# stem|feature|bundled
MODELS=(
    "focus_point_model|Pan to focus point|1"
    "pmrid|Denoise raw|1"
    "u2net|Select Subject|0"
    "skyseg|Select Sky|0"
    "midas|Depth Range|0"
    "migan|Spot Heal|0"
    "lama|Spot Heal|0"
    "sam2_encoder|Object Mask|0"
    "sam2_decoder|Object Mask|0"
)

ALL=0
[ "${1:-}" = "--all" ] && ALL=1

fail=0
stale=0
rows=""
manifest_entries=""
uploads=""

for entry in "${MODELS[@]}"; do
    IFS='|' read -r stem feature bundled <<< "$entry"
    f="$EXTRAS/$stem.onnx"

    if [ ! -f "$f" ]; then
        # On-demand models are deleted from ReleaseExtras once they live on the server;
        # that is the expected steady state, so only complain about the bundled ones.
        if [ "$bundled" = "1" ]; then
            echo "ERROR: $f is missing and is a BUNDLED model." >&2
            fail=1
        else
            echo "note: $f absent (on-demand; already published) -- keeping committed row" >&2
        fi
        continue
    fi

    bytes=$(stat -f%z "$f")
    sha=$(shasum -a 256 "$f" | cut -d' ' -f1)

    # What the committed catalog currently claims for this stem.
    committed_sha=""
    committed_ver=""
    if [ -f "$CATALOG" ]; then
        # A catalog row spans TWO lines -- id/name/feature/version on the first, then
        # bytes/sha/bundled on the second -- so take the row and its successor.
        row=$(grep -F -A1 "\"$stem.onnx\"" "$CATALOG" | head -2)
        if [ -n "$row" ]; then
            committed_sha=$(echo "$row" | grep -oE '[0-9a-f]{64}' | head -1)
            committed_ver=$(echo "$row" | head -1 | sed -E 's/.*",[[:space:]]*([0-9]+),.*/\1/')
        fi
    fi

    ver="${committed_ver:-1}"
    changed=0
    if [ -n "$committed_sha" ] && [ "$committed_sha" != "$sha" ]; then
        changed=1
        ver=$((committed_ver + 1))
        echo "CHANGED: $stem.onnx  (v$committed_ver -> v$ver)" >&2
        if [ "$bundled" = "0" ]; then
            uploads="${uploads}put $EXTRAS/$stem.onnx $stem-$ver.onnx"$'\n'
        fi
    elif [ -z "$committed_sha" ]; then
        changed=1
        echo "NEW: $stem.onnx (v1)" >&2
        [ "$bundled" = "0" ] && uploads="${uploads}put $EXTRAS/$stem.onnx $stem-1.onnx"$'\n'
    fi

    [ "$changed" = "1" ] && stale=1

    if [ "$changed" = "1" ] || [ "$ALL" = "1" ]; then
        rows="${rows}    { Model::__ID__, \"$stem.onnx\", \"$feature\", $ver,"$'\n'
        rows="${rows}      ${bytes}LL, \"$sha\", $( [ "$bundled" = "1" ] && echo true || echo false ) },"$'\n'
    fi

    if [ "$bundled" = "0" ]; then
        manifest_entries="${manifest_entries}    {\"file\": \"$stem.onnx\", \"version\": $ver, \"url\": \"$stem-$ver.onnx\", \"bytes\": $bytes, \"sha256\": \"$sha\"},"$'\n'
    fi
done

if [ -n "$manifest_entries" ]; then
    {
        echo "{"
        echo "  \"generated\": \"$(date -u +%Y-%m-%dT%H:%M:%SZ)\","
        echo "  \"models\": ["
        echo "${manifest_entries%,$'\n'}"
        echo "  ]"
        echo "}"
    } > "$OUT/manifest.json"
    echo "wrote $OUT/manifest.json" >&2
fi

if [ -n "$rows" ]; then
    echo
    echo "==== catalog rows (paste into $CATALOG, set __ID__) ===="
    printf '%s' "$rows"
fi

if [ -n "$uploads" ]; then
    echo
    echo "==== upload to /var/www/html/winnow/models (NEVER overwrite an existing -n) ===="
    echo "sftp root@165.227.46.158"
    echo "cd /var/www/html/winnow/models"
    printf '%s' "$uploads"
    echo "put $OUT/manifest.json manifest.json"
fi

# Any model whose bytes no longer match its committed row leaves the catalog stale --
# including a BUNDLED one, whose row still carries the size the Manage dialog shows.
# Failing here is the whole point of the script: a stale catalog must not reach a release.
if [ "$stale" = "1" ] && [ "$ALL" = "0" ]; then
    echo
    echo "ERROR: catalog is stale -- paste the rows above into $CATALOG before releasing." >&2
    fail=1
fi

[ "$fail" = "0" ] && echo "catalog is up to date." >&2
exit $fail
