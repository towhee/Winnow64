#!/usr/bin/env python3
"""
FUTURE / long-term path -- NOT the active denoiser. The shipped "Denoise raw" currently uses the
POST-demosaic RawRefinery TreeNet (see export_treenet.py -> rawdenoise.onnx, and the C++ engine
ImageFormats/Raw/rawdenoise.cpp which is RGB+ISO). This script targets the eventual PRE-demosaic
(Bayer-domain) design; wiring it back up would require reverting rawdenoise.cpp to the 4-channel
packed-Bayer engine. Kept because pre-demosaic denoising is the better long-term option.

Export a NAFNet raw denoiser to ONNX:

    ReleaseExtras/nafnet_raw.onnx   -- 4-channel (packed-Bayer) NAFNet, dynamic H/W

Winnow runs this through the unified inference layer (Utilities/inference/) via ONNX Runtime --
CoreML EP on macOS (Apple Neural Engine), DirectML on Windows. The engine (ImageFormats/Raw/
rawdenoise.cpp) packs the Bayer mosaic into a 4-channel half-resolution image in canonical
[R, Gr, Gb, B] order, normalised to [0,1], and feeds tiles as input tensor {1,4,H,W}; the model
returns the denoised 4 planes in the same layout.

INPUT MODEL. NAFNet's public SIDD weights are 3-channel sRGB; raw denoise needs a 4-CHANNEL
(img_channel=4) NAFNet trained on packed Bayer (e.g. a SID/ELD/AIM-raw checkpoint). Point
--checkpoint at that .pth. The architecture below is the official megvii-research/NAFNet arch
(MIT), so a checkpoint trained with that arch loads by key. If your checkpoint uses a different
arch, replace build_model() or import your own and keep the ONNX I/O contract (one {1,C,H,W}
float input in [0,1], one same-shape output).

Build-time only: torch / onnx / onnxruntime PRODUCE the file; nothing extra ships with Winnow.

Usage:
    export_nafnet_raw.py --checkpoint path/to/raw_nafnet.pth
                         [--img-channel 4] [--width 64]
                         [--enc-blks 2 2 4 8] [--middle-blks 12] [--dec-blks 2 2 2 2]
                         [--opset 17] [--output <ReleaseExtras>/nafnet_raw.onnx]
"""
import argparse
import os

import torch
import torch.nn as nn
import torch.nn.functional as F

HERE = os.path.dirname(os.path.abspath(__file__))
RELEASE_EXTRAS = os.path.abspath(os.path.join(HERE, ".."))     # ReleaseExtras/


# ---------------------------------------------------------------------------
# NAFNet architecture (megvii-research/NAFNet, MIT). LayerNorm2d is written with
# plain ops (no custom autograd Function) so it traces cleanly to ONNX; parameter
# names match the official arch, so official/derived checkpoints load by key.
# ---------------------------------------------------------------------------
class LayerNorm2d(nn.Module):
    def __init__(self, channels, eps=1e-6):
        super().__init__()
        self.weight = nn.Parameter(torch.ones(channels))
        self.bias = nn.Parameter(torch.zeros(channels))
        self.eps = eps

    def forward(self, x):
        mu = x.mean(1, keepdim=True)
        var = (x - mu).pow(2).mean(1, keepdim=True)
        y = (x - mu) / (var + self.eps).sqrt()
        return self.weight.view(1, -1, 1, 1) * y + self.bias.view(1, -1, 1, 1)


class SimpleGate(nn.Module):
    def forward(self, x):
        x1, x2 = x.chunk(2, dim=1)
        return x1 * x2


class NAFBlock(nn.Module):
    def __init__(self, c, dw_expand=2, ffn_expand=2):
        super().__init__()
        dw = c * dw_expand
        self.conv1 = nn.Conv2d(c, dw, 1, 1, 0, bias=True)
        self.conv2 = nn.Conv2d(dw, dw, 3, 1, 1, groups=dw, bias=True)
        self.conv3 = nn.Conv2d(dw // 2, c, 1, 1, 0, bias=True)
        self.sca = nn.Sequential(
            nn.AdaptiveAvgPool2d(1),
            nn.Conv2d(dw // 2, dw // 2, 1, 1, 0, bias=True),
        )
        self.sg = SimpleGate()
        ffn = ffn_expand * c
        self.conv4 = nn.Conv2d(c, ffn, 1, 1, 0, bias=True)
        self.conv5 = nn.Conv2d(ffn // 2, c, 1, 1, 0, bias=True)
        self.norm1 = LayerNorm2d(c)
        self.norm2 = LayerNorm2d(c)
        self.beta = nn.Parameter(torch.zeros((1, c, 1, 1)))
        self.gamma = nn.Parameter(torch.zeros((1, c, 1, 1)))

    def forward(self, inp):
        x = self.norm1(inp)
        x = self.conv1(x)
        x = self.conv2(x)
        x = self.sg(x)
        x = x * self.sca(x)
        x = self.conv3(x)
        y = inp + x * self.beta
        x = self.conv4(self.norm2(y))
        x = self.sg(x)
        x = self.conv5(x)
        return y + x * self.gamma


class NAFNet(nn.Module):
    def __init__(self, img_channel=4, width=64, middle_blk_num=12,
                 enc_blk_nums=(2, 2, 4, 8), dec_blk_nums=(2, 2, 2, 2)):
        super().__init__()
        self.intro = nn.Conv2d(img_channel, width, 3, 1, 1, bias=True)
        self.ending = nn.Conv2d(width, img_channel, 3, 1, 1, bias=True)
        self.encoders = nn.ModuleList()
        self.decoders = nn.ModuleList()
        self.ups = nn.ModuleList()
        self.downs = nn.ModuleList()
        chan = width
        for num in enc_blk_nums:
            self.encoders.append(nn.Sequential(*[NAFBlock(chan) for _ in range(num)]))
            self.downs.append(nn.Conv2d(chan, 2 * chan, 2, 2))
            chan *= 2
        self.middle_blks = nn.Sequential(*[NAFBlock(chan) for _ in range(middle_blk_num)])
        for num in dec_blk_nums:
            self.ups.append(nn.Sequential(nn.Conv2d(chan, chan * 2, 1, bias=False),
                                          nn.PixelShuffle(2)))
            chan //= 2
            self.decoders.append(nn.Sequential(*[NAFBlock(chan) for _ in range(num)]))
        self.padder_size = 2 ** len(self.encoders)

    def _pad(self, x):
        _, _, h, w = x.size()
        ph = (self.padder_size - h % self.padder_size) % self.padder_size
        pw = (self.padder_size - w % self.padder_size) % self.padder_size
        return F.pad(x, (0, pw, 0, ph))

    def forward(self, inp):
        H, W = inp.shape[2], inp.shape[3]
        inp = self._pad(inp)
        x = self.intro(inp)
        encs = []
        for encoder, down in zip(self.encoders, self.downs):
            x = encoder(x)
            encs.append(x)
            x = down(x)
        x = self.middle_blks(x)
        for decoder, up, skip in zip(self.decoders, self.ups, encs[::-1]):
            x = up(x)
            x = x + skip
            x = decoder(x)
        x = self.ending(x)
        x = x + inp
        return x[:, :, :H, :W]


def build_model(args):
    model = NAFNet(img_channel=args.img_channel, width=args.width,
                   middle_blk_num=args.middle_blks,
                   enc_blk_nums=tuple(args.enc_blks), dec_blk_nums=tuple(args.dec_blks))
    if not args.checkpoint:
        print("[warn] no --checkpoint: exporting RANDOM-INIT weights. This is a PLUMBING TEST "
              "ONLY (validates load/run/ANE + tiling); the output is NOT denoised.")
        model.eval()
        return model
    ckpt = torch.load(args.checkpoint, map_location="cpu")
    # Unwrap the common basicsr / lightning containers.
    for key in ("params", "params_ema", "state_dict", "model"):
        if isinstance(ckpt, dict) and key in ckpt:
            ckpt = ckpt[key]
            break
    ckpt = {k.replace("module.", "", 1): v for k, v in ckpt.items()}
    missing, unexpected = model.load_state_dict(ckpt, strict=False)
    if missing:
        print(f"[warn] {len(missing)} missing keys (first few): {missing[:5]}")
    if unexpected:
        print(f"[warn] {len(unexpected)} unexpected keys (first few): {unexpected[:5]}")
    model.eval()
    return model


def main():
    ap = argparse.ArgumentParser(description="Export a NAFNet raw denoiser to ONNX.")
    ap.add_argument("--checkpoint", default=None,
                    help="path to the trained NAFNet .pth (omit for a RANDOM-INIT plumbing test)")
    ap.add_argument("--img-channel", type=int, default=4)
    ap.add_argument("--width", type=int, default=64)
    ap.add_argument("--enc-blks", type=int, nargs="+", default=[2, 2, 4, 8])
    ap.add_argument("--middle-blks", type=int, default=12)
    ap.add_argument("--dec-blks", type=int, nargs="+", default=[2, 2, 2, 2])
    ap.add_argument("--opset", type=int, default=17)
    ap.add_argument("--output", default=os.path.join(RELEASE_EXTRAS, "nafnet_raw.onnx"))
    args = ap.parse_args()

    model = build_model(args)
    dummy = torch.rand(1, args.img_channel, 256, 256)
    with torch.no_grad():
        _ = model(dummy)   # sanity forward

    export_kw = dict(
        input_names=["input"], output_names=["output"],
        opset_version=args.opset,
        dynamic_axes={"input": {2: "h", 3: "w"}, "output": {2: "h", 3: "w"}},
    )
    try:
        # Force the legacy TorchScript exporter: it needs no onnxscript and emits a cleaner,
        # more EP-friendly graph than torch>=2.x's default dynamo path.
        torch.onnx.export(model, dummy, args.output, dynamo=False, **export_kw)
    except TypeError:
        torch.onnx.export(model, dummy, args.output, **export_kw)   # older torch: no dynamo kwarg
    print(f"Wrote {args.output}")

    # Optional: verify it loads + runs under onnxruntime (the runtime Winnow uses).
    try:
        import numpy as np
        import onnxruntime as ort
        sess = ort.InferenceSession(args.output, providers=["CPUExecutionProvider"])
        out = sess.run(["output"], {"input": dummy.numpy().astype(np.float32)})[0]
        print(f"onnxruntime OK: output shape {out.shape}")
    except Exception as e:   # noqa: BLE001 -- best-effort check, build-time only
        print(f"[warn] onnxruntime verification skipped/failed: {e}")


if __name__ == "__main__":
    main()
