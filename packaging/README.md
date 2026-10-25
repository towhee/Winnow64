# Winnow packaging / deployment

This directory is the **single, in-repo deployment pipeline** for Winnow. Because
it lives in the repo, every machine gets it from `git clone` / `git pull`. Each
machine supplies only its own paths and secrets through a local, git-ignored
config file — the committed `*.example` files are the templates.

```
packaging/
  macos/
    deploy.command       build -> stage -> sign -> notarize -> DMG -> archive -> upload
    verify_bundle.command  vanilla-Mac readiness checker
    entitlements.plist   static hardened-runtime entitlements (committed)
    config.sh.example    macOS config template  ->  copy to config.sh  (git-ignored)
  windows/
    deploy.ps1           menu: build -> sign -> Inno Setup installer -> archive -> GitHub Release -> promote
    deploy.cmd           double-click launcher for deploy.ps1 (ExecutionPolicy Bypass)
    winnow.iss           Inno Setup installer definition
    config.ps1.example   Windows config template -> copy to config.ps1 (git-ignored)
```

Build output and deploy artifacts go to the git-ignored `build/` and `out/`
directories at the repo root. Nothing is written outside the repo except the
optional version archive and the optional server upload.

## Version

The version is single-sourced from `CMakeLists.txt`:

```cmake
project(Winnow VERSION 2.05 LANGUAGES C CXX)
```

This feeds **(a)** the generated `Main/winnow_version.h` → the in-app version
string (`mainwindow.h` uses `WINNOW_VERSION`), and **(b)** the macOS bundle
`CFBundleShortVersionString`. The deploy scripts read the version *back* from the
built artifact, so DMG / installer filenames always match the binary. **To bump
the version, edit only the `project(... VERSION ...)` line.**

---

## One-time setup per machine

### macOS (both Macs)

1. Xcode command line tools: `xcode-select --install`
2. Qt 6.9.2 (macos) installed (default `~/Qt/6.9.2/macos`).
3. Homebrew deps: `brew install opencv ffmpeg webp`
4. Import the **Developer ID Application** certificate into the login keychain.
5. Store notarization credentials once under the profile name `AC_APIKEY`
   (matches `NOTARY_KEYCHAIN_PROFILE` in config.sh). **Preferred: App Store
   Connect API key** — it has no 2FA/password/expiry friction and won't lock the
   Apple ID on repeated use, which makes it the robust choice for an automated
   pipeline:
   ```sh
   # App Store Connect -> Users and Access -> Integrations -> App Store Connect
   # API -> generate a key (Developer role is sufficient for notarization).
   # Download AuthKey_<KEYID>.p8 ONCE; note the Key ID and Issuer ID.
   xcrun notarytool store-credentials AC_APIKEY \
     --key /path/to/AuthKey_<KEYID>.p8 \
     --key-id <KEY_ID> --issuer <ISSUER_ID>
   ```
   **Fallback: Apple ID + app-specific password** (more fragile — repeated failed
   attempts can lock the Apple ID, and resetting the Apple ID password
   invalidates every app-specific password):
   ```sh
   xcrun notarytool store-credentials AC_APIKEY \
     --apple-id "<your-apple-id>" --team-id 2663CS489R \
     --password "<app-specific-password>"
   ```
   (Generate the app-specific password at appleid.apple.com — never commit it.)
   Verify either route with `xcrun notarytool history --keychain-profile AC_APIKEY`.
6. Create the local config:
   ```sh
   cp packaging/macos/config.sh.example packaging/macos/config.sh
   # edit QT_DIR, DEVELOPER_ID, ARCHIVE_DIR, upload settings
   ```

### macOS dev builds are signed too (not just releases)

Every local build of the `Winnow` target is signed by a CMake POST_BUILD step
with `WINNOW_DEV_CODESIGN_IDENTITY` (defaults to the same Developer ID as
`config.sh`) and `packaging/macos/dev-entitlements.plist`.

This is not about distribution — it is what stops macOS asking to confirm access
to Documents / Desktop / Pictures / iCloud on *every* run. Without it the linker
ad-hoc-signs the bundle, so the designated requirement is a cdhash of the binary;
each recompile changes it, TCC sees an app it has never seen, and re-prompts. A
real certificate gives a `identifier "com.winnow.Winnow" … subject.OU =
"2663CS489R"` requirement that is identical across rebuilds, so grants persist.

Dev signing deliberately omits the hardened runtime and adds
`com.apple.security.get-task-allow`, so lldb/Qt Creator debugging is unaffected.
Release signing is unchanged: `deploy.command` re-signs the staged bundle with
`entitlements.plist` and `--options runtime` before notarization.

Disable with `-DWINNOW_DEV_CODESIGN_IDENTITY=""` (falls back to ad-hoc, and the
repeated prompts return).

**On a second Mac** (MacBook Pro / Mac Studio) nothing extra is needed *provided
step 4 above was done there* — the identity must be importable as an identity,
i.e. the `.p12` with its private key, not a bare `.cer`. Configure checks with
`security find-identity -v -p codesigning`; if the identity is missing it warns,
skips signing and the build still succeeds.

The macOS privacy database is per-machine and is not in git, iCloud or Migration
Assistant's synced set, so each Mac still asks once on the first signed run and
then remembers. What travels in git is the *stable identity* that makes the
answer stick.

### Windows 11

1. Visual Studio 2022 (Desktop C++ workload) — includes the MSVC toolchain and,
   via "C++ CMake tools for Windows", **Ninja** (the generator the `win-release`
   preset uses). Run the build from a *Developer PowerShell for VS 2022 (x64)* so
   `cl.exe` and `ninja` are on PATH.
2. Qt 6 (MSVC). Build selects it via the `QT_DIR` env var (CMakePresets
   `CMAKE_PREFIX_PATH`) or the Qt Creator kit.
3. [Inno Setup 6](https://jrsoftware.org/isdl.php).
4. *(optional)* A code-signing certificate + the Windows SDK `signtool.exe`.
5. For the **promote** step (publishing to the server): the OpenSSH client
   (`ssh.exe` / `scp.exe`, built into Windows 10/11) and an **OpenSSH-format**
   private key that logs into the server. A PuTTY `.ppk` is **not** usable —
   convert it once with `puttygen <key>.ppk -O private-openssh -o id_winnow`.
6. Create the local config:
   ```powershell
   Copy-Item packaging\windows\config.ps1.example packaging\windows\config.ps1
   # edit $InnoSetupCompiler, signing, $Server* / $WebBaseUrl, upload settings
   ```

**One build tree, shared with Qt Creator.** The `win-release` preset builds into
`build\win-release\` (Ninja), and CMake redirects the app into a clean
`build\win-release\app\` subdir — that's exactly what `deploy.ps1` packages. To
make Qt Creator build into the *same* tree (instead of its own sibling
`build-Winnow64-…` shadow dir), let Creator use the preset: open the project and,
in the configure/kit screen, pick the **win-release** (and **win-debug**)
*preset* rather than a manual build configuration. Creator then configures with
Ninja into `build\win-release\`, so the binary you run in the IDE is the binary
the deploy menu ships — no stale-artifact drift. (A *kit* only sets the
toolchain/Qt; the build directory comes from the preset.)

---

## The Magic Phrases

There is one phrase per platform. Say it to Claude on the machine that builds that
platform, and Claude packages the release and publishes it to the **GitHub Release**
(menu `G`). The phrases do not touch the winnow.ca server.

| Say | Where | Claude runs | Publishes |
|---|---|---|---|
| **"Package and deploy Mac"** | a Mac | `deploy.command` `0`–`4` + `V`, then `G` | `Winnow<version>.dmg` |
| **"Package and deploy Intel"** | a Mac | `WINNOW_ARCH=x86_64 deploy.command` `0`–`4` + `V`, then `G` | `Winnow<version>_Intel.dmg` |
| **"Package and deploy Windows"** | the Windows PC | `deploy.ps1` `A`, then `G` | `WinnowSetup-<version>.exe` |

For each one, Claude:

1. Confirms the version (`project(Winnow VERSION ...)` and `WINNOW_VERSION_LABEL` in
   `CMakeLists.txt`) and the release pre-checks it can verify (e.g.
   `isLogger`/`isFlowLogger = false` in `Main/global.cpp`), and asks about anything
   that needs your decision.
2. Builds, signs, notarizes (macOS) and verifies. It stops if verify fails.
3. Runs `G`: shows the plan and publishes only after your explicit `yes`, every time.

Run them in order: **Mac first, then Intel, then Windows**. Older copies of Winnow take
the first `.dmg` on the release. The first phrase creates the release as a draft; it
goes public when the third file is attached.

Prerequisites: the release commit is pushed (the tag goes on HEAD), the platform's
config file exists, and on macOS the keychain is unlocked (Developer ID cert +
`AC_APIKEY` notary profile).

Publishing to winnow.ca (promote, mac `7` / Windows `5`) is a separate, manual step. It
is still needed for copies of Winnow older than 3.0.

### All platforms from Quark: "Build release" / "Publish release"

`packaging/release.sh` runs a whole release from one Mac, with Windows built on the
UTM Windows VM (`ssh winvm`) and Linux on two UTM Linux VMs (ARM64 "Linux" and the
emulated x86_64 "Linux-x64"):

| Say | Claude runs | Result |
|---|---|---|
| **"Build release"** | `release.sh prepare <label>` (if the label is new), then `release.sh build` | every platform built, packaged and tested from one commit into `out/release/<label>/`; nothing published |
| **"Publish release"** | `release.sh publish` (shows the plan), then `release.sh publish --yes` after your `yes` | one GitHub release with every file, public, links and digests checked |

You write `packaging/changes/<label>.md` first; its name is the new label. The
platform scripts have unattended modes for this: `deploy.command --steps 0,1,2,3,4,V`
and `deploy.ps1 -Steps 0,1,2,3,V` (no prompts, non-zero exit on failure), and
`packaging/windows/smoketest.ps1` tests the installer. Linux is packaged as an AppImage
per architecture by `packaging/linux/appimage.sh`, which also self-tests the finished
AppImage (offscreen and on a headless Wayland compositor). Full checklist:
WinnowShared/Deployment.txt section 1.

### Linux AppImage

```sh
packaging/linux/appimage.sh            # on a Linux build VM, after building linux-release
```
Produces `out/Winnow-<label>-<arch>.AppImage` (arch = `x86_64` or `aarch64`). The tools
(linuxdeploy, its Qt plugin, appimagetool and the AppImage runtime) are pinned releases
kept in `~/UTMShare/appimage-tools` on Quark and checked against
`packaging/linux/tools.sha256`; `release.sh` copies them to each VM. Details:
`notes/Documentation.md` §2.13.4.

---

## Running a deployment (manually)

### macOS
```sh
chmod +x packaging/macos/deploy.command   # first time only
packaging/macos/deploy.command            # interactive menu; "A" runs the full pipeline
```
Produces `out/DMG/Winnow<version>.dmg` (notarized, stapled, with an
`/Applications` drag-install symlink).

### macOS Intel (x86_64)

The Intel build is cross-compiled on an Apple Silicon Mac. Homebrew there is
arm64-only, so OpenCV, libtiff and libjpeg-turbo are built from source as static
x86_64 libraries (deployment target 12.0). Qt is the official universal install, and
its x86_64 slice is used.

```sh
packaging/macos/build_x86_64_deps.sh               # once: ~/Projects/winnow-deps/macos-x86_64
WINNOW_ARCH=x86_64 packaging/macos/deploy.command  # same menu; preset mac-release-x86_64
```

`WINNOW_ARCH=x86_64` selects the `mac-release-x86_64` preset (`build/mac-release-x86_64`),
puts everything under `out/x86_64/`, thins to x86_64, skips the Homebrew dylib/WebP
steps and names the result `Winnow<version>_Intel.dmg`. Verify checks for the x86_64
slice. Promote (menu 7) refuses to run, because it rewrites the Apple Silicon link on
winnow.ca. Publish the Intel DMG with **G** instead. Test it on Apple Silicon under
Rosetta: `arch -x86_64 <app>/Contents/MacOS/Winnow --selftest tests/fixtures/images`.

### Windows
```powershell
# from a Developer PowerShell for VS 2022 (x64)
powershell -ExecutionPolicy Bypass -File packaging\windows\deploy.ps1
```
Or double-click `packaging\windows\deploy.cmd` in Explorer (it launches the same
menu with `-ExecutionPolicy Bypass`). For the build/sign steps, prefer launching
from a Developer PowerShell / VS x64 prompt so `cmake`, MSVC, and `signtool`
resolve.

Interactive numbered menu mirroring the macOS one:

| Key | Step |
|-----|------|
| `0` | Build (`cmake --build --preset win-release`) |
| `1` | Sign binaries (deployed build output) |
| `2` | Build installer (Inno Setup) |
| `3` | Archive installer (local versioned copy) |
| `4` | Upload installer to test dir |
| `5` | **Promote** + publish to server (archive old → upload new → update `index.html` → publish `version.json`) |
| `G` | Publish installer to the **GitHub Release** for this version |
| `V` | Verify Windows readiness (exe + Qt runtime present; Authenticode if signing on) |
| `D` | Promote **dry-run** (print every server command, write nothing) |
| `A` | Full pipeline (`0–3` + verify; does **not** publish) |
| `P` | Print config |

`0–3 + A` produce `out\WinnowSetup-<version>.exe`. **Promote** publishes to
`winnow_win/current/WinnowSetup-<version>.exe`, repoints the landing-page Windows
download link, and keeps `version.json` in sync. Like the mac promote it shows a
plan and requires an explicit `yes` before any production write; run `D` first to
preview.

---

## Distribution

### GitHub Releases (primary download location)

The README's download link is `https://github.com/towhee/Winnow64/releases/latest`.
Menu **`G`** in both scripts (mac `deploy.command`, Windows `deploy.ps1`) publishes
the platform's artifact to the GitHub Release tagged `v<version>` (e.g. `v3_beta`):

- It runs the verify gate, shows a plan, and requires an explicit `yes`.
- One release per version is shared by both platforms: whichever machine runs `G`
  first creates the release (tag on the current HEAD — **push first**), and the
  other attaches its file.
- The release is created as a **draft** and goes public automatically when the `G`
  run that attaches the last of the three assets finishes (`release_assets` /
  `Release-Assets`). Until then users, the README link and the updater see nothing,
  so the notes' direct links can never 404. To go public with an asset missing, type
  `publish` at the prompt the script shows.
- Re-running `G` replaces that platform's file. On a public release this is a swap,
  not `--clobber`: the new file uploads as `<name>.part` (the updater ignores it),
  then the old asset is deleted and the new one renamed, so the link is down for
  about a second instead of the whole upload.
- Releases are created as full releases, not pre-releases, because
  `/releases/latest` ignores pre-releases.
- The release notes (the "What to Download" table with direct links to both files)
  come from `packaging/release-notes.md`. `{{TITLE}}`, `{{REPO}}`, `{{TAG}}` and
  `{{VERSION}}` are filled in for the new tag. `{{CHANGES}}` becomes a "Release
  Notes" section holding `packaging/changes/<version>.md` (e.g. `changes/3_beta3.md`;
  Markdown, no heading), or nothing when that file is absent or empty. Menu **`N`**
  (`update_github_notes` / `Update-GitHubNotes`) re-renders the notes from both files
  onto the existing release, draft or public, after a `yes`; it leaves the assets alone
  and replaces the whole body, so edits made on the GitHub page are lost; asset names must match
  `Winnow<version>.dmg`, `Winnow<version>_Intel.dmg` and `WinnowSetup-<version>.exe`.
- Each Mac build's updater takes only its own architecture's DMG (`_Intel` or not;
  see `AppUpdate::isPlatformAsset`). Builds from before that change take the FIRST
  `.dmg` on the release, so attach the Apple Silicon DMG before the Intel one. Edit the template before a
  release when the text changes (e.g. drop the beta warning for a final release).
- Requires the GitHub CLI logged in once per machine (`brew install gh` /
  `winget install GitHub.cli`, then `gh auth login`). The repo is `GITHUB_REPO` /
  `$GitHubRepo` in the config (default `towhee/Winnow64`).

### Server (winnow.ca)

Distribution is plain downloads: the DMG and the `Setup.exe` are uploaded to the
DigitalOcean server (`winnow_mac` / `winnow_win`). The **promote** step (mac step
`7`, Windows menu `5`) publishes a release to `current/`, archives the previous
one, repoints the landing-page link, and updates `version.json` — preview it with
the dry-run first (mac `D`, Windows `D`). The separate **upload** step
(`UPLOAD_ENABLED=true` / `$UploadEnabled = $true`) just pushes the artifact to a
`test/` dir for pre-release checking. The ssh key path is per-machine and never
committed.

### In-app "Check for updates" (auto-update)

Winnow 3.0+ checks **GitHub** for updates (Help → *Check for updates*, and silently
at startup when *Check for program updates at startup* is on). It reads
`https://api.github.com/repos/towhee/Winnow64/releases/latest`, so **publishing a full
release with menu `G` IS the update announcement** — there is no other descriptor to
keep in sync. When the user accepts, Winnow downloads the platform asset, checks it
against GitHub's SHA-256 digest, installs it over the running copy and relaunches
(macOS: signature-checked bundle swap; Windows: the Inno installer run silently). See
*Check for Updates* in `notes/Documentation.md`.

What this requires of a release:

- The tag is `v<WINNOW_VERSION>` (menu `G` does this). Versions are ordered
  numerically with pre-release suffixes below the release: `3_beta` < `3.0` < `3.1`.
  **Never re-publish a lower version as "latest"** — nobody would be offered it, and
  anyone already on the higher one stays there.
- The assets keep their names: `Winnow<ver>.dmg` (with the signed, notarized
  `Winnow<ver>.app` inside) and `WinnowSetup-<ver>.exe`.
- The release stays a draft until all three assets are attached (see above). If you
  force it public with an asset missing, users on that platform are told nothing
  (silent check), or that the release is "not yet available for this computer"
  (manual check), and that asset's link in the notes 404s.
- Do not mark a release you want installed as a pre-release, or leave it a draft
  (one is left a draft only while assets are missing); `/releases/latest` skips both. That is also the way to stage a release for
  testing without offering it.
- **Testing before a real release:** publish the build as a *pre-release* under a
  throwaway tag (nobody's update check or the README link sees pre-releases), then
  point a copy of Winnow at it:
  ```sh
  gh release create v3_beta-test --repo towhee/Winnow64 --prerelease \
      --target "$(git rev-parse HEAD)" --title "Update test" --notes "Test only" \
      out/DMG/Winnow3_beta.dmg
  WINNOW_UPDATE_TAG=v3_beta-test WINNOW_UPDATE_AS_VERSION=2.0 \
      /Applications/Winnow3_beta.app/Contents/MacOS/Winnow
  ```
  `WINNOW_UPDATE_TAG` checks that one tag (pre-releases allowed);
  `WINNOW_UPDATE_AS_VERSION` makes the running copy pretend to be older. Delete the
  test release and its tag afterwards:
  `gh release delete v3_beta-test --repo towhee/Winnow64 --cleanup-tag`.
- macOS in-place install needs Winnow in an `Applications` folder the user can
  write to; otherwise the DMG is downloaded and opened for a manual install.

**Legacy server check.** Copies of Winnow older than 3.0 read a per-platform
`version.json` on winnow.ca instead (written by the promote steps: mac step 7b,
Windows menu `5`):

```json
{ "version": "2.05",
  "url":     "https://winnow.ca/winnow_mac/current/Winnow2.05.dmg",
  "notes":   "https://winnow.ca/winnow/versions.html" }
```
Keep promoting to the server for as long as those copies need to find their way to
3.0; they only open the downloaded installer, they do not auto-install.

## ⚠️ Security note

`notes/DeployInstall.txt` historically contained plaintext credentials (Apple ID
password, notarization app-specific password, login password). Those must not
live in the repo: **regenerate the app-specific password** at appleid.apple.com,
keep notary credentials only in the keychain (step 5 above), and scrub the old
values from that file.
