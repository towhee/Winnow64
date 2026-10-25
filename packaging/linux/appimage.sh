#!/bin/bash
# Build Winnow-<label>-<arch>.AppImage from an existing Linux build (run on the build VM,
# from the repository root; packaging/release.sh does this over SSH).
#
#   packaging/linux/appimage.sh [preset]          preset defaults to linux-release
#
# Output: out/Winnow-<label>-<arch>.AppImage, then a headless --selftest of the AppImage
# itself, offscreen and (with weston installed) on a headless Wayland compositor: the
# Linux analogue of the Windows installer smoke test. It catches a library or Qt plugin
# the bundle left out, which a test of the build tree cannot.
#
# Tools: linuxdeploy + linuxdeploy-plugin-qt bundle the libraries and Qt plugins,
# appimagetool packs the AppDir. They are PINNED releases, kept outside git in
# $APPIMAGE_TOOLS (default ~/appimage-tools; the master copy is ~/UTMShare/appimage-tools
# on Quark) and checked against packaging/linux/tools.sha256 before use. The AppImage
# runtime is passed in too, so the build never reaches the network.
#
# Compatibility: an AppImage needs the glibc it was built against or newer, so one built
# on Ubuntu 26.04 runs only on distros with glibc 2.43+.
#
# Exit codes: 0 = built and self-test passed, 2 = built but the self-test failed,
# 1 = not built.
set -uo pipefail

PRESET="${1:-linux-release}"
TOOLS="${APPIMAGE_TOOLS:-$HOME/appimage-tools}"
REPO="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$REPO" || exit 1

die() { echo "❌ $*"; exit 1; }

ARCH="$(uname -m)"
case "$ARCH" in
    x86_64|aarch64) ;;
    *) die "unsupported architecture $ARCH" ;;
esac
LABEL="$(sed -nE 's/^set\(WINNOW_VERSION_LABEL "([^"]*)"\).*/\1/p' CMakeLists.txt)"
[[ -n "$LABEL" ]] || die "WINNOW_VERSION_LABEL not found in CMakeLists.txt"
BUILD="build/$PRESET/app"
[[ -x "$BUILD/Winnow" ]] || die "$BUILD/Winnow not found: build $PRESET first"

# --- the pinned tools ---------------------------------------------------
LD="linuxdeploy-$ARCH.AppImage"
LDQT="linuxdeploy-plugin-qt-$ARCH.AppImage"
AIT="appimagetool-$ARCH.AppImage"
RUNTIME="runtime-$ARCH"
for f in "$LD" "$LDQT" "$AIT" "$RUNTIME"; do
    [[ -f "$TOOLS/$f" ]] || die "$TOOLS/$f missing (copy it from ~/UTMShare/appimage-tools on Quark)"
    want=$(awk -v f="$f" '$2 == f {print $1}' packaging/linux/tools.sha256)
    [[ -n "$want" ]] || die "$f is not listed in packaging/linux/tools.sha256"
    got=$(sha256sum "$TOOLS/$f" | awk '{print $1}')
    [[ "$got" == "$want" ]] || die "$f does not match packaging/linux/tools.sha256"
    chmod +x "$TOOLS/$f"
done
# linuxdeploy finds its Qt plugin by name on PATH
BIN="$(mktemp -d)"
trap 'rm -rf "$BIN"' EXIT
ln -s "$TOOLS/$LDQT" "$BIN/linuxdeploy-plugin-qt"
export PATH="$BIN:$PATH"
# the tools are AppImages themselves; run them without FUSE
export APPIMAGE_EXTRACT_AND_RUN=1
QMAKE="$(command -v qmake6 || echo /usr/lib/qt6/bin/qmake6)"
[[ -x "$QMAKE" ]] || die "qmake6 not found (apt install qt6-base-dev-tools)"
export QMAKE

# --- AppDir -------------------------------------------------------------
APPDIR="out/AppDir"
OUT="out/Winnow-$LABEL-$ARCH.AppImage"
rm -rf "$APPDIR" "$OUT"
mkdir -p "$APPDIR/usr/bin" || die "cannot create $APPDIR"
echo "=== AppDir ($LABEL, $ARCH) ==="
cp "$BUILD/Winnow" "$BUILD/Winnet" "$APPDIR/usr/bin/" || die "copy of the executables failed"
cp "$BUILD"/*.onnx "$APPDIR/usr/bin/" 2>/dev/null
# The ExifTool Perl distribution the Mac ships; Winnow looks beside its exe first.
cp -R ReleaseExtras/ExifTool "$APPDIR/usr/bin/ExifTool" || die "copy of ExifTool failed"
mkdir -p "$APPDIR/usr/share/licenses/winnow"
cp -R licenses/. "$APPDIR/usr/share/licenses/winnow/" 2>/dev/null
[[ -f LICENSE ]] && cp LICENSE "$APPDIR/usr/share/licenses/winnow/"

echo "=== linuxdeploy ==="
# wayland for Wayland sessions (xcb is deployed by default), offscreen for the headless
# self-test below.
export EXTRA_PLATFORM_PLUGINS="libqwayland.so;libqoffscreen.so"
# The Wayland platform plugin cannot open a window without its shell integration (and
# Winnow's title bar is the bradient decoration, Linux::useBradientDecoration). The Qt
# plugin deploys these only for the pre-6.10 plugin names (libqwayland-generic.so), so
# they are copied here and linuxdeploy bundles what they link.
QT_PLUGINS="$("$QMAKE" -query QT_INSTALL_PLUGINS)"
DEPS_ONLY=()
for d in wayland-shell-integration wayland-decoration-client wayland-graphics-integration-client; do
    [[ -d "$QT_PLUGINS/$d" ]] || die "$QT_PLUGINS/$d missing (apt install qt6-wayland)"
    mkdir -p "$APPDIR/usr/plugins"
    cp -R "$QT_PLUGINS/$d" "$APPDIR/usr/plugins/" || die "copy of $d failed"
    DEPS_ONLY+=(--deploy-deps-only "$APPDIR/usr/plugins/$d")
done
# libOpenGL.so.0 is on linuxdeploy's exclude list (assumed on every host), but a stock
# Ubuntu 26.04 desktop does not install it (libopengl0) while Ubuntu's Qt6Gui links it:
# the AppImage failed to start on the Linux-clean VM. It is GLVND's thin OpenGL
# dispatcher, forwarding to the host's libGLdispatch, so bundling it is safe.
LIBOPENGL="$(ldconfig -p | awk '/libOpenGL\.so\.0 /{print $NF; exit}')"
[[ -f "$LIBOPENGL" ]] || die "libOpenGL.so.0 not found (apt install libopengl0)"
"$TOOLS/$LD" --appdir "$APPDIR" \
    -e "$APPDIR/usr/bin/Winnow" -e "$APPDIR/usr/bin/Winnet" "${DEPS_ONLY[@]}" \
    -l "$LIBOPENGL" \
    -d packaging/linux/winnow.desktop -i packaging/linux/winnow.png \
    --plugin qt || die "linuxdeploy failed"

# Licences: linuxdeploy copies the copyright file of the Ubuntu package behind every
# library and plugin it bundles into usr/share/doc/<package>/; licenses/ (above) covers
# Winnow's own and the models'.

echo "=== appimagetool ==="
ARCH="$ARCH" "$TOOLS/$AIT" --no-appstream --runtime-file "$TOOLS/$RUNTIME" \
    "$APPDIR" "$OUT" || die "appimagetool failed"
chmod +x "$OUT"
echo "✅ $OUT ($(du -h "$OUT" | cut -f1))"

# --- self-test of the AppImage itself ----------------------------------
echo "=== selftest of the AppImage ==="
# Mounted through FUSE the way a user runs it (the runtime extracts instead whenever
# APPIMAGE_EXTRACT_AND_RUN is set at all, so it is removed rather than set to 0).
failed=0
selftest() {        # $1 = description; the environment for the run is already set
    if env -u APPIMAGE_EXTRACT_AND_RUN timeout 300 \
            "$OUT" --selftest "$REPO/tests/fixtures/images"; then
        echo "✅ AppImage selftest passed ($1)"
    else
        echo "❌ AppImage selftest failed ($1)"
        failed=1
    fi
}
QT_QPA_PLATFORM=offscreen selftest offscreen

# Again on a real (headless) Wayland compositor, when the VM has weston: a window must
# open through the bundled shell integration, which offscreen never exercises.
if command -v weston >/dev/null; then
    WL_RUN="$(mktemp -d)"
    chmod 700 "$WL_RUN"
    XDG_RUNTIME_DIR="$WL_RUN" weston --backend=headless --socket=winnow-test \
        --idle-time=0 >/dev/null 2>&1 &
    WESTON=$!
    sleep 3
    XDG_RUNTIME_DIR="$WL_RUN" WAYLAND_DISPLAY=winnow-test QT_QPA_PLATFORM=wayland \
        selftest "Wayland"
    kill "$WESTON" 2>/dev/null
    wait "$WESTON" 2>/dev/null
    rm -rf "$WL_RUN"
else
    echo "ℹ️  weston not installed: Wayland selftest skipped (apt install weston)"
fi
(( failed )) && exit 2
exit 0
