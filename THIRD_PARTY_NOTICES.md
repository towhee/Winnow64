# Third-Party Notices

Winnow is Copyright (C) Rory Hill and is licensed under the **GNU General Public
License, version 3 only** (GPL-3.0-only). The full text is in [LICENSE](LICENSE).

Winnow uses and redistributes the open-source software listed below. Each component
remains under its own license. The full license texts are in the [licenses](licenses)
folder. Winnow also shows this page and those texts from **Help > About Winnow >
Third-party licenses**.

Where a library is licensed under the GNU LGPL, Winnow links it dynamically, so you may
replace it with a modified version. The source code of each LGPL library is available
from the upstream project linked below, or on request from the Winnow author.

## Application framework

- **[Qt 6](https://www.qt.io/)** (Core, Gui, Widgets, Multimedia, Network, Sql, Svg,
  Concurrent). Application framework.
  LGPL-3.0: [LGPL-3.0](licenses/LGPL-3.0.txt).
  Qt contains further third-party code, listed in
  [Licenses Used in Qt](https://doc.qt.io/qt-6/licenses-used-in-qt.html).
- **[FFmpeg](https://ffmpeg.org/)**, bundled with Qt Multimedia. Video playback.
  LGPL-2.1-or-later: [LGPL-2.1](licenses/LGPL-2.1.txt).

## Image processing and file formats

- **[OpenCV](https://opencv.org/)**. Image processing and neural network inference.
  Apache-2.0: [Apache-2.0](licenses/Apache-2.0.txt), [copyright](licenses/opencv.txt).
- **[libjpeg-turbo](https://libjpeg-turbo.org/)**. JPEG decoding.
  IJG, BSD-3-Clause and Zlib: [libjpeg-turbo](licenses/libjpeg-turbo.md).
  This software is based in part on the work of the Independent JPEG Group.
- **[LibTIFF](https://libtiff.gitlab.io/libtiff/)**. TIFF decoding.
  libtiff license (BSD-like): [libtiff](licenses/libtiff.md).
- **[zlib](https://zlib.net/)**. Compression.
  Zlib: [zlib](licenses/zlib.txt).
- **[Little CMS 2](https://www.littlecms.com/)**, source included in `Lcms2/`. Colour
  management. MIT: [lcms2](licenses/lcms2.txt).
- **[libheif](https://github.com/strukturag/libheif)** (Windows). HEIF/HEIC decoding.
  LGPL-3.0-or-later: [LGPL-3.0](licenses/LGPL-3.0.txt).
- **[libde265](https://github.com/strukturag/libde265)** (Windows). HEVC decoding for
  HEIF. LGPL-3.0-or-later: [LGPL-3.0](licenses/LGPL-3.0.txt).
- **[LibRaw](https://www.libraw.org/)**. The Fujifilm compressed RAF and Canon CR3
  decoders are ported from LibRaw into `ImageFormats/Fuji/fujicompressed.cpp` and
  `ImageFormats/Canon/crxcodec.cpp`. LGPL-2.1 or CDDL-1.0, used under LGPL-2.1:
  [LibRaw](licenses/libraw.txt), [LGPL-2.1](licenses/LGPL-2.1.txt).
- **[ONNX Runtime](https://onnxruntime.ai/)**. Neural network inference.
  MIT: [onnxruntime](licenses/onnxruntime.txt).
- **[ExifTool](https://exiftool.org/)** by Phil Harvey. Metadata writing.
  Same terms as Perl (Artistic-1.0-Perl or GPL-1.0-or-later):
  [ExifTool](licenses/exiftool.txt), [Artistic](licenses/Artistic-1.0-Perl.txt).

## Libraries bundled as dependencies (macOS)

OpenCV and ONNX Runtime pull these in, and they ship inside the macOS application.

- **[Abseil](https://abseil.io/)**. Apache-2.0: [Apache-2.0](licenses/Apache-2.0.txt).
- **[Protocol Buffers](https://protobuf.dev/)**, including utf8_range.
  BSD-3-Clause: [protobuf](licenses/protobuf.txt).
- **[oneTBB](https://github.com/uxlfoundation/oneTBB)**.
  Apache-2.0: [Apache-2.0](licenses/Apache-2.0.txt).
- **[OpenVINO](https://github.com/openvinotoolkit/openvino)**.
  Apache-2.0: [Apache-2.0](licenses/Apache-2.0.txt).
- **[OpenBLAS](https://www.openblas.net/)**.
  BSD-3-Clause: [openblas](licenses/openblas.txt).
- **[LLVM OpenMP runtime](https://openmp.llvm.org/)** (libomp).
  Apache-2.0 WITH LLVM-exception: [libomp](licenses/libomp.txt).
- **GCC runtime libraries** (libgfortran, libquadmath, libgcc_s).
  GPL-3.0 WITH GCC-exception-3.1:
  [GCC Runtime Library Exception](licenses/GCC-Runtime-Exception-3.1.txt).
- **[OpenEXR](https://openexr.com/)**. BSD-3-Clause: [openexr](licenses/openexr.md).
- **[Imath](https://github.com/AcademySoftwareFoundation/Imath)**.
  BSD-3-Clause: [imath](licenses/imath.md).
- **[OpenJPEG](https://www.openjpeg.org/)**.
  BSD-2-Clause: [openjpeg](licenses/openjpeg.txt).
- **[OpenJPH](https://github.com/aous72/OpenJPH)**.
  BSD-2-Clause: [openjph](licenses/openjph.txt).
- **[libpng](http://www.libpng.org/pub/png/libpng.html)**.
  libpng-2.0: [libpng](licenses/libpng.txt).
- **[libwebp](https://chromium.googlesource.com/webm/libwebp)**, including sharpyuv.
  BSD-3-Clause: [webp](licenses/webp.txt).
- **[libdeflate](https://github.com/ebiggers/libdeflate)**.
  MIT: [libdeflate](licenses/libdeflate.txt).
- **[XZ Utils](https://tukaani.org/xz/)** (liblzma). 0BSD: [xz](licenses/xz.txt).
- **[Zstandard](https://facebook.github.io/zstd/)**.
  BSD-3-Clause: [zstd](licenses/zstd.txt).
- **[pugixml](https://pugixml.org/)**. MIT: [pugixml](licenses/pugixml.md).

## Data

- **[lensfun](https://lensfun.github.io/) lens database**, data only, compiled in. Lens
  corrections. CC BY-SA 3.0: [CC-BY-SA-3.0](licenses/CC-BY-SA-3.0.txt).
  Lens profiles from the lensfun project (lensfun.github.io), CC BY-SA 3.0.

Winnow does not ship Adobe camera profiles (.dcp). It reads them only where the user
already has them installed.

## Neural network models

Most models are not part of the application. Winnow downloads them from winnow.ca the
first time a feature needs them. They are converted to ONNX from the upstream weights
below and keep their upstream licenses.

- **Focus point model** (Focus point detection). Winnow's own model.
  GPL-3.0-only: [LICENSE](LICENSE).
- **PMRID** (Raw denoise). [MegEngine/PMRID](https://github.com/MegEngine/PMRID).
  Apache-2.0: [Apache-2.0](licenses/Apache-2.0.txt).
- **U²-Net** (Select Subject). [xuebinqin/U-2-Net](https://github.com/xuebinqin/U-2-Net).
  Apache-2.0: [Apache-2.0](licenses/Apache-2.0.txt).
- **Sky segmentation** (Select Sky).
  <https://github.com/xiongzhu666/Sky-Segmentation-and-Post-processing>.
  MIT: [skyseg](licenses/skyseg.txt).
- **SegFormer-B4, ADE20K** (Select Sky).
  [NVlabs/SegFormer](https://github.com/NVlabs/SegFormer).
  NVIDIA Source Code License, **non-commercial**: [segformer](licenses/segformer.txt).
- **MiDaS v2.1 small** (Depth Range mask).
  [isl-org/MiDaS](https://github.com/isl-org/MiDaS).
  MIT: [midas](licenses/midas.txt).
- **MI-GAN** (Spot removal).
  [Picsart-AI-Research/MI-GAN](https://github.com/Picsart-AI-Research/MI-GAN).
  MIT: [migan](licenses/migan.txt).
- **LaMa** (Generative fill). [advimman/lama](https://github.com/advimman/lama).
  Apache-2.0: [Apache-2.0](licenses/Apache-2.0.txt).
- **SAM 2**, encoder and decoder (Object mask, Segmenter).
  [facebookresearch/sam2](https://github.com/facebookresearch/sam2).
  Apache-2.0: [Apache-2.0](licenses/Apache-2.0.txt).
- **SigLIP 2**, image tower (Audit Keywords).
  [google-research/big_vision](https://github.com/google-research/big_vision).
  Apache-2.0: [Apache-2.0](licenses/Apache-2.0.txt).

The SegFormer weights may be used **for non-commercial research or evaluation purposes
only**, under the NVIDIA Source Code License for SegFormer. That restriction applies to
the model file, not to Winnow.
