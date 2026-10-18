# Packaging and Deploying Winnow

A how-to for building a release and getting it to users. For the reasoning behind the
pipeline, the config options and the gotchas, see `packaging/README.md`.

## Contents

- [1 Overview](#1-overview)
- [2 One-Time Setup](#2-one-time-setup)
  - [2.1 macOS](#21-macos)
  - [2.2 Windows](#22-windows)
- [3 Before Every Release](#3-before-every-release)
- [4 Package on macOS (Apple Silicon)](#4-package-on-macos-apple-silicon)
- [5 Package on macOS (Intel)](#5-package-on-macos-intel)
- [6 Package on Windows](#6-package-on-windows)
- [7 Publish](#7-publish)
  - [7.1 GitHub Release (Main Download)](#71-github-release-main-download)
  - [7.2 winnow.ca Server (Legacy)](#72-winnowca-server-legacy)
  - [7.3 Beta or Test Upload](#73-beta-or-test-upload)
- [8 Check the Release](#8-check-the-release)
- [9 Menu Reference](#9-menu-reference)
  - [9.1 macOS deploy.command](#91-macos-deploycommand)
  - [9.2 Windows deploy.ps1](#92-windows-deployps1)
- [10 Troubleshooting](#10-troubleshooting)

## 1 Overview

> **The Magic Phrases: say one to Claude on the machine that builds that platform.**
>
> | Say | On | Publishes to GitHub |
> |---|---|---|
> | **"Package and deploy Mac"** | a Mac | `Winnow<version>.dmg` (Apple Silicon) |
> | **"Package and deploy Intel"** | a Mac | `Winnow<version>_Intel.dmg` |
> | **"Package and deploy Windows"** | the Windows PC | `WinnowSetup-<version>.exe` |
>
> For each one, Claude checks the version and release flags, builds, signs, notarizes
> (macOS) and verifies, then publishes to the GitHub Release (menu **G**) after your
> explicit `yes`.
>
> - **Order:** Mac, then Intel, then Windows. The release stays a draft until all
>   three files are attached, then goes public.
> - **Before you say it:** commit and push, and unlock the keychain (macOS).
> - The phrases do **not** update winnow.ca; that is a separate manual step
>   ([7.2](#72-winnowca-server-legacy)).

A release has three files, all built from the same commit and attached to one GitHub
Release tagged `v<version>`:

| Platform | Built on | File |
|---|---|---|
| macOS Apple Silicon | a Mac | `Winnow<version>.dmg` |
| macOS Intel | an Apple Silicon Mac (cross-compiled) | `Winnow<version>_Intel.dmg` |
| Windows | the Windows PC | `WinnowSetup-<version>.exe` |

Everything is driven by two interactive menu scripts in the repo:

- macOS: `packaging/macos/deploy.command`
- Windows: `packaging/windows/deploy.ps1` (or double-click `deploy.cmd`)

Output goes to the git-ignored `out/` folder at the repo root. The in-app *Check for
updates* reads GitHub's latest release, so publishing the release is also the update
announcement.

## 2 One-Time Setup

Do this once per machine. Full detail is in `packaging/README.md` → *One-time setup per
machine*.

### 2.1 macOS

1. Install the Xcode command line tools, Qt (official installer, not Homebrew) and the
   Homebrew deps (`brew install opencv ffmpeg webp`).
2. Import the **Developer ID Application** certificate (the `.p12` with its private key)
   into the login keychain. Check with `security find-identity -v -p codesigning`.
3. Store the notarization credentials in the keychain under the profile `AC_APIKEY`, using
   the App Store Connect API key:
   ```sh
   xcrun notarytool store-credentials AC_APIKEY \
     --key ~/.appstoreconnect/private_keys/AuthKey_<KEYID>.p8 \
     --key-id <KEYID> --issuer <ISSUER_ID>
   ```
   Check with `xcrun notarytool history --keychain-profile AC_APIKEY`.
4. Create the local config and set `QT_DIR` to the Qt you build with, plus the server
   settings:
   ```sh
   cp packaging/macos/config.sh.example packaging/macos/config.sh
   ```
5. Install and log in to the GitHub CLI: `brew install gh`, then `gh auth login`.
6. For Intel builds only: build the static x86_64 libraries once with
   `packaging/macos/build_x86_64_deps.sh` (writes to
   `~/Projects/winnow-deps/macos-x86_64`).

### 2.2 Windows

1. Install Visual Studio 2022 (Desktop C++ workload, includes Ninja), Qt 6 for MSVC and
   Inno Setup 6.
2. *(Optional)* a code-signing certificate and `signtool.exe`.
3. For the server step: an OpenSSH-format private key (convert a PuTTY `.ppk` with
   `puttygen key.ppk -O private-openssh -o id_winnow`).
4. Create the local config:
   ```powershell
   Copy-Item packaging\windows\config.ps1.example packaging\windows\config.ps1
   ```
5. Install and log in to the GitHub CLI: `winget install GitHub.cli`, then `gh auth
   login`.

## 3 Before Every Release

1. **Set the version** in `CMakeLists.txt` — this is the only place:
   - `project(Winnow VERSION 3.0 ...)` — the numeric version.
   - `set(WINNOW_VERSION_LABEL "3_beta2")` — the user-facing name used for the tag, file
     names, About box and update check. Leave it empty for a plain release.
   
   A new release must be *higher* than the one already published, or nobody is offered it
   (`3_beta` < `3.0` < `3.1`).
2. **Check release flags** in `Main/global.cpp`: `isLogger = false` and `isFlowLogger =
   false`.
3. **Write the Release Notes list** in `packaging/changes/<label>.md`, named after
   `WINNOW_VERSION_LABEL` (e.g. `packaging/changes/3_beta3.md`). Write plain Markdown,
   usually a bullet list, without a heading; the scripts add the "Release Notes" heading.
   With no file the release has no Release Notes section. See
   [7.1](#71-github-release-main-download) to change it after publishing.
4. **Edit the release notes** template `packaging/release-notes.md` if the rest of the
   text needs to change (e.g. remove the beta warning for a final release).
5. **Commit and push** to GitHub. The release tag is created on the current HEAD, so the
   pushed commit is what gets tagged.
6. Pull the same commit on every machine that will build (the other Mac, the Windows PC).

## 4 Package on macOS (Apple Silicon)

Unlock the keychain, then run the menu:

```sh
chmod +x packaging/macos/deploy.command   # first time only
packaging/macos/deploy.command
```

Choose **A** (full pipeline). It builds, stages, signs, notarizes, makes the DMG, archives
a copy and runs the vanilla-Mac verify. It does not publish anything.

To go step by step instead: `0` build, `1` stage, `2` sign, `3` notarize, `4` DMG, then
`V` verify.

Result: `out/DMG/Winnow<version>.dmg`, signed, notarized and stapled.

Notarization usually takes a few minutes. Don't continue if **V** reports a failure.

## 5 Package on macOS (Intel)

On the Apple Silicon Mac, run the same menu with the architecture set:

```sh
WINNOW_ARCH=x86_64 packaging/macos/deploy.command
```

Choose **A** (or `0`–`4` then `V`). The result is
`out/x86_64/DMG/Winnow<version>_Intel.dmg`.

You can run this at the same time as the Apple Silicon build in another terminal; they use
separate build folders.

To smoke-test it under Rosetta:

```sh
arch -x86_64 <app>/Contents/MacOS/Winnow --selftest tests/fixtures/images
```

## 6 Package on Windows

From a **Developer PowerShell for VS 2022 (x64)**:

```powershell
powershell -ExecutionPolicy Bypass -File packaging\windows\deploy.ps1
```

Choose **A** (build, sign, installer, archive, verify). The result is
`out\WinnowSetup-<version>.exe`.

## 7 Publish

### 7.1 GitHub Release (Main Download)

On each machine, after its build verifies, choose **G** in the menu. It shows a plan; type
`yes` to go ahead.

**Order matters:** publish the Apple Silicon DMG first, then the Intel DMG, then the
Windows installer. Older copies of Winnow take the first `.dmg` on the release, so Apple
Silicon must be first.

- The first **G** creates the release `v<version>` as a **draft** and attaches its file.
- Each later **G** attaches its file to the same release.
- When the third file is attached, the release goes public automatically and becomes
  `/releases/latest`.
- To go public with a file still missing, type `publish` at the prompt. That platform's
  link in the notes will 404 until its file is attached.
- Running **G** again for a platform replaces its file.

**Editing the notes after the release exists.** Edit `packaging/changes/<label>.md` (or
`packaging/release-notes.md`), then choose **N** on any machine. It shows the full notes it
will set; type `yes`. **N** works on a draft or a public release and leaves the files
alone. It replaces the whole notes text, so make changes in these files rather than on the
GitHub page, or the next **N** overwrites them. Commit the edited file so the other
machines have it.

### 7.2 winnow.ca Server (Legacy)

Copies of Winnow older than 3.0 check `version.json` on winnow.ca rather than GitHub. Keep
updating the server until those copies have moved to 3.x.

- macOS (Apple Silicon only): choose **D** first for a dry run that prints every server
  command and writes nothing. Then choose **7** (promote) and type `yes`. It archives the
  old DMG, uploads the new one to `winnow_mac/current/`, updates the link in `index.html`
  (keeping a backup) and writes `version.json`.
- Windows: **D** for the dry run, then **5** (promote).

The Intel DMG is not promoted to the server; it is published only through **G**.

### 7.3 Beta or Test Upload

Menu **6** (macOS) / **4** (Windows) uploads the build to a test folder on the server
without touching the live download. Set `UPLOAD_ENABLED=true` and `UPLOAD_SUBDIR` in the
config (e.g. `winnow_mac/beta`).

To test the in-app updater against a build before releasing it, publish it as a GitHub
*pre-release* under a throwaway tag and point Winnow at it with `WINNOW_UPDATE_TAG`
and `WINNOW_UPDATE_AS_VERSION`. The commands are in `packaging/README.md` → *In-app
"Check for updates"*.

## 8 Check the Release

1. Open `https://github.com/towhee/Winnow64/releases/latest` — the new version is shown,
   with all three files and the notes.
2. Click each download link in the notes.
3. On a Mac with an older Winnow, run *Help → Check for updates* and confirm it offers
   and installs the new version.
4. If you promoted to winnow.ca, open the site and confirm the download link points at the
   new DMG.

## 9 Menu Reference

### 9.1 macOS deploy.command

| Key | Step |
|---|---|
| `0` | Build |
| `1` | Stage fresh app |
| `2` | Sign staged app |
| `3` | Notarize + staple |
| `4` | Create DMG |
| `5` | Archive a local copy |
| `6` | Upload DMG to the test/beta folder |
| `7` | Promote to winnow.ca (live) |
| `G` | Publish DMG to the GitHub Release |
| `N` | Update the GitHub release notes (template + Release Notes) |
| `V` | Verify vanilla-Mac readiness |
| `D` | Promote dry run |
| `A` | Full pipeline (`0`–`6` + verify, no publish) |
| `P` | Print config |
| `Q` | Quit |

### 9.2 Windows deploy.ps1

| Key | Step |
|---|---|
| `0` | Build |
| `1` | Sign binaries |
| `2` | Build installer (Inno Setup) |
| `3` | Archive installer |
| `4` | Upload installer to the test folder |
| `5` | Promote to winnow.ca (live) |
| `G` | Publish installer to the GitHub Release |
| `N` | Update the GitHub release notes (template + Release Notes) |
| `V` | Verify Windows readiness |
| `D` | Promote dry run |
| `A` | Full pipeline (`0`–`3` + verify, no publish) |
| `P` | Print config |

## 10 Troubleshooting

- **"Could not read presets"** — run the script from the repo, or update to the current
  script (it now changes to the repo root itself).
- **Notarization 401** — check `xcrun notarytool history --keychain-profile
  AC_APIKEY`. If a profile keeps failing after re-storing it, store the credentials
  under a new profile name and update `NOTARY_KEYCHAIN_PROFILE`.
- **Notarization 403 "required agreement is missing or has expired"** — the Account
  Holder must accept the updated agreement at developer.apple.com/account.
- **Verify fails on an external dylib** — a Qt plugin is linking something outside the
  bundle (e.g. a SQL driver). Fix it before publishing; don't publish past a failed
  verify.
- **Build uses the wrong Qt** — `QT_DIR` in `config.sh` must match the Qt you mean to
  ship. The script reconfigures the build folder if it was set up with a different Qt.
- **Release not showing as latest** — it is still a draft (a file is missing) or was
  marked pre-release. `/releases/latest` skips both.

