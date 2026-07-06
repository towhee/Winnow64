#!/bin/bash
# Convenience wrapper: throwaway venv with the build-time export deps, then run export_nafnet_raw.py.
# Produces ReleaseExtras/nafnet_raw.onnx (see that script for the model requirements -- you must
# supply a 4-channel NAFNet raw-denoise checkpoint via --checkpoint).
# Windows: run export_nafnet_raw.py directly in a venv (python -m venv; pip install torch onnx onnxruntime).
#
# Usage: ./export_nafnet_raw.sh --checkpoint path/to/raw_nafnet.pth [more export_nafnet_raw.py args]
set -euo pipefail
DIR="$(cd "$(dirname "$0")" && pwd)"
VENV="$DIR/_nafnet_venv"

python3 -m venv "$VENV"
"$VENV/bin/pip" -q install --upgrade pip
"$VENV/bin/pip" -q install torch onnx onnxruntime numpy
"$VENV/bin/python" "$DIR/export_nafnet_raw.py" "$@"
