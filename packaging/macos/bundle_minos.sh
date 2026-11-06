#!/bin/zsh
# --------------------------------------------------------------------
# The macOS a bundle REALLY needs: the highest minos of every Mach-O in it.
#
#   bundle_minos.sh <app> [arch]            # prints e.g. 13.4
#   bundle_minos.sh <app> [arch] --above F  # lists each binary whose minos > F
#
# dyld refuses a library built for a newer macOS than the one running ("built for
# macOS 27.0 which is newer than running OS"), so one Homebrew dylib decides the
# floor, whatever the executable says. v3_beta5 claimed macOS 12 on Apple Silicon
# while bundling dylibs built for 27.0. arch defaults to arm64.
# --------------------------------------------------------------------
APP="${1:A}"; ARCH="${2:-arm64}"; ABOVE=""
[[ "$3" == "--above" ]] && ABOVE="$4"
[[ -d "$APP" ]] || { echo "usage: bundle_minos.sh <app> [arch] [--above X.Y]" >&2; exit 2; }

osnum() { local v="$1" maj min; maj="${v%%.*}"; min="${v#*.}"; [[ "$min" == "$v" ]] && min=0
          min="${min%%.*}"; echo $(( maj * 100 + min )); }

max="0.0"
while IFS= read -r f; do
    file -b "$f" | grep -q Mach-O || continue
    v=$(otool -arch "$ARCH" -l "$f" 2>/dev/null \
        | awk '/LC_BUILD_VERSION/{g=1} g&&/minos/{print $2; exit}')
    [[ -z "$v" ]] && v=$(otool -arch "$ARCH" -l "$f" 2>/dev/null \
        | awk '/LC_VERSION_MIN_MACOSX/{g=1} g&&/version/{print $2; exit}')
    [[ -n "$v" ]] || continue
    (( $(osnum "$v") > $(osnum "$max") )) && max="$v"
    [[ -n "$ABOVE" ]] && (( $(osnum "$v") > $(osnum "$ABOVE") )) && echo "$v  ${f#$APP/}"
done < <(find "$APP" -type f \( -perm -u+x -o -name '*.dylib' -o -name '*.so' \))
[[ -z "$ABOVE" ]] && echo "$max"
exit 0
