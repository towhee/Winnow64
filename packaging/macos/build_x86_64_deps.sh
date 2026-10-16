#!/bin/bash
# --------------------------------------------------------------------
# Build the STATIC x86_64 dependencies for the Intel macOS release.
#
# Homebrew on an Apple Silicon Mac is arm64-only, and an x86_64 Homebrew on a
# current macOS would ship dylibs whose minimum OS is the HOST OS (Intel Macs stop
# at macOS 26). So the Intel build links OpenCV, libtiff and libjpeg-turbo built
# here from source: x86_64, deployment target 12.0, static (nothing to bundle).
#
#   packaging/macos/build_x86_64_deps.sh          # -> ~/Projects/winnow-deps/macos-x86_64
#
# Then: WINNOW_ARCH=x86_64 packaging/macos/deploy.command   (preset mac-release-x86_64)
#
# One-time, ~10 minutes. Needs cmake, Qt Creator's ninja and nasm
# (`brew install nasm`; an arm64 nasm assembles x86_64 SIMD fine).
# --------------------------------------------------------------------
set -euo pipefail

JPEG_VER=3.1.2
TIFF_VER=4.7.1
OPENCV_VER=4.14.0          # keep in step with Homebrew opencv@4 (the arm64 build)

DEPS="${WINNOW_DEPS_DIR:-$HOME/Projects/winnow-deps}"
PREFIX="$DEPS/macos-x86_64"
NINJA="${NINJA:-$HOME/Qt/Tools/Ninja/ninja}"
NASM="${NASM:-$(command -v nasm || echo /opt/homebrew/bin/nasm)}"
mkdir -p "$DEPS/src" "$DEPS/build"

fetch() {   # url dir
    [[ -d "$DEPS/src/$2" ]] && return 0
    echo "⬇️  $1"
    curl -sSLf "$1" | tar xz -C "$DEPS/src"
}
fetch "https://github.com/libjpeg-turbo/libjpeg-turbo/archive/refs/tags/$JPEG_VER.tar.gz" "libjpeg-turbo-$JPEG_VER"
fetch "https://download.osgeo.org/libtiff/tiff-$TIFF_VER.tar.gz" "tiff-$TIFF_VER"
fetch "https://github.com/opencv/opencv/archive/refs/tags/$OPENCV_VER.tar.gz" "opencv-$OPENCV_VER"

# CMAKE_SYSTEM_PROCESSOR matters: without it OpenCV detects the arm64 HOST and
# builds NEON/KleidiCV code, which fails for -arch x86_64.
COMMON=(-G Ninja -DCMAKE_MAKE_PROGRAM="$NINJA" -DCMAKE_BUILD_TYPE=Release
        -DCMAKE_OSX_ARCHITECTURES=x86_64 -DCMAKE_SYSTEM_PROCESSOR=x86_64
        -DCMAKE_OSX_DEPLOYMENT_TARGET=12.0 -DCMAKE_INSTALL_PREFIX="$PREFIX"
        "-DCMAKE_IGNORE_PREFIX_PATH=/opt/homebrew;/usr/local"
        -DCMAKE_POSITION_INDEPENDENT_CODE=ON)

build() {   # name srcdir cmake-args...
    local name=$1 src=$2; shift 2
    echo "🏗  $name"
    rm -rf "$DEPS/build/$name"
    cmake -S "$DEPS/src/$src" -B "$DEPS/build/$name" "${COMMON[@]}" "$@" \
        > "$DEPS/$name.log" 2>&1
    cmake --build "$DEPS/build/$name" >> "$DEPS/$name.log" 2>&1
    cmake --install "$DEPS/build/$name" >> "$DEPS/$name.log" 2>&1
}

build jpeg "libjpeg-turbo-$JPEG_VER" \
    -DENABLE_SHARED=OFF -DENABLE_STATIC=ON -DWITH_TURBOJPEG=ON \
    -DCMAKE_ASM_NASM_COMPILER="$NASM"

build tiff "tiff-$TIFF_VER" \
    -DBUILD_SHARED_LIBS=OFF -Dtiff-tools=OFF -Dtiff-tests=OFF -Dtiff-docs=OFF \
    -Dtiff-contrib=OFF -Dtiff-cxx=OFF \
    -Djpeg=ON -DJPEG_INCLUDE_DIR="$PREFIX/include" -DJPEG_LIBRARY="$PREFIX/lib/libjpeg.a" \
    -Dzlib=ON -Dzstd=OFF -Dlzma=OFF -Dwebp=OFF -Dlerc=OFF -Djbig=OFF -Dlibdeflate=OFF

# imgcodecs uses the jpeg/tiff above (not OpenCV's bundled copies) so the static link
# has ONE copy of each. CMAKE_DISABLE_FIND_PACKAGE_Tiff stops FindTIFF from loading
# libtiff's exported config, which references an undefined CMath::CMath target.
build opencv "opencv-$OPENCV_VER" \
    -DCMAKE_SYSTEM_NAME=Darwin -DWITH_KLEIDICV=OFF -DCPU_BASELINE=SSE4_2 \
    -DBUILD_SHARED_LIBS=OFF -DBUILD_LIST=core,imgproc,imgcodecs,video,dnn,photo \
    -DBUILD_TESTS=OFF -DBUILD_PERF_TESTS=OFF -DBUILD_EXAMPLES=OFF -DBUILD_DOCS=OFF \
    -DBUILD_opencv_apps=OFF -DBUILD_JAVA=OFF -DWITH_PYTHON=OFF \
    -DBUILD_opencv_python3=OFF -DBUILD_opencv_python2=OFF \
    -DWITH_FFMPEG=OFF -DWITH_GSTREAMER=OFF -DWITH_AVFOUNDATION=OFF -DWITH_QT=OFF \
    -DWITH_GTK=OFF -DWITH_OPENEXR=OFF -DWITH_JASPER=OFF -DWITH_OPENJPEG=OFF \
    -DWITH_AVIF=OFF -DWITH_EIGEN=OFF -DWITH_VTK=OFF -DWITH_OPENCL=OFF -DWITH_LAPACK=OFF \
    -DWITH_PROTOBUF=ON -DBUILD_PROTOBUF=ON \
    -DWITH_JPEG=ON -DBUILD_JPEG=OFF \
    -DJPEG_INCLUDE_DIR="$PREFIX/include" -DJPEG_LIBRARY="$PREFIX/lib/libjpeg.a" \
    -DWITH_TIFF=ON -DBUILD_TIFF=OFF -DCMAKE_DISABLE_FIND_PACKAGE_Tiff=ON \
    -DTIFF_INCLUDE_DIR="$PREFIX/include" -DTIFF_LIBRARY="$PREFIX/lib/libtiff.a" \
    -DBUILD_ZLIB=OFF -DWITH_PNG=ON -DBUILD_PNG=ON -DWITH_WEBP=ON -DBUILD_WEBP=ON \
    -DWITH_IPP=ON -DOPENCV_GENERATE_PKGCONFIG=OFF

for a in "$PREFIX"/lib/*.a "$PREFIX"/lib/opencv4/3rdparty/*.a; do
    [[ "$(lipo -archs "$a")" == x86_64 ]] || { echo "❌ not x86_64: $a"; exit 1; }
done
echo "✅ x86_64 deps installed in $PREFIX"
