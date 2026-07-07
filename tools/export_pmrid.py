#!/usr/bin/env python3
"""
Export PMRID (MegVii, ECCV'20, Apache-2.0) PyTorch weights to ONNX and verify
torch-vs-onnxruntime parity. Part of the raw-denoise oracle spike -- see
~/.claude/plans/moonlit-dancing-peach.md. Standalone; touches no Winnow code.
"""
import os
import sys

import numpy as np
import torch

HERE = os.path.dirname(os.path.abspath(__file__))
DL = os.path.join(HERE, "_pmrid_dl")
sys.path.insert(0, DL)  # so `from models.net_torch import Network` resolves

from models.net_torch import Network  # noqa: E402

CKPT = os.path.join(DL, "models", "torch_pretrained.ckp")
ONNX = os.path.join(HERE, "pmrid.onnx")


def build_net():
    net = Network()
    sd = torch.load(CKPT, map_location="cpu", weights_only=True)  # tensors only
    net.load_state_dict(sd)
    net.eval()
    return net


def main():
    net = build_net()

    # Packed RGGB input [1,4,H,W]; H,W must be multiples of 32 (4x stride-2 + pad-to-32).
    dummy = torch.randn(1, 4, 64, 64, dtype=torch.float32)

    torch.onnx.export(
        net, dummy, ONNX,
        input_names=["input"], output_names=["output"],
        dynamic_axes={"input": {2: "H", 3: "W"}, "output": {2: "H", 3: "W"}},
        opset_version=17,
        dynamo=False,  # legacy TorchScript exporter (avoids onnxscript dep)
    )
    print("wrote", ONNX)

    # Parity: torch vs onnxruntime on a fresh random input.
    import onnxruntime as ort
    x = np.random.randn(1, 4, 96, 128).astype(np.float32)
    with torch.no_grad():
        t = net(torch.from_numpy(x)).numpy()
    sess = ort.InferenceSession(ONNX, providers=["CPUExecutionProvider"])
    o = sess.run(["output"], {"input": x})[0]
    diff = float(np.max(np.abs(t - o)))
    print(f"ORT-vs-torch max-abs-diff = {diff:.3e}  ->  {'PASS' if diff < 1e-4 else 'FAIL'}")


if __name__ == "__main__":
    main()
