#!/bin/zsh
# --------------------------------------------------------------------
# Winnow macOS Deployment
#   build -> stage -> sign -> notarize -> DMG -> archive -> (upload)
#
# This script is self-locating: it derives every path from its own
# location in the repo, so it works unchanged on any Mac after a
# `git clone`. Machine-specific values (Qt path, signing identity,
# notary profile, upload target) live in ../config.sh — copy
# ../config.sh.example to ../config.sh and edit it once per machine.
#
# Usage:
#   chmod +x deploy.command
#   ./deploy.command           # interactive menu
# Or double-click in Finder.
#
# Requirements (see ../README.md for one-time setup):
#   • Xcode command line tools
#   • Qt 6.9.2 (macos) at the path in config.sh
#   • Homebrew deps: opencv ffmpeg webp
#   • Developer ID Application certificate in the login keychain
#   • notarytool keychain profile (default name: AC_APIKEY)
# --------------------------------------------------------------------

# Launch in Terminal if double-clicked from Finder
if [[ -z "$TERM_PROGRAM" ]]; then
    osascript <<EOF
tell application "Terminal"
    do script "cd \"$(dirname "$0")\" && ./deploy.command"
    activate
end tell
EOF
    exit 0
fi

# NOTE: deliberately NOT using `set -e`. This is an interactive menu; a
# failed step should report and return to the menu, not kill the shell.
# Each function returns non-zero on failure and the caller checks it.

# --- Locate ourselves in the repo ------------------------------------
SCRIPT_DIR="${0:A:h}"                 # packaging/macos
PACKAGING_DIR="${SCRIPT_DIR:h}"       # packaging
REPO_ROOT="${PACKAGING_DIR:h}"        # repo root (Winnow64)

# --- Load per-machine config (lives beside this script; macOS-specific) ------
CONFIG_FILE="$SCRIPT_DIR/config.sh"
if [[ ! -f "$CONFIG_FILE" ]]; then
    echo "❌ Missing $CONFIG_FILE"
    echo "   Run: cp \"$SCRIPT_DIR/config.sh.example\" \"$CONFIG_FILE\" and edit it."
    exit 1
fi
source "$CONFIG_FILE"

# Defaults for anything the config omitted
: ${HOMEBREW_PREFIX:=/opt/homebrew}
: ${NOTARY_KEYCHAIN_PROFILE:=AC_APIKEY}
: ${UPLOAD_ENABLED:=false}
: ${SERVER_WEB_ROOT:=/var/www/html}
: ${WEB_BASE_URL:=https://winnow.ca}
: ${UPLOAD_SUBDIR:=winnow_mac/test}

# Server-derived paths (used by upload + promote)
MAC_DIR="${SERVER_WEB_ROOT}/winnow_mac"
INDEX_PATH="${SERVER_WEB_ROOT}/index.html"

# --- Derived paths ----------------------------------------------------
APP_BUNDLE="Winnow.app"
QTBIN_DIR="${QT_DIR}/bin"

# Canonical CMakePresets release output
BUILD_DIR="${REPO_ROOT}/build/mac-release"
RELEASE_APP_PATH="${BUILD_DIR}/${APP_BUNDLE}"

# All deploy artifacts live under the git-ignored out/ dir
OUT_DIR="${REPO_ROOT}/out"
STAGING_DIR="${OUT_DIR}/Staging"
STAGING_APP_PATH="${STAGING_DIR}/${APP_BUNDLE}"
STAGE_FRAMEWORKS_DIR="${STAGING_APP_PATH}/Contents/Frameworks"
NOTARIZED_DIR="${OUT_DIR}/Notarized"
DMG_DIR="${OUT_DIR}/DMG"

ENTITLEMENTS="${SCRIPT_DIR}/entitlements.plist"
VERIFY_SCRIPT="${SCRIPT_DIR}/verify_bundle.command"

# Version + names are derived AFTER build/stage by reading the built bundle.
WINNOW_VERSION=""
WINNOW_VERSION_NAME=""
NOTARIZED_APP_PATH=""
NOTARIZED_ZIP_PATH=""
DMG_PATH=""

# Read CFBundleShortVersionString from a built bundle and (re)compute names.
function resolve_version() {
    local app="$1"
    WINNOW_VERSION=$(/usr/libexec/PlistBuddy -c "Print CFBundleShortVersionString" \
        "$app/Contents/Info.plist" 2>/dev/null)
    if [[ -z "$WINNOW_VERSION" || "$WINNOW_VERSION" == "1.0" ]]; then
        echo "⚠️  Bundle version is '$WINNOW_VERSION' — expected the project version."
        echo "    Ensure CMakeLists project(VERSION ...) is set and the app was rebuilt."
    fi
    WINNOW_VERSION_NAME="Winnow${WINNOW_VERSION}"
    NOTARIZED_APP_PATH="${NOTARIZED_DIR}/${WINNOW_VERSION_NAME}.app"
    NOTARIZED_ZIP_PATH="${NOTARIZED_DIR}/${WINNOW_VERSION_NAME}.zip"
    DMG_PATH="${DMG_DIR}/${WINNOW_VERSION_NAME}.dmg"
}

# Ensure WINNOW_VERSION and its derived paths (DMG_PATH, NOTARIZED_APP_PATH, …) are
# populated when a menu step runs standalone in a fresh session, where no prior
# build/stage/notarize has set them. The version is identical across all bundles for
# a given build, so resolve it from a FIXED-path bundle (staging, else release); that
# version then drives the version-named NOTARIZED/DMG paths the later steps check.
function ensure_version() {
    [[ -n "$WINNOW_VERSION" ]] && return 0
    if [[ -d "$STAGING_APP_PATH" ]]; then resolve_version "$STAGING_APP_PATH"
    elif [[ -d "$RELEASE_APP_PATH" ]]; then resolve_version "$RELEASE_APP_PATH"
    fi
}

# --- Build (CMake preset) --------------------------------------------
function build() {
    [[ -d "$QT_DIR" ]] || { echo "❌ QT_DIR not found: $QT_DIR (set it in config.sh)"; return 1; }
    echo "🏗  Building (preset mac-release) against Qt: $QT_DIR"

    # The mac-release preset uses the Ninja generator. Qt Creator bundles a ninja
    # that is usually NOT on the shell PATH; make it findable so a CLI (re)configure
    # works. NINJA may be overridden in config.sh.
    if ! command -v ninja >/dev/null 2>&1; then
        local bundled_ninja="${NINJA:-$HOME/Qt/Tools/Ninja/ninja}"
        if [[ -x "$bundled_ninja" ]]; then
            export PATH="${bundled_ninja:h}:$PATH"
        else
            echo "❌ ninja not on PATH or at $bundled_ninja (set NINJA in config.sh)"; return 1
        fi
    fi

    # CMakePresets resolves CMAKE_PREFIX_PATH from $env{QT_DIR}; export it so the
    # build links the Qt chosen in config.sh (not whatever CMake auto-detects, e.g.
    # a Homebrew Qt). Reconfigure if the build dir was configured against a
    # different Qt — otherwise an existing cache silently keeps the old one.
    export QT_DIR
    local need_configure=false
    if [[ ! -f "$BUILD_DIR/CMakeCache.txt" ]]; then
        need_configure=true
    else
        local cached_qt
        cached_qt=$(grep -E "^Qt6_DIR:" "$BUILD_DIR/CMakeCache.txt" | cut -d= -f2)
        if [[ "$cached_qt" != "$QT_DIR"/* ]]; then
            echo "   ↻ build dir uses a different Qt ($cached_qt); reconfiguring for $QT_DIR"
            rm -f "$BUILD_DIR/CMakeCache.txt"   # force Qt re-detection (keeps object files)
            need_configure=true
        fi
    fi
    # CMake resolves presets relative to the current working directory (the build
    # preset has no -S to point it at the repo), so run both from REPO_ROOT in a
    # subshell. Otherwise launching the script from elsewhere (e.g. $HOME) makes
    # `cmake --build --preset` look for CMakePresets.json in the wrong place.
    ( cd "$REPO_ROOT" || exit 1
      $need_configure && { cmake --preset mac-release || exit 1; }
      cmake --build --preset mac-release ) || return 1
    if [[ ! -d "$RELEASE_APP_PATH" ]]; then
        echo "❌ Build did not produce $RELEASE_APP_PATH"
        return 1
    fi
    resolve_version "$RELEASE_APP_PATH"
    echo "   ✓ Built Winnow ${WINNOW_VERSION}"
}

# --- Deploy Qt frameworks --------------------------------------------
function deployQt() {
    if [[ ! -x "$QTBIN_DIR/macdeployqt" ]]; then
        echo "❌ macdeployqt not found at $QTBIN_DIR/macdeployqt (check QT_DIR in config.sh)"
        return 1
    fi
    echo "⚙️  Running macdeployqt…"
    "$QTBIN_DIR/macdeployqt" "$STAGING_APP_PATH" -appstore-compliant -verbose=2 || return 1
    echo "   ✓ Qt frameworks deployed"
}

# --- Fix dependent dylibs (OpenCV, FFmpeg, tiff, turbojpeg, WebP) -----
function dependencies() {
    echo "🛠  Fixing dylibs…"
    local added_new_libs=true
    while $added_new_libs; do
        added_new_libs=false
        find "$STAGE_FRAMEWORKS_DIR" -type f -name "*.dylib" -print0 |
        while IFS= read -r -d '' dylib; do
            base=$(basename "$dylib")

            # Thin universal binaries to arm64
            if lipo -info "$dylib" 2>/dev/null | grep -q "x86_64"; then
                lipo "$dylib" -thin arm64 -output "${dylib}.arm64" && mv "${dylib}.arm64" "$dylib"
            fi

            install_name_tool -id "@executable_path/../Frameworks/$base" "$dylib"

            otool -L "$dylib" | tail -n +2 | awk '{print $1}' |
            while read -r dep; do
                case "$dep" in
                    /usr/*|/System/*|/Library/*|@executable_path/*) continue ;;
                esac
                dep_base=$(basename "$dep")
                install_name_tool -change "$dep" \
                    "@executable_path/../Frameworks/$dep_base" "$dylib"
                if [[ ! -f "$STAGE_FRAMEWORKS_DIR/$dep_base" ]]; then
                    candidate=$(find "$HOMEBREW_PREFIX" -type f -name "$dep_base" -print -quit 2>/dev/null)
                    if [[ -n "$candidate" ]]; then
                        cp "$candidate" "$STAGE_FRAMEWORKS_DIR/$dep_base"
                        added_new_libs=true
                    else
                        echo "      ⚠️  Could not locate $dep_base in Homebrew."
                    fi
                fi
            done
        done
    done

    # Thin the nested Winnet helper's bundled binaries to arm64. The loop above
    # only covers Contents/Frameworks loose *.dylib; the Winnet copy lives under
    # Contents/MacOS/Winnets and its frameworks store the Mach-O without a .dylib
    # extension, so match by Mach-O content (lipo) rather than by name.
    local winnets_dir="${STAGING_APP_PATH}/Contents/MacOS/Winnets"
    if [[ -d "$winnets_dir" ]]; then
        find "$winnets_dir" -type f | while IFS= read -r macho; do
            if lipo -archs "$macho" 2>/dev/null | grep -q "x86_64"; then
                echo "      thinning Winnets/${macho#$winnets_dir/} → arm64"
                lipo "$macho" -thin arm64 -output "${macho}.arm64" && mv "${macho}.arm64" "$macho"
            fi
        done
    fi

    # Reinstate Homebrew WebP libs (Qt ships incompatible copies)
    rm -f "$STAGE_FRAMEWORKS_DIR"/libwebp*.dylib
    cp "$HOMEBREW_PREFIX"/lib/libwebp*.dylib "$STAGE_FRAMEWORKS_DIR/" 2>/dev/null || true
    cp "$HOMEBREW_PREFIX"/lib/libsharpyuv.0.dylib "$STAGE_FRAMEWORKS_DIR/" 2>/dev/null || true

    # Strip absolute Homebrew LC_RPATHs from every bundled Mach-O. The link step
    # bakes $HOMEBREW_PREFIX/lib into the executable (OpenCV/FFmpeg/etc. live
    # there), and it is searched BEFORE @executable_path/../Frameworks. On any Mac
    # that has a Homebrew Qt installed, dyld then resolves @rpath/QtCore against
    # that Homebrew Qt instead of the bundled one — a version skew that crashes the
    # cocoa plugin at startup (Symbol not found). Vanilla Macs lack the path and
    # fall through to the bundle, so this only bites dev machines, but the rpath
    # has no business shipping. Remove it so only the bundled Frameworks resolve.
    find "$STAGING_APP_PATH/Contents" -type f -perm +111 2>/dev/null | while IFS= read -r macho; do
        otool -l "$macho" 2>/dev/null |
            awk '/LC_RPATH/{g=1} g&&/path /{print $2; g=0}' |
            grep "^${HOMEBREW_PREFIX}" |
        while IFS= read -r rp; do
            install_name_tool -delete_rpath "$rp" "$macho" 2>/dev/null &&
                echo "      removed rpath $rp from ${macho#$STAGING_APP_PATH/}"
        done
    done
    echo "   ✓ dylibs fixed"
}

function strip_symbols() {
    echo "🪚  Stripping symbols…"
    find "$STAGING_APP_PATH/Contents" -type f -perm +111 -exec strip -x {} \; 2>/dev/null || true
    echo "   ✓ Stripped"
}

# --- Pre-sign sanity checks ------------------------------------------
function check_release() {
    local ok=true
    echo "🔍 Verifying staged app…"

    [[ -d "$STAGING_APP_PATH" ]] || { echo "❌ Staged app missing"; ok=false; }

    if security find-certificate -c "$DEVELOPER_ID" >/dev/null 2>&1; then
        echo "   ✓ Developer certificate present"
    else
        echo "❌ Developer certificate not found: $DEVELOPER_ID"; ok=false
    fi

    if xcrun notarytool history --keychain-profile "$NOTARY_KEYCHAIN_PROFILE" >/dev/null 2>&1; then
        echo "   ✓ Notary profile OK ($NOTARY_KEYCHAIN_PROFILE)"
    else
        echo "❌ Notary profile invalid/missing: $NOTARY_KEYCHAIN_PROFILE"; ok=false
    fi

    if plutil -lint "$ENTITLEMENTS" >/dev/null 2>&1; then
        echo "   ✓ Entitlements valid"
    else
        echo "❌ Entitlements invalid: $ENTITLEMENTS"; ok=false
    fi

    # Structural vanilla-Mac readiness (load deps, rpaths, dangling deps, arch,
    # min-OS). Gatekeeper checks are skipped here — the app is not signed/notarized
    # yet — and run later via verify() against the notarized bundle.
    echo "   — running structural readiness checks —"
    if "$VERIFY_SCRIPT" "$STAGING_APP_PATH" --structural-only; then
        echo "   ✓ Structural readiness OK"
    else
        echo "❌ Structural readiness FAILED (see above)"; ok=false
    fi

    [[ "$ok" == true ]] && { echo "✅ Checks passed"; return 0; } || { echo "❌ Checks failed"; return 1; }
}

# --- Stage a fresh app from the release build ------------------------
function stage() {
    [[ -d "$RELEASE_APP_PATH" ]] || { echo "❌ No release build at $RELEASE_APP_PATH — run Build first."; return 1; }
    resolve_version "$RELEASE_APP_PATH"

    echo "📦 Staging Winnow ${WINNOW_VERSION}…"
    rm -rf "$STAGING_DIR"
    mkdir -p "$STAGING_DIR"
    cp -R "$RELEASE_APP_PATH" "$STAGING_APP_PATH" || return 1

    # Static entitlements from the repo (no longer generated inline)
    cp "$ENTITLEMENTS" "$STAGING_APP_PATH/Contents/entitlements.plist" || return 1

    deployQt        || return 1
    dependencies    || return 1
    strip_symbols
    check_release   || return 1
    echo "   ✓ Staged: $STAGING_APP_PATH"
}

# --- Code sign the STAGED app only (never the build output) ----------
function sign() {
    [[ -d "$STAGING_APP_PATH" ]] || { echo "❌ Nothing staged — run Stage first."; return 1; }
    echo "✍️  Signing staged app…"

    # Strip any inherited signatures first
    find "$STAGING_APP_PATH" -type f -perm +111 -print0 | while IFS= read -r -d '' f; do
        codesign --remove-signature "$f" 2>/dev/null || true
    done

    # --deep signs nested code (the Winnets/Winnet helper + its bundled Qt, the
    # ExifTool perl script + exiftool_wrapper, and the Frameworks/PlugIns dylibs)
    # inside-out in one pass. Without it codesign signs only the outer bundle and
    # --verify --strict rejects the unsigned subcomponents (e.g. ExifTool/exiftool).
    # This matches the legacy WinnowInstall sign step that produced notarized 2.04.
    codesign --deep --force --options runtime --timestamp \
        --entitlements "$STAGING_APP_PATH/Contents/entitlements.plist" \
        --sign "$DEVELOPER_ID" "$STAGING_APP_PATH" || return 1

    echo "🔍 Verifying signature…"
    codesign --verify --deep --strict --verbose=2 "$STAGING_APP_PATH" || return 1
    echo "   ✓ Signed (Gatekeeper will report 'Unnotarized' until notarization — expected)"
}

# --- Notarize + staple ------------------------------------------------
function notarize() {
    [[ -d "$STAGING_APP_PATH" ]] || { echo "❌ Nothing staged."; return 1; }
    resolve_version "$STAGING_APP_PATH"
    mkdir -p "$NOTARIZED_DIR"

    echo "🔁 Creating versioned copy: $NOTARIZED_APP_PATH"
    rm -rf "$NOTARIZED_APP_PATH"
    cp -R "$STAGING_APP_PATH" "$NOTARIZED_APP_PATH" || return 1

    setopt local_options NULL_GLOB
    rm -f "$NOTARIZED_DIR"/*.zip

    echo "📦 Zipping for notarization…"
    ditto -c -k --keepParent "$NOTARIZED_APP_PATH" "$NOTARIZED_ZIP_PATH" || return 1

    echo "📤 Submitting to Apple (waits for result)…"
    xcrun notarytool submit "$NOTARIZED_ZIP_PATH" \
        --keychain-profile "$NOTARY_KEYCHAIN_PROFILE" --wait || return 1

    echo "📌 Stapling…"
    xcrun stapler staple "$NOTARIZED_APP_PATH" || return 1
    rm -f "$NOTARIZED_DIR"/*.zip

    echo "🔍 Gatekeeper assessment…"
    spctl --assess --type exec --verbose=2 "$NOTARIZED_APP_PATH" || return 1
    echo "✅ Notarized: $NOTARIZED_APP_PATH"
}

# --- Build a distributable DMG with an /Applications symlink ----------
function disk_image() {
    ensure_version   # set NOTARIZED_APP_PATH/DMG_PATH when run standalone
    [[ -d "$NOTARIZED_APP_PATH" ]] || { echo "❌ No notarized app — run Notarize first."; return 1; }
    mkdir -p "$DMG_DIR"
    rm -f "$DMG_PATH"

    local tmp="${OUT_DIR}/dmg_tmp"
    rm -rf "$tmp"; mkdir -p "$tmp"
    cp -R "$NOTARIZED_APP_PATH" "$tmp/${APP_BUNDLE}"
    ln -s /Applications "$tmp/Applications"

    echo "📦 Creating DMG: $DMG_PATH"
    hdiutil create -volname "$WINNOW_VERSION_NAME" \
        -srcfolder "$tmp" -ov -format UDZO "$DMG_PATH" || { rm -rf "$tmp"; return 1; }
    rm -rf "$tmp"
    echo "   ✓ DMG: $DMG_PATH"
}

# --- Archive a version-named copy ------------------------------------
function archive() {
    ensure_version   # set NOTARIZED_APP_PATH/WINNOW_VERSION when run standalone
    [[ -d "$NOTARIZED_APP_PATH" ]] || { echo "❌ No notarized app."; return 1; }
    [[ -n "$ARCHIVE_DIR" ]] || { echo "⚠️  ARCHIVE_DIR unset; skipping."; return 0; }
    local dated="${ARCHIVE_DIR}/Winnow${WINNOW_VERSION}_$(date +%Y-%m-%d).app"
    mkdir -p "$ARCHIVE_DIR"
    rm -rf "$dated"
    cp -R "$NOTARIZED_APP_PATH" "$dated" && echo "   ✓ Archived: $dated"
}

# --- Optional plain upload to a test dir (separate from promote) -----
function upload() {
    [[ "$UPLOAD_ENABLED" == true ]] || { echo "ℹ️  Upload disabled (UPLOAD_ENABLED=false)."; return 0; }
    ensure_version   # set DMG_PATH when run standalone (see ensure_version)
    [[ -f "$DMG_PATH" ]] || { echo "❌ No DMG to upload (looked for $DMG_PATH)."; return 1; }
    local target="${SERVER_USER}@${SERVER_HOST}:${SERVER_WEB_ROOT}/${UPLOAD_SUBDIR}/"
    echo "📡 Uploading DMG to $target…"
    scp -i "$SERVER_SSH_KEY" "$DMG_PATH" "$target" || return 1
    echo "   ✓ Uploaded"
}

# --- Promote: publish a release to the server ------------------------
# Archives the current DMG, uploads the new one to current/, and updates the
# landing-page download link. Outward-facing PRODUCTION writes: backs up what it
# overwrites, never deletes the old DMG, and confirms before the first write.
#   promote            interactive (detect old version, confirm, then publish)
#   promote --dry-run  print every ssh/scp/sed it WOULD run; touches nothing
function promote() {
    local dry=false
    [[ "$1" == "--dry-run" ]] && dry=true

    for v in SERVER_USER SERVER_HOST SERVER_SSH_KEY SERVER_WEB_ROOT WEB_BASE_URL; do
        [[ -n "${(P)v}" ]] || { echo "❌ $v not set in config.sh"; return 1; }
    done
    # The key/app/DMG only have to exist for a REAL run; a dry-run is fully offline.
    if ! $dry; then
        [[ -f "$SERVER_SSH_KEY" ]] || { echo "❌ SERVER_SSH_KEY not found: $SERVER_SSH_KEY"; return 1; }
    fi

    local SSH="ssh -i $SERVER_SSH_KEY ${SERVER_USER}@${SERVER_HOST}"

    # run-or-print helper
    run() { if $dry; then echo "   [dry-run] $*"; else eval "$@"; fi }

    # 1) NEW version + DMG present
    local NEW
    if [[ -d "$NOTARIZED_APP_PATH" ]]; then
        resolve_version "$NOTARIZED_APP_PATH"; NEW="$WINNOW_VERSION"
    elif $dry; then
        printf "   [dry-run] new version (no notarized app found): "; read NEW
        DMG_PATH="${DMG_DIR}/Winnow${NEW}.dmg"   # derive expected path for the preview
    else
        echo "❌ No notarized app — run Notarize first."; return 1
    fi
    if ! $dry; then
        [[ -f "$DMG_PATH" ]] || { echo "❌ DMG not found: $DMG_PATH — run Create DMG first."; return 1; }
    fi

    # 2) Verify gate (full, incl. Gatekeeper)
    if $dry; then
        echo "   [dry-run] would run: $VERIFY_SCRIPT \"$NOTARIZED_APP_PATH\"  (publish blocked on failure)"
    else
        echo "🔒 Verify gate: $NOTARIZED_APP_PATH"
        "$VERIFY_SCRIPT" "$NOTARIZED_APP_PATH" || { echo "❌ Verify failed — refusing to publish."; return 1; }
    fi

    # 3) Determine OLD version (live detection for a real run; prompt for a dry-run)
    local OLD
    if $dry; then
        printf "   [dry-run] old (currently published) version: "; read OLD
    else
        echo "🔎 Detecting current published version…"
        local listing
        listing=$($SSH "ls -1 ${MAC_DIR}/current/Winnow*.dmg 2>/dev/null") || {
            echo "❌ Could not reach server or list current/ (check SERVER_SSH_KEY)."; return 1; }
        OLD=$(echo "$listing" | sed -nE 's#.*/Winnow([0-9.]+)\.dmg#\1#p' | head -1)
        echo "   current/: ${listing:-<empty>}"
        printf "   Old version detected as '%s'. Enter to accept, or type the correct version: " "$OLD"
        read reply; [[ -n "$reply" ]] && OLD="$reply"
    fi
    [[ -n "$OLD" && -n "$NEW" ]] || { echo "❌ Need both OLD and NEW versions."; return 1; }
    [[ "$OLD" == "$NEW" ]] && { echo "❌ OLD ($OLD) == NEW ($NEW); nothing to promote."; return 1; }

    # 4) Plan + confirm
    cat <<EOF

================ PROMOTE PLAN  (old=$OLD  new=$NEW) ================
  1. archive : mv  current/Winnow$OLD.dmg  ->  $OLD/Winnow$OLD.dmg
  2. upload  : $DMG_PATH  ->  current/Winnow$NEW.dmg
  3. index   : back up index.html, bump Mac link  $OLD -> $NEW
  server     : ${SERVER_USER}@${SERVER_HOST}:${MAC_DIR}
$([[ $dry == true ]] && echo "  MODE: DRY-RUN (nothing will be written)")
===================================================================
EOF
    if ! $dry; then
        printf "Type 'yes' to publish to PRODUCTION: "
        read confirm; [[ "$confirm" == "yes" ]] || { echo "Aborted."; return 1; }
    fi

    # 5) Archive old (move, never delete; skip if already archived)
    echo "📦 Archiving current/Winnow$OLD.dmg → $OLD/…"
    run "$SSH 'mkdir -p ${MAC_DIR}/$OLD && if [ -f ${MAC_DIR}/current/Winnow$OLD.dmg ]; then mv ${MAC_DIR}/current/Winnow$OLD.dmg ${MAC_DIR}/$OLD/Winnow$OLD.dmg; else echo already-archived-or-absent; fi'"

    # 6) Upload new
    echo "⬆️  Uploading Winnow$NEW.dmg → current/…"
    run "scp -i $SERVER_SSH_KEY '$DMG_PATH' ${SERVER_USER}@${SERVER_HOST}:${MAC_DIR}/current/Winnow$NEW.dmg"

    # 7) Update index.html (server-side backup, then precise edit via download/diff/upload)
    echo "📝 Updating index.html (Mac link $OLD → $NEW)…"
    local stamp; stamp=$(date +%Y%m%d-%H%M%S)
    run "$SSH 'cp ${INDEX_PATH} ${INDEX_PATH}.bak-$stamp'"
    if $dry; then
        echo "   [dry-run] download ${INDEX_PATH}; sed 's/Winnow$OLD.dmg/Winnow$NEW.dmg/' and 's/Winnow $OLD (Apple Silicon/Winnow $NEW (Apple Silicon/'; diff; upload"
    else
        local tmp; tmp=$(mktemp -d)
        scp -i "$SERVER_SSH_KEY" "${SERVER_USER}@${SERVER_HOST}:${INDEX_PATH}" "$tmp/index.html" || { echo "❌ index.html download failed"; rm -rf "$tmp"; return 1; }
        sed -e "s#Winnow${OLD}\.dmg#Winnow${NEW}.dmg#g" \
            -e "s#Winnow ${OLD} (Apple Silicon#Winnow ${NEW} (Apple Silicon#g" \
            "$tmp/index.html" > "$tmp/index.new.html"
        echo "   --- index.html change ---"
        diff "$tmp/index.html" "$tmp/index.new.html" || true
        if ! diff -q "$tmp/index.html" "$tmp/index.new.html" >/dev/null; then
            printf "   Apply this index.html change? (yes): "; read ic
            if [[ "$ic" == "yes" ]]; then
                scp -i "$SERVER_SSH_KEY" "$tmp/index.new.html" "${SERVER_USER}@${SERVER_HOST}:${INDEX_PATH}" || { echo "❌ index.html upload failed (backup at ${INDEX_PATH}.bak-$stamp)"; rm -rf "$tmp"; return 1; }
                echo "   ✓ index.html updated (backup: ${INDEX_PATH}.bak-$stamp)"
            else
                echo "   ⏭  index.html left unchanged."
            fi
        else
            echo "   ⚠️  No matching link found to update — check index.html manually."
        fi
        rm -rf "$tmp"
    fi

    # 8) Post-publish check
    echo "🔍 Post-publish:"
    run "$SSH 'ls -l ${MAC_DIR}/current ${MAC_DIR}/$OLD'"
    if ! $dry; then
        echo "   HTTP check: ${WEB_BASE_URL}/winnow_mac/current/Winnow$NEW.dmg"
        curl -sI "${WEB_BASE_URL}/winnow_mac/current/Winnow$NEW.dmg" | head -1
    fi
    echo "✅ Promote complete (old=$OLD new=$NEW)."
}

# --- Verify vanilla-Mac readiness ------------------------------------
# Runs the full check set (structural + Gatekeeper) against the notarized app
# if it exists, otherwise the staged app (structural only, since it is not yet
# notarized). Used on demand (menu V) and at the end of the full pipeline.
function verify() {
    # Resolve version from whatever bundle exists so NOTARIZED_APP_PATH is set
    # even when this is run standalone (menu V) before a build in this session.
    if [[ -d "$STAGING_APP_PATH" ]]; then resolve_version "$STAGING_APP_PATH"
    elif [[ -d "$RELEASE_APP_PATH" ]]; then resolve_version "$RELEASE_APP_PATH"
    fi
    if [[ -d "$NOTARIZED_APP_PATH" ]]; then
        "$VERIFY_SCRIPT" "$NOTARIZED_APP_PATH"
    elif [[ -d "$STAGING_APP_PATH" ]]; then
        echo "ℹ️  No notarized app yet — verifying STAGED app (structural only)."
        "$VERIFY_SCRIPT" "$STAGING_APP_PATH" --structural-only
    else
        echo "❌ Nothing to verify — run Stage (and Notarize) first."
        return 1
    fi
}

function print_config() {
    echo "REPO_ROOT          = $REPO_ROOT"
    echo "QT_DIR             = $QT_DIR"
    echo "HOMEBREW_PREFIX    = $HOMEBREW_PREFIX"
    echo "DEVELOPER_ID       = $DEVELOPER_ID"
    echo "NOTARY_PROFILE     = $NOTARY_KEYCHAIN_PROFILE"
    echo "RELEASE_APP_PATH   = $RELEASE_APP_PATH"
    echo "OUT_DIR            = $OUT_DIR"
    echo "ENTITLEMENTS       = $ENTITLEMENTS"
    echo "ARCHIVE_DIR        = $ARCHIVE_DIR"
    echo "UPLOAD_ENABLED     = $UPLOAD_ENABLED"
    [[ -d "$RELEASE_APP_PATH" ]] && { resolve_version "$RELEASE_APP_PATH"; echo "VERSION (built)    = $WINNOW_VERSION"; }
}

# --- Menu -------------------------------------------------------------
while true; do
    clear
    echo "=================== Winnow Deployment ==================="
    echo "0) Build (cmake --build --preset mac-release)"
    echo "1) Stage fresh app"
    echo "2) Sign staged app"
    echo "3) Notarize + staple"
    echo "4) Create DMG"
    echo "5) Archive"
    echo "6) Upload DMG to test dir"
    echo "7) Promote + publish to server (archive old → upload new → update index)"
    echo "V) Verify vanilla-Mac readiness"
    echo "D) Promote DRY-RUN (print server commands, write nothing)"
    echo "A) FULL PIPELINE (0-6 + verify; does NOT publish)"
    echo "P) Print config"
    echo "Q) Quit"
    echo "---------------------------------------------------------"
    printf "Select option: "
    read choice

    case "$choice" in
        0) build ;;
        1) stage ;;
        2) sign ;;
        3) notarize ;;
        4) disk_image ;;
        5) archive ;;
        6) upload ;;
        7) promote ;;
        v|V) verify ;;
        d|D) promote --dry-run ;;
        a|A) build && stage && sign && notarize && disk_image && archive && verify && upload ;;
        p|P) print_config ;;
        q|Q) exit 0 ;;
        *) echo "❌ Invalid choice"; sleep 1 ;;
    esac

    echo ""
    printf "Press Enter to return to menu..."
    read -r _dummy
done
