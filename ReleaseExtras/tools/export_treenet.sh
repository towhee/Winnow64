#!/bin/bash
# Convenience wrapper: throwaway venv with the build-time export deps, then run export_treenet.py.
# Downloads a RawRefinery TreeNet checkpoint and writes ReleaseExtras/rawdenoise.onnx (the ACTIVE
# raw denoiser). Windows: run export_treenet.py directly in a venv (pip install torch onnx onnxruntime numpy).
#
# Usage: ./export_treenet.sh [variant]   variant in {super_light (default), light, standard, heavy}
set -euo pipefail
DIR="$(cd "$(dirname "$0")" && pwd)"
VENV="$DIR/_nafnet_venv"

python3 -m venv "$VENV"
"$VENV/bin/pip" -q install --upgrade pip
"$VENV/bin/pip" -q install torch onnx onnxruntime numpy
"$VENV/bin/python" "$DIR/export_treenet.py" "${1:-super_light}"
