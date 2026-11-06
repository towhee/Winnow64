#!/bin/bash
# --------------------------------------------------------------------
# Build the macOS release dependencies for one architecture, for macOS 13.4.
#
#   packaging/macos/build_mac_deps.sh arm64          # -> ~/Projects/winnow-deps/macos-arm64
#   packaging/macos/build_mac_deps.sh x86_64         # -> ~/Projects/winnow-deps/macos-x86_64
#   packaging/macos/build_mac_deps.sh <arch> ort     # only (re)install ONNX Runtime
#   packaging/macos/build_mac_deps.sh <arch> jxl     # only (re)build libjxl + its deps
#
# Then: packaging/macos/deploy.command (arm64, preset mac-release) or
#       WINNOW_ARCH=x86_64 packaging/macos/deploy.command (preset mac-release-x86_64).
#
# WHY NOT HOMEBREW. Homebrew builds every bottle for the HOST macOS, and dyld refuses a
# library built for a newer macOS than the one running: v3_beta5's Apple Silicon app
# bundled ~140 Homebrew dylibs (OpenCV pulls ffmpeg, OpenVINO, OpenBLAS, gfortran, TBB)
# built for macOS 26-27, so it started on nothing older than 27. Homebrew is also
# arm64-only here, so the Intel build never could use it. These are built from source,
# STATIC (nothing to bundle), with deployment target 13.4.
#
# ONNX Runtime is the one PREBUILT, SHARED dependency (raw denoise): Microsoft's
# official onnxruntime-osx-<arch> package, pinned by SHA-256. 1.23.2 is the LAST release
# with an Intel macOS package, and its minos is 13.4 -- the floor for both Macs (ORT
# 1.29 for arm64 needs 14.0; 1.23.2 measured the same pmrid output and speed).
# macdeployqt copies the dylib into the bundle.
#
# libjxl (JPEG XL tiles in DNG 1.7) is built with its own skcms for colour management,
# on Highway and Brotli built here, at the commits/versions libjxl 0.12.0 expects.
#
# One-time, ~15 minutes per arch. Needs cmake, Qt Creator's ninja, and for x86_64 nasm
# (`brew install nasm`; an arm64 nasm assembles x86_64 SIMD fine).
# --------------------------------------------------------------------
set -euo pipefail

ARCH="${1:-}"
STEP="${2:-all}"
[[ "$ARCH" == arm64 || "$ARCH" == x86_64 ]] \
    || { echo "usage: build_mac_deps.sh arm64|x86_64 [all|ort|jxl]"; exit 2; }

DEPLOY_TARGET=13.4         # keep in step with CMakePresets.json + verify_bundle.command
JPEG_VER=3.1.2
TIFF_VER=4.7.1
OPENCV_VER=4.14.0
ORT_VER=1.23.2             # last ORT release with an osx-x86_64 package
JXL_VER=0.12.0
HWY_VER=1.4.0
BROTLI_VER=1.2.1
SKCMS_COMMIT=96d9171c94b937a1b5f0293de7309ac16311b722   # libjxl 0.12.0's submodule

# Pinned SHA-256 of every download (bash 3.2 -- /bin/bash on macOS -- has no maps)
sha256_of() {
    case "$1" in
        onnxruntime-osx-x86_64-1.23.2.tgz) echo d10359e16347b57d9959f7e80a225a5b4a66ed7d7e007274a15cae86836485a6 ;;
        onnxruntime-osx-arm64-1.23.2.tgz)  echo b4d513ab2b26f088c66891dbbc1408166708773d7cc4163de7bdca0e9bbb7856 ;;
        libjxl-0.12.0.tar.gz)  echo 03e9be69a30be4011f559da75328b6d7cea8ad921fabfbd551ce10bf45cdc992 ;;
        highway-1.4.0.tar.gz)  echo e72241ac9524bb653ae52ced768b508045d4438726a303f10181a38f764a453c ;;
        brotli-1.2.1.tar.gz)   echo 98d35d11a78dc453aebb7d10c3748f3f73ff78c8267c0dc697b8631f67bf7e8f ;;
        skcms-96d9171c94b937a1b5f0293de7309ac16311b722.tar.gz)
                               echo 9bb4b5bba0b7c04f6c2bce9ff713d61e23c9a20c4945161ae16290498ad74627 ;;
    esac
}

DEPS="${WINNOW_DEPS_DIR:-$HOME/Projects/winnow-deps}"
PREFIX="$DEPS/macos-$ARCH"
NINJA="${NINJA:-$HOME/Qt/Tools/Ninja/ninja}"
NASM="${NASM:-$(command -v nasm || echo /opt/homebrew/bin/nasm)}"
mkdir -p "$DEPS/src" "$DEPS/build" "$PREFIX/lib" "$PREFIX/include"

# download url -> $DEPS/src/<file>, checked against sha256_of <file> when it is pinned
download() {   # url file
    local f="$DEPS/src/$2"
    if [[ ! -f "$f" ]]; then
        echo "⬇️  $1"
        curl -sSLf -o "$f.part" "$1"
        mv "$f.part" "$f"
    fi
    local want; want=$(sha256_of "$2")
    if [[ -n "$want" ]]; then
        [[ "$(shasum -a 256 "$f" | cut -d' ' -f1)" == "$want" ]] \
            || { echo "❌ $f: SHA-256 mismatch"; exit 1; }
    fi
}

fetch() {   # url dir [file]  -- unpacked source tree under $DEPS/src
    [[ -d "$DEPS/src/$2" ]] && return 0
    local file="${3:-$2.tar.gz}"
    download "$1" "$file"
    tar xzf "$DEPS/src/$file" -C "$DEPS/src"
}

install_ort() {
    local name="onnxruntime-osx-$ARCH-$ORT_VER"
    download "https://github.com/microsoft/onnxruntime/releases/download/v$ORT_VER/$name.tgz" \
        "$name.tgz"
    tar xzf "$DEPS/src/$name.tgz" -C "$DEPS/src"
    rm -rf "$PREFIX/include/onnxruntime"
    mkdir -p "$PREFIX/include/onnxruntime"
    cp -R "$DEPS/src/$name/include/." "$PREFIX/include/onnxruntime/"
    cp -a "$DEPS/src/$name/lib/"libonnxruntime*.dylib "$PREFIX/lib/"
    cp "$DEPS/src/$name/LICENSE" "$PREFIX/lib/LICENSE-onnxruntime.txt"
    echo "✅ ONNX Runtime $ORT_VER ($ARCH) in $PREFIX"
}

# CMAKE_SYSTEM_PROCESSOR matters when cross-compiling x86_64: without it OpenCV detects
# the arm64 HOST and builds NEON/KleidiCV code, which fails for -arch x86_64.
COMMON=(-G Ninja -DCMAKE_MAKE_PROGRAM="$NINJA" -DCMAKE_BUILD_TYPE=Release
        -DCMAKE_OSX_ARCHITECTURES="$ARCH" -DCMAKE_SYSTEM_PROCESSOR="$ARCH"
        -DCMAKE_OSX_DEPLOYMENT_TARGET="$DEPLOY_TARGET" -DCMAKE_INSTALL_PREFIX="$PREFIX"
        -DCMAKE_PREFIX_PATH="$PREFIX"
        "-DCMAKE_IGNORE_PREFIX_PATH=/opt/homebrew;/usr/local"
        -DCMAKE_POSITION_INDEPENDENT_CODE=ON)

build() {   # name srcdir cmake-args...
    local name=$1 src=$2; shift 2
    echo "🏗  $name ($ARCH)"
    rm -rf "$DEPS/build/$name-$ARCH"
    { cmake -S "$DEPS/src/$src" -B "$DEPS/build/$name-$ARCH" "${COMMON[@]}" "$@" \
        && cmake --build "$DEPS/build/$name-$ARCH" \
        && cmake --install "$DEPS/build/$name-$ARCH"; } > "$DEPS/$name-$ARCH.log" 2>&1 \
        || { echo "❌ $name failed: $DEPS/$name-$ARCH.log"; tail -20 "$DEPS/$name-$ARCH.log"; exit 1; }
}

build_jxl() {
    fetch "https://github.com/google/highway/archive/refs/tags/$HWY_VER.tar.gz" \
        "highway-$HWY_VER"
    fetch "https://github.com/google/brotli/archive/refs/tags/v$BROTLI_VER.tar.gz" \
        "brotli-$BROTLI_VER"
    fetch "https://github.com/libjxl/libjxl/archive/refs/tags/v$JXL_VER.tar.gz" \
        "libjxl-$JXL_VER"
    # skcms is a git submodule of libjxl, which GitHub's source archive leaves empty
    local sk="$DEPS/src/libjxl-$JXL_VER/third_party/skcms"
    if [[ ! -f "$sk/skcms.h" ]]; then
        download "https://github.com/google/skcms/archive/$SKCMS_COMMIT.tar.gz" \
            "skcms-$SKCMS_COMMIT.tar.gz"
        rm -rf "$sk"; mkdir -p "$sk"
        tar xzf "$DEPS/src/skcms-$SKCMS_COMMIT.tar.gz" -C "$sk" --strip-components 1
    fi
    build highway "highway-$HWY_VER" \
        -DBUILD_SHARED_LIBS=OFF -DBUILD_TESTING=OFF -DHWY_ENABLE_TESTS=OFF \
        -DHWY_ENABLE_EXAMPLES=OFF -DHWY_ENABLE_CONTRIB=OFF
    build brotli "brotli-$BROTLI_VER" \
        -DBUILD_SHARED_LIBS=OFF -DBROTLI_DISABLE_TESTS=ON -DBROTLI_BUILD_TOOLS=OFF
    build jxl "libjxl-$JXL_VER" \
        -DBUILD_SHARED_LIBS=OFF -DBUILD_TESTING=OFF -DJPEGXL_STATIC=OFF \
        -DJPEGXL_FORCE_SYSTEM_HWY=ON -DJPEGXL_FORCE_SYSTEM_BROTLI=ON \
        -DJPEGXL_ENABLE_SKCMS=ON -DJPEGXL_ENABLE_TOOLS=OFF -DJPEGXL_ENABLE_DEVTOOLS=OFF \
        -DJPEGXL_ENABLE_BENCHMARK=OFF -DJPEGXL_ENABLE_EXAMPLES=OFF \
        -DJPEGXL_ENABLE_DOXYGEN=OFF -DJPEGXL_ENABLE_MANPAGES=OFF -DJPEGXL_ENABLE_JNI=OFF \
        -DJPEGXL_ENABLE_SJPEG=OFF -DJPEGXL_ENABLE_OPENEXR=OFF -DJPEGXL_ENABLE_JPEGLI=OFF \
        -DJPEGXL_ENABLE_PLUGINS=OFF -DJPEGXL_ENABLE_VIEWERS=OFF -DJPEGXL_ENABLE_FUZZERS=OFF \
        -DJPEGXL_ENABLE_TCMALLOC=OFF
}

build_core() {
    fetch "https://github.com/libjpeg-turbo/libjpeg-turbo/archive/refs/tags/$JPEG_VER.tar.gz" \
        "libjpeg-turbo-$JPEG_VER"
    fetch "https://download.osgeo.org/libtiff/tiff-$TIFF_VER.tar.gz" "tiff-$TIFF_VER"
    fetch "https://github.com/opencv/opencv/archive/refs/tags/$OPENCV_VER.tar.gz" \
        "opencv-$OPENCV_VER"

    local jpeg_simd=() cv_cpu=()
    if [[ $ARCH == x86_64 ]]; then
        jpeg_simd=(-DCMAKE_ASM_NASM_COMPILER="$NASM")
        cv_cpu=(-DWITH_KLEIDICV=OFF -DCPU_BASELINE=SSE4_2 -DWITH_IPP=ON)
    else
        # NEON is the arm64 baseline; KleidiCV (Arm's accelerated imgproc kernels) is
        # OpenCV's default there, as in the Homebrew build this replaces.
        cv_cpu=(-DWITH_IPP=OFF)
    fi

    build jpeg "libjpeg-turbo-$JPEG_VER" \
        -DENABLE_SHARED=OFF -DENABLE_STATIC=ON -DWITH_TURBOJPEG=ON ${jpeg_simd[@]+"${jpeg_simd[@]}"}

    build tiff "tiff-$TIFF_VER" \
        -DBUILD_SHARED_LIBS=OFF -Dtiff-tools=OFF -Dtiff-tests=OFF -Dtiff-docs=OFF \
        -Dtiff-contrib=OFF -Dtiff-cxx=OFF \
        -Djpeg=ON -DJPEG_INCLUDE_DIR="$PREFIX/include" \
        -DJPEG_LIBRARY="$PREFIX/lib/libjpeg.a" \
        -Dzlib=ON -Dzstd=OFF -Dlzma=OFF -Dwebp=OFF -Dlerc=OFF -Djbig=OFF -Dlibdeflate=OFF

    # imgcodecs uses the jpeg/tiff above (not OpenCV's bundled copies) so the static link
    # has ONE copy of each. CMAKE_DISABLE_FIND_PACKAGE_Tiff stops FindTIFF from loading
    # libtiff's exported config, which references an undefined CMath::CMath target.
    build opencv "opencv-$OPENCV_VER" \
        -DCMAKE_SYSTEM_NAME=Darwin "${cv_cpu[@]}" \
        -DBUILD_SHARED_LIBS=OFF -DBUILD_LIST=core,imgproc,imgcodecs,video,dnn,photo \
        -DBUILD_TESTS=OFF -DBUILD_PERF_TESTS=OFF -DBUILD_EXAMPLES=OFF -DBUILD_DOCS=OFF \
        -DBUILD_opencv_apps=OFF -DBUILD_JAVA=OFF -DWITH_PYTHON=OFF \
        -DBUILD_opencv_python3=OFF -DBUILD_opencv_python2=OFF \
        -DWITH_FFMPEG=OFF -DWITH_GSTREAMER=OFF -DWITH_AVFOUNDATION=OFF -DWITH_QT=OFF \
        -DWITH_GTK=OFF -DWITH_OPENEXR=OFF -DWITH_JASPER=OFF -DWITH_OPENJPEG=OFF \
        -DWITH_AVIF=OFF -DWITH_EIGEN=OFF -DWITH_VTK=OFF -DWITH_OPENCL=OFF \
        -DWITH_LAPACK=OFF -DWITH_PROTOBUF=ON -DBUILD_PROTOBUF=ON \
        -DWITH_JPEG=ON -DBUILD_JPEG=OFF \
        -DJPEG_INCLUDE_DIR="$PREFIX/include" -DJPEG_LIBRARY="$PREFIX/lib/libjpeg.a" \
        -DWITH_TIFF=ON -DBUILD_TIFF=OFF -DCMAKE_DISABLE_FIND_PACKAGE_Tiff=ON \
        -DTIFF_INCLUDE_DIR="$PREFIX/include" -DTIFF_LIBRARY="$PREFIX/lib/libtiff.a" \
        -DBUILD_ZLIB=OFF -DWITH_PNG=ON -DBUILD_PNG=ON -DWITH_WEBP=ON -DBUILD_WEBP=ON \
        -DOPENCV_GENERATE_PKGCONFIG=OFF
}

case "$STEP" in
    ort) install_ort ;;
    jxl) build_jxl ;;
    all) install_ort; build_core; build_jxl ;;
    *)   echo "unknown step: $STEP (all|ort|jxl)"; exit 2 ;;
esac

for a in "$PREFIX"/lib/*.a "$PREFIX"/lib/opencv4/3rdparty/*.a "$PREFIX"/lib/*.dylib; do
    [[ -e "$a" ]] || continue
    [[ "$(lipo -archs "$a")" == "$ARCH" ]] || { echo "❌ not $ARCH: $a"; exit 1; }
done
echo "✅ $ARCH deps installed in $PREFIX"
