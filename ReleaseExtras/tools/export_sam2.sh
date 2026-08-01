#!/bin/bash
# Convenience wrapper: set up a throwaway venv with the build-time export deps and run export_sam2.py.
# Produces ReleaseExtras/sam2_encoder.onnx + sam2_decoder.onnx (see that script for the why).
# Windows: run export_sam2.py directly in a venv (python -m venv; pip install onnx onnxsim onnxruntime).
#
# Usage: ./export_sam2.sh [variant]   variant in {tiny (default), small, base_plus, large}
set -euo pipefail
DIR="$(cd "$(dirname "$0")" && pwd)"
VENV="$DIR/_sam2_venv"

python3 -m venv "$VENV"
"$VENV/bin/pip" -q install --upgrade pip
"$VENV/bin/pip" -q install onnx onnxsim onnxruntime
"$VENV/bin/python" "$DIR/export_sam2.py" "${1:-tiny}"
