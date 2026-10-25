#!/bin/zsh
# --------------------------------------------------------------------
# Winnow all-platform release, driven from one Mac (Quark).
#
#   packaging/release.sh prepare <label>   set the label, commit "Release <label>", push
#   packaging/release.sh build             build + package + test every platform
#   packaging/release.sh publish           print the publish plan (writes nothing)
#   packaging/release.sh publish --yes     publish every file to one GitHub release
#                                          (add --accept-failures to publish past a
#                                          failed test; the plan lists them)
#   packaging/release.sh status            what is built for the current label
#
# The magic phrases map onto it: "Build release" = prepare (when the label is
# new) + build; "Publish release" = publish, then publish --yes after the user's
# yes. See WinnowShared/Deployment.txt section 1 and notes/Deploy.md.
#
# Platforms:
#   mac      this Mac   deploy.command --steps 0,1,2,3,4,V      Winnow<label>.dmg
#   intel    this Mac   WINNOW_ARCH=x86_64 ... (same steps)       Winnow<label>_Intel.dmg
#   windows  WIN_HOST   deploy.ps1 -Steps 0,1,2,3,V + smoketest   WinnowSetup-<label>.exe
#   linux    LINUX_HOST linux-release build + ctest (packaging not built yet)
#
# Every platform builds the SAME commit: the remote trees are refreshed with
# `git archive <commit> | tar -x`, so the commit must be clean (no uncommitted
# changes). Results go to out/release/<label>/ with SHA256SUMS and COMMIT.
#
# Per-machine overrides (env or packaging/macos/config.sh):
#   WIN_HOST (winvm)  WIN_REPO (Winnow64, relative to the remote home)
#   LINUX_HOST (roryhill@192.168.64.3)  LINUX_REPO (Winnow64)
#   RELEASE_PLATFORMS ("mac intel windows linux")
# --------------------------------------------------------------------

SCRIPT_DIR="${0:A:h}"                       # packaging
REPO_ROOT="${SCRIPT_DIR:h}"
MAC_DEPLOY="$SCRIPT_DIR/macos/deploy.command"
[[ -f "$SCRIPT_DIR/macos/config.sh" ]] && source "$SCRIPT_DIR/macos/config.sh"

: ${WIN_HOST:=winvm}
: ${WIN_REPO:=Winnow64}
: ${LINUX_HOST:=roryhill@192.168.64.3}
: ${LINUX_REPO:=Winnow64}
: ${GITHUB_REPO:=towhee/Winnow64}
: ${RELEASE_PLATFORMS:="mac intel windows linux"}
SSH_OPTS=(-o LogLevel=ERROR -o ConnectTimeout=10 -o BatchMode=yes)

function die() { echo "❌ $*" >&2; exit 1; }

# The label in CMakeLists.txt (WINNOW_VERSION_LABEL, else the project VERSION).
function current_label() {
    local cm="$REPO_ROOT/CMakeLists.txt" label
    label=$(sed -nE 's/^set\(WINNOW_VERSION_LABEL "([^"]*)"\).*/\1/p' "$cm" | head -1)
    [[ -z "$label" ]] && label=$(sed -nE 's/^project\(Winnow VERSION ([0-9.]+).*/\1/p' "$cm" | head -1)
    echo "$label"
}

function release_dir() { echo "$REPO_ROOT/out/release/$1"; }

# File each platform contributes, by label.
function asset_name() {
    case "$1" in
        mac)     echo "Winnow$2.dmg" ;;
        intel)   echo "Winnow$2_Intel.dmg" ;;
        windows) echo "WinnowSetup-$2.exe" ;;
        linux)   echo "" ;;                    # no Linux package yet
    esac
}

function has_platform() { [[ " $RELEASE_PLATFORMS " == *" $1 "* ]]; }

# --- prepare <label> -------------------------------------------------
function prepare() {
    local label="$1"
    [[ -n "$label" ]] || die "prepare needs a label, e.g. prepare 3_beta4"
    local changes="packaging/changes/$label.md"
    [[ -s "$REPO_ROOT/$changes" ]] || die "$changes is missing or empty — write the release file first."
    cd "$REPO_ROOT" || die "no repo"
    # Only the release file and CMakeLists.txt may be dirty; anything else is
    # the user's unrelated work, which this commit must not sweep up.
    local other
    other=$(git status --porcelain | grep -vE "^(\?\?| M|M |A ) (CMakeLists.txt|$changes)$")
    [[ -z "$other" ]] || { echo "$other"; die "uncommitted changes besides $changes — commit or stash them first."; }
    gh release view "v$label" --repo "$GITHUB_REPO" >/dev/null 2>&1 \
        && die "release v$label already exists on GitHub — pick a new label."
    sed -i '' -E "s/^set\(WINNOW_VERSION_LABEL \"[^\"]*\"\)/set(WINNOW_VERSION_LABEL \"$label\")/" CMakeLists.txt
    [[ "$(current_label)" == "$label" ]] || die "could not set WINNOW_VERSION_LABEL in CMakeLists.txt"
    git add CMakeLists.txt "$changes" || die "git add failed"
    if git diff --cached --quiet; then
        echo "ℹ️  Label already $label and release file committed; nothing to commit."
    else
        git commit -q -m "Release $label" || die "git commit failed"
        echo "✅ Committed: $(git log -1 --oneline)"
    fi
    git push -q || die "git push failed"
    echo "✅ Pushed $(git rev-parse --short HEAD)"
}

# --- pre-flight for build --------------------------------------------
function preflight() {
    cd "$REPO_ROOT" || die "no repo"
    LABEL=$(current_label)
    [[ -n "$LABEL" ]] || die "no version label in CMakeLists.txt"
    [[ -s "packaging/changes/$LABEL.md" ]] || die "packaging/changes/$LABEL.md is missing — write the release file."
    [[ -z "$(git status --porcelain --untracked-files=no)" ]] || {
        git status --short --untracked-files=no
        die "uncommitted changes — every platform builds the committed tree, so commit first."; }
    COMMIT=$(git rev-parse HEAD)
    [[ -n "$(git branch -r --contains "$COMMIT" 2>/dev/null)" ]] || die "HEAD ${COMMIT:0:8} is not pushed."
    local flag
    for flag in isLogger isFlowLogger; do
        grep -qE "^bool $flag *= *false;" Main/global.cpp || die "Main/global.cpp: $flag must be false for a release."
    done
    command -v gh >/dev/null && gh auth status >/dev/null 2>&1 || die "gh not logged in (gh auth login)."
    if has_platform mac || has_platform intel; then
        xcrun notarytool history --keychain-profile "${NOTARY_KEYCHAIN_PROFILE:-AC_APIKEY}" >/dev/null 2>&1 \
            || die "notary profile ${NOTARY_KEYCHAIN_PROFILE:-AC_APIKEY} does not answer — unlock the keychain."
    fi
    has_platform windows && ensure_vm Windows "$WIN_HOST"
    has_platform linux   && ensure_vm Linux "$LINUX_HOST"
    echo "✅ Pre-flight: $LABEL at ${COMMIT:0:8}"
}

# Start UTM VM $1 if needed and wait for SSH on $2 (up to 4 minutes).
function ensure_vm() {
    local name="$1" host="$2" utmctl=/Applications/UTM.app/Contents/MacOS/utmctl i
    ssh "${SSH_OPTS[@]}" "$host" exit 2>/dev/null && return 0
    [[ -x "$utmctl" ]] || die "$name VM unreachable at $host and UTM not found."
    echo "▶️  Starting VM $name ..."
    "$utmctl" start "$name" || die "utmctl start $name failed"
    for i in {1..40}; do
        ssh "${SSH_OPTS[@]}" "$host" exit 2>/dev/null && { STARTED_VMS+=("$name"); return 0; }
        sleep 6
    done
    die "$name VM started but SSH to $host never answered."
}

# --- one job per platform (each runs in the background, logs to its file) --
# Exit status: 0 = built and every test passed; 2 = built (its file is in RDIR)
# but a test failed; anything else = build or packaging failed (no file).
# Tests: unit tests in parallel, then everything else (the smoke tests, and
# metadata_read, which has no label) one at a time: those launch the whole app
# and interfere when run together (model_mutation failed under -j4 on Windows).
CTEST_UNIT_ARGS="-L unit -j4 --output-on-failure --timeout 600"
CTEST_SMOKE_ARGS="-LE unit --output-on-failure --timeout 900"

function job_mac() {
    local arch="$1"                                   # arm64 | x86_64
    WINNOW_ARCH=$arch "$MAC_DEPLOY" --steps 0,1,2,3,4,V || return 1
    local sub="" suffix="" preset=mac-release
    [[ $arch == x86_64 ]] && { sub="x86_64/"; suffix="_Intel"; preset=mac-release-x86_64; }
    local app="$REPO_ROOT/out/${sub}Notarized/Winnow$LABEL$suffix.app"
    local dmg="$REPO_ROOT/out/${sub}DMG/Winnow$LABEL$suffix.dmg"
    [[ -d "$app" && -f "$dmg" ]] || { echo "❌ expected $app and $dmg"; return 1; }
    cp -f "$dmg" "$RDIR/" || return 1
    local failed=0
    if [[ $arch == arm64 ]]; then
        echo "=== ctest ($preset) ==="
        # ctest sits beside the real cmake; /usr/local/bin may hold only a cmake symlink
        local cm; cm=$(command -v cmake) || { echo "❌ cmake not on PATH"; return 2; }
        local ct="${cm:A:h}/ctest"
        [[ -x "$ct" ]] || ct=$(command -v ctest) || { echo "❌ ctest not found beside $cm"; return 2; }
        "$ct" --test-dir "$REPO_ROOT/build/$preset" ${=CTEST_UNIT_ARGS} || failed=1
        "$ct" --test-dir "$REPO_ROOT/build/$preset" ${=CTEST_SMOKE_ARGS} || failed=1
        echo "=== selftest of the notarized app ==="
        "$app/Contents/MacOS/Winnow" --selftest "$REPO_ROOT/tests/fixtures/images" || failed=1
    else
        echo "=== selftest of the notarized app under Rosetta ==="
        arch -x86_64 "$app/Contents/MacOS/Winnow" --selftest "$REPO_ROOT/tests/fixtures/images" || failed=1
    fi
    (( failed )) && return 2 || return 0
}

# Run PowerShell on the Windows VM in the repo folder; returns its exit code.
function winps() {
    ssh "${SSH_OPTS[@]}" "$WIN_HOST" "cd $WIN_REPO; $1; exit \$LASTEXITCODE"
}

function job_windows() {
    echo "=== sync ${COMMIT:0:8} -> $WIN_HOST:$WIN_REPO ==="
    ssh "${SSH_OPTS[@]}" "$WIN_HOST" "New-Item -ItemType Directory -Force $WIN_REPO\\out | Out-Null" || return 1
    git -C "$REPO_ROOT" archive --format=tar "$COMMIT" \
        | ssh "${SSH_OPTS[@]}" "$WIN_HOST" "tar -xf - -C $WIN_REPO" || return 1
    # tar -x never deletes: drop AppleDouble files and list anything else not in the commit
    git -C "$REPO_ROOT" ls-tree -r --name-only "$COMMIT" > "$RDIR/logs/manifest.txt"
    scp "${SSH_OPTS[@]}" -q "$RDIR/logs/manifest.txt" "$WIN_HOST:$WIN_REPO/out/manifest.txt" || return 1
    winps "powershell -NoProfile -ExecutionPolicy Bypass -File packaging\\windows\\prune.ps1 -Manifest out\\manifest.txt" || return 1
    echo "=== deploy.ps1 -Steps 0,1,2,3,V ==="
    winps "powershell -NoProfile -ExecutionPolicy Bypass -File packaging\\windows\\deploy.ps1 -Steps 0,1,2,3,V" || return 1
    local setup; setup=$(asset_name windows "$LABEL")
    scp "${SSH_OPTS[@]}" "$WIN_HOST:$WIN_REPO/out/$setup" "$RDIR/" || return 1
    # A copy on the VM's Desktop, for trying it by hand
    winps "Copy-Item -Force out\\$setup \$HOME\\Desktop\\" || true
    local failed=0
    echo "=== installer smoke test: silent install + selftest + uninstall ==="
    winps "powershell -NoProfile -ExecutionPolicy Bypass -File packaging\\windows\\smoketest.ps1 -Version $LABEL" || failed=1
    echo "=== ctest (win-release) ==="
    # ctest ships with the VS Build Tools' CMake, which is not on the SSH PATH.
    local ct='& "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe"'
    winps "$ct --test-dir build\\win-release $CTEST_UNIT_ARGS" || failed=1
    winps "$ct --test-dir build\\win-release $CTEST_SMOKE_ARGS" || failed=1
    (( failed )) && return 2 || return 0
}

function job_linux() {
    echo "=== sync ${COMMIT:0:8} -> $LINUX_HOST:$LINUX_REPO ==="
    ssh "${SSH_OPTS[@]}" "$LINUX_HOST" "mkdir -p $LINUX_REPO" || return 1
    git -C "$REPO_ROOT" archive --format=tar "$COMMIT" \
        | ssh "${SSH_OPTS[@]}" "$LINUX_HOST" "tar -xf - -C $LINUX_REPO" || return 1
    # Same tidy as packaging/windows/prune.ps1: tar -x never deletes, so AppleDouble files
    # go and anything else not in the commit (outside build/ and out/) is listed.
    git -C "$REPO_ROOT" ls-tree -r --name-only "$COMMIT" \
        | ssh "${SSH_OPTS[@]}" "$LINUX_HOST" "cd $LINUX_REPO && export LC_ALL=C && \
            m=\$(mktemp) && sort > \$m && skip='( -path ./build -o -path ./out ) -prune -o' && \
            n=\$(find . \$skip -type f -name '._*' -print -exec rm -f {} + | wc -l); \
            [ \$n -gt 0 ] && echo \"Removed \$n AppleDouble (._*) files\"; \
            st=\$(find . \$skip -type f -print | sed 's|^\\./||' | sort | comm -13 \$m -); \
            [ -n \"\$st\" ] && { echo \"WARNING: \$(echo \"\$st\" | wc -l) file(s) in the tree are not in the commit:\"; \
                echo \"\$st\" | head -20 | sed 's/^/   /'; }; rm -f \$m; true" || return 1
    echo "=== linux-release build ==="
    ssh "${SSH_OPTS[@]}" "$LINUX_HOST" "cd $LINUX_REPO && cmake --preset linux-release >/dev/null \
        && cmake --build --preset linux-release" || return 1
    echo "ℹ️  Linux packaging is not built yet: build and tests only, no file to publish."
    local failed=0
    echo "=== ctest (linux-release) ==="
    ssh "${SSH_OPTS[@]}" "$LINUX_HOST" "cd $LINUX_REPO && ctest --test-dir build/linux-release $CTEST_UNIT_ARGS" || failed=1
    ssh "${SSH_OPTS[@]}" "$LINUX_HOST" "cd $LINUX_REPO && ctest --test-dir build/linux-release $CTEST_SMOKE_ARGS" || failed=1
    (( failed )) && return 2 || return 0
}

# --- build -------------------------------------------------------------
function build_all() {
    typeset -ga STARTED_VMS=()
    preflight
    RDIR=$(release_dir "$LABEL")
    mkdir -p "$RDIR/logs" || die "cannot create $RDIR"
    rm -f "$RDIR"/*.(dmg|exe)(N) "$RDIR"/SHA256SUMS(N) "$RDIR"/RESULTS(N)
    echo "$COMMIT" > "$RDIR/COMMIT"

    typeset -A pids
    typeset -gA RESULT
    local p pid
    for p in ${=RELEASE_PLATFORMS}; do
        case $p in
            mac)     ( job_mac arm64 )  > "$RDIR/logs/mac.log" 2>&1 & ;;
            intel)   ( job_mac x86_64 ) > "$RDIR/logs/intel.log" 2>&1 & ;;
            windows) ( job_windows )    > "$RDIR/logs/windows.log" 2>&1 & ;;
            linux)   ( job_linux )      > "$RDIR/logs/linux.log" 2>&1 & ;;
            *) die "unknown platform $p" ;;
        esac
        pid=$!; pids[$p]=$pid             # zsh: pids[$p]=$! would store the text "$!"
        echo "▶️  $p started (log: out/release/$LABEL/logs/$p.log)"
    done

    local rc
    for p in ${(k)pids}; do
        wait ${pids[$p]}; rc=$?
        case $rc in
            0) RESULT[$p]=PASS ;;
            2) RESULT[$p]=TESTS-FAILED ;;
            *) RESULT[$p]=BUILD-FAILED ;;
        esac
        echo "   $p: ${RESULT[$p]}"
        echo "$p ${RESULT[$p]}" >> "$RDIR/RESULTS"
    done

    local built=("$RDIR"/*.(dmg|exe)(N:t))
    (cd "$RDIR" && : > SHA256SUMS && (( ${#built} )) && shasum -a 256 $built > SHA256SUMS)
    report "$RDIR"
    local f; for f in ${(v)RESULT}; do [[ $f == PASS ]] || return 1; done
    return 0
}

# Summary table: platform, result, file, size, sha256 (first 16).
function report() {
    local dir="$1"
    echo ""
    echo "================ Release $LABEL (${COMMIT:0:8}) ================"
    printf "%-8s %-13s %-30s %6s  %s\n" PLATFORM RESULT FILE SIZE SHA256
    local p name size sum
    for p in ${=RELEASE_PLATFORMS}; do
        name=$(asset_name $p "$LABEL")
        size="-"; sum="-"
        if [[ -n "$name" && -f "$dir/$name" ]]; then
            size=$(du -h "$dir/$name" | cut -f1)
            sum=$(grep " $name\$" "$dir/SHA256SUMS" | cut -c1-16)
        fi
        printf "%-8s %-13s %-30s %6s  %s\n" $p "${RESULT[$p]:-?}" "${name:-(none yet)}" "$size" "$sum"
    done
    echo "Files: out/release/$LABEL/   Logs: out/release/$LABEL/logs/"
}

# --- publish -----------------------------------------------------------
function publish() {
    local go="$1" accept="$2"
    cd "$REPO_ROOT" || die "no repo"
    LABEL=$(current_label)
    RDIR=$(release_dir "$LABEL")
    local tag="v$LABEL"
    [[ -f "$RDIR/COMMIT" && -f "$RDIR/SHA256SUMS" ]] || die "nothing built for $LABEL — run build first."
    COMMIT=$(<"$RDIR/COMMIT")
    [[ "$(git rev-parse HEAD)" == "$COMMIT" ]] || die "HEAD moved since the build (${COMMIT:0:8}) — build again."
    (cd "$RDIR" && shasum -a 256 -c SHA256SUMS >/dev/null) || die "files changed since the build (SHA256SUMS mismatch)."
    command -v gh >/dev/null && gh auth status >/dev/null 2>&1 || die "gh not logged in (gh auth login)."

    # Upload order matters: arm64 first, because updaters older than the _Intel
    # match take the first .dmg on the release.
    local files=() p name
    for p in mac intel windows linux; do
        has_platform $p || continue
        name=$(asset_name $p "$LABEL")
        [[ -n "$name" ]] || continue
        [[ -f "$RDIR/$name" ]] || die "$name is missing from $RDIR — that platform did not build."
        files+=("$RDIR/$name")
    done

    local exists=false draft=true
    if gh release view "$tag" --repo "$GITHUB_REPO" >/dev/null 2>&1; then
        exists=true
        draft=$(gh release view "$tag" --repo "$GITHUB_REPO" --json isDraft --jq .isDraft)
        [[ "$draft" == "true" ]] || die "$tag is already public. Do not replace published files; bump the label."
    fi

    echo ""
    echo "================ PUBLISH  $GITHUB_REPO  $tag ================"
    echo "  commit : ${COMMIT:0:8}  $(git log -1 --format=%s "$COMMIT")"
    echo "  release: $($exists && echo "existing DRAFT (files replaced)" || echo "NEW draft, then public")"
    for name in $files; do echo "  upload : ${name:t}"; done
    echo "  then   : make public + latest; check every link and digest"
    local failures=""
    [[ -f "$RDIR/RESULTS" ]] && failures=$(grep -v " PASS$" "$RDIR/RESULTS")
    if [[ -n "$failures" ]]; then
        echo "  ⚠️  not every platform passed its tests (logs in $RDIR/logs/):"
        print -r -- "$failures" | sed 's/^/        /'
    fi
    echo "------------------------------ notes ------------------------------"
    "$MAC_DEPLOY" --release-notes "$LABEL" || die "could not render the notes"
    echo "==================================================================="
    [[ "$go" == "--yes" ]] || { echo "Plan only. Run 'publish --yes' to publish."; return 0; }
    [[ -z "$failures" || "$accept" == "--accept-failures" ]] \
        || die "test failures above; fix and build again, or publish --yes --accept-failures."

    if ! $exists; then
        local notes; notes=$("$MAC_DEPLOY" --release-notes "$LABEL") || die "notes failed"
        gh release create "$tag" --repo "$GITHUB_REPO" --target "$COMMIT" \
            --title "Winnow ${LABEL//_/ }" --notes "$notes" --draft || die "gh release create failed"
    fi
    for name in $files; do
        echo "⬆️  ${name:t}"
        gh release upload "$tag" "$name" --repo "$GITHUB_REPO" --clobber || die "upload of ${name:t} failed (release still a draft)"
    done
    gh release edit "$tag" --repo "$GITHUB_REPO" --draft=false --latest || die "could not make $tag public"
    check_published "$tag" "${files[@]}"
}

# /releases/latest is this tag, each download answers, each digest matches.
function check_published() {
    local tag="$1"; shift
    local ok=true latest name url want got
    latest=$(gh release view --repo "$GITHUB_REPO" --json tagName --jq .tagName)
    [[ "$latest" == "$tag" ]] && echo "   OK  latest = $tag" || { echo "   X  latest is $latest, not $tag"; ok=false; }
    for name in "$@"; do
        url="https://github.com/$GITHUB_REPO/releases/download/$tag/${name:t}"
        if curl -sfIL -o /dev/null "$url"; then echo "   OK  200 ${name:t}"; else echo "   X  no download: $url"; ok=false; fi
        want=$(shasum -a 256 "$name" | cut -d' ' -f1)
        got=$(gh release view "$tag" --repo "$GITHUB_REPO" --json assets \
              --jq ".assets[] | select(.name==\"${name:t}\") | .digest" 2>/dev/null)
        if [[ -z "$got" ]]; then echo "   ?  no digest reported for ${name:t}"
        elif [[ "$got" == "sha256:$want" ]]; then echo "   OK  digest ${name:t}"
        else echo "   X  digest mismatch ${name:t}: $got"; ok=false; fi
    done
    $ok && echo "✅ Published: https://github.com/$GITHUB_REPO/releases/tag/$tag" || die "post-publish checks failed"
}

function status_cmd() {
    LABEL=$(current_label); RDIR=$(release_dir "$LABEL")
    echo "Label: $LABEL   commit built: $([[ -f $RDIR/COMMIT ]] && cut -c1-8 "$RDIR/COMMIT" || echo none)   HEAD: $(git -C "$REPO_ROOT" rev-parse --short=8 HEAD)"
    [[ -f "$RDIR/SHA256SUMS" ]] && cat "$RDIR/SHA256SUMS"
    gh release view "v$LABEL" --repo "$GITHUB_REPO" --json isDraft,assets \
        --jq '"GitHub: draft=\(.isDraft)  assets: \([.assets[].name] | join(", "))"' 2>/dev/null \
        || echo "GitHub: no release v$LABEL"
}

case "$1" in
    prepare) prepare "$2" ;;
    build)   build_all; rc=$?
             for vm in $STARTED_VMS; do echo "ℹ️  VM $vm was started for this build; it is left running for testing."; done
             exit $rc ;;
    publish) publish "$2" "$3" ;;
    status)  status_cmd ;;
    *) sed -n '3,11p' "$0"; exit 2 ;;
esac
