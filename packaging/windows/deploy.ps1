<#
  Winnow Windows Deployment
    build -> (sign) -> Inno Setup installer -> archive -> GitHub Release -> promote

  Windows equivalent of packaging\macos\deploy.command. Self-locating: derives
  every path from its own location in the repo, so it works unchanged on any
  Windows machine after a `git clone`. Machine-specific values (Inno Setup path,
  signing cert, server / upload target) live in config.ps1 — copy
  config.ps1.example to config.ps1 and edit it once per machine.

  Unlike macOS there is no stage/notarize step: the CMake build already deploys
  the Qt runtime (windeployqt) and all third-party DLLs / the ONNX model /
  ExifTool / Winnet next to Winnow.exe as POST_BUILD steps. So the build output
  IS the staged tree — this script builds, optionally signs it, packages it into
  WinnowSetup-<ver>.exe, and (via Promote) publishes that to the server.

  Usage (from a Developer PowerShell for VS 2022, x64):
    powershell -ExecutionPolicy Bypass -File .\deploy.ps1     # interactive menu

  Requirements (see ..\README.md for one-time setup):
    * Visual Studio 2022 (Desktop C++), Qt 6 (MSVC), CMake + Ninja/VS generator
    * Inno Setup 6 (iscc.exe) for the installer
    * OpenSSH client (ssh.exe / scp.exe — built into Windows 10/11) for upload/promote
    * GitHub CLI (gh.exe, `gh auth login` once) for Publish to GitHub Release
    * (optional) a code-signing certificate + Windows SDK signtool.exe
#>

# NOTE: like deploy.command this is an interactive menu, so a failed step must
# report and return to the menu rather than kill the shell. Steps THROW on
# failure; the menu loop wraps each dispatch in try/catch.
$ErrorActionPreference = "Stop"

# --- Locate ourselves in the repo ------------------------------------
$ScriptDir    = Split-Path -Parent $MyInvocation.MyCommand.Path   # packaging\windows
$PackagingDir = Split-Path -Parent $ScriptDir                     # packaging
$RepoRoot     = Split-Path -Parent $PackagingDir                  # repo root (Winnow64)

# --- Load per-machine config (lives beside this script) --------------
$ConfigFile = Join-Path $ScriptDir "config.ps1"
if (-not (Test-Path $ConfigFile)) {
    Write-Host "X Missing $ConfigFile" -ForegroundColor Red
    Write-Host "  Run: Copy-Item `"$ScriptDir\config.ps1.example`" `"$ConfigFile`" and edit it." -ForegroundColor Red
    exit 1
}
. $ConfigFile

# --- Defaults for anything the config omitted ------------------------
if (-not $ServerWebRoot) { $ServerWebRoot = "/var/www/html" }
if (-not $WebBaseUrl)    { $WebBaseUrl    = "https://winnow.ca" }
if (-not $UploadSubdir)  { $UploadSubdir  = "winnow_win/test" }
if ($null -eq $UploadEnabled) { $UploadEnabled = $false }
if ($null -eq $SignEnabled)   { $SignEnabled   = $false }
if (-not $GitHubRepo)    { $GitHubRepo    = "towhee/Winnow64" }

# --- Derived paths ----------------------------------------------------
$BuildDir      = Join-Path $RepoRoot "build\win-release"
$OutDir        = Join-Path $RepoRoot "out"
# CMakeLists redirects the Winnow target into a clean build\win-release\app subdir
# (so the deployable tree isn't mixed with CMake internals). Where Winnow.exe
# lands under there depends on the generator: single-config Ninja puts it in
# app\ directly; a multi-config generator (Visual Studio, what Qt Creator uses by
# default; or Ninja Multi-Config) puts it in app\Release\. Resolve-AppDir finds
# it either way so deploy works against whatever Qt Creator or Build produced.
$AppBase       = Join-Path $BuildDir "app"
$AppDir        = $AppBase          # resolved for real by Resolve-AppDir
$ExePath       = Join-Path $AppDir "Winnow.exe"
$VersionHeader = Join-Path $BuildDir "Main\winnow_version.h"
$Iss           = Join-Path $ScriptDir "winnow.iss"

# Locate the deployed Winnow.exe and point $AppDir/$ExePath at its folder. Throws
# with a clear message if no build output exists yet.
function Resolve-AppDir {
    foreach ($sub in @("", "Release", "RelWithDebInfo", "Debug")) {
        $dir = if ($sub) { Join-Path $AppBase $sub } else { $AppBase }
        $exe = Join-Path $dir "Winnow.exe"
        if (Test-Path $exe) {
            $script:AppDir  = $dir
            $script:ExePath = $exe
            return
        }
    }
    throw "No build output under $AppBase (looked in app\, app\Release\, app\RelWithDebInfo\, app\Debug\) - run Build first, or build in Qt Creator."
}

# Server-derived paths (used by Upload + Promote)
$WinDir    = "$ServerWebRoot/winnow_win"
$IndexPath = "$ServerWebRoot/index.html"

# Version is single-sourced from CMake -> generated winnow_version.h, read back
# AFTER a build. Filled lazily by Resolve-Version / Ensure-Version.
$Version = $null

# Read WINNOW_VERSION from the generated header (single source of truth).
function Resolve-Version {
    if (-not (Test-Path $VersionHeader)) { return $null }
    $m = Select-String -Path $VersionHeader -Pattern '#define\s+WINNOW_VERSION\s+"([^"]+)"'
    if ($m) { return $m.Matches[0].Groups[1].Value }
    return $null
}

# Populate $script:Version when a step runs standalone in a fresh session where
# no build happened yet (the header still exists from a prior build).
function Ensure-Version {
    if ($script:Version) { return }
    $script:Version = Resolve-Version
}

function Setup-Path {
    param([string]$Ver)
    return (Join-Path $OutDir "WinnowSetup-$Ver.exe")
}

# --- run-or-print helpers (Promote dry-run prints; never writes) ------
# ssh/scp WRITES are gated by $script:DryRun. Reads (index.html download,
# directory listings) always run because a dry-run still needs to look.
function Invoke-Ssh {
    param([string]$RemoteCmd, [switch]$ReadOnly)
    if ($script:DryRun -and -not $ReadOnly) {
        Write-Host "   [dry-run] ssh ${ServerUser}@${ServerHost} `"$RemoteCmd`"" -ForegroundColor DarkGray
        return ""
    }
    $out = & ssh -i $ServerSshKey "${ServerUser}@${ServerHost}" $RemoteCmd
    if ($LASTEXITCODE -ne 0) { throw "ssh failed: $RemoteCmd" }
    return $out
}

function Send-Scp {
    param([string]$Local, [string]$Remote)
    if ($script:DryRun) {
        Write-Host "   [dry-run] scp `"$Local`" ${ServerUser}@${ServerHost}:$Remote" -ForegroundColor DarkGray
        return
    }
    & scp -i $ServerSshKey $Local "${ServerUser}@${ServerHost}:$Remote"
    if ($LASTEXITCODE -ne 0) { throw "scp upload failed: $Local -> $Remote" }
}

function Receive-Scp {
    param([string]$Remote, [string]$Local)   # read-only: runs even in dry-run
    & scp -i $ServerSshKey "${ServerUser}@${ServerHost}:$Remote" $Local
    if ($LASTEXITCODE -ne 0) { throw "scp download failed: $Remote" }
}

# Print a +/- line diff of two strings; returns $true if they differ.
function Show-Diff {
    param([string]$Before, [string]$After)
    $b = $Before -split "`r?`n"
    $a = $After  -split "`r?`n"
    $cmp = Compare-Object $b $a
    if (-not $cmp) { Write-Host "   (no changes)"; return $false }
    foreach ($c in $cmp) {
        if ($c.SideIndicator -eq '=>') {
            Write-Host "   + $($c.InputObject.Trim())" -ForegroundColor Green
        } else {
            Write-Host "   - $($c.InputObject.Trim())" -ForegroundColor Red
        }
    }
    return $true
}

# --- 0) Build (CMake) ------------------------------------------------
function Build {
    if (-not (Get-Command cl -ErrorAction SilentlyContinue)) {
        throw "cl.exe (MSVC) not on PATH. Launch from a Developer PowerShell for VS 2022 (x64)."
    }
    $cache = Join-Path $BuildDir "CMakeCache.txt"
    # Respect whatever generator the build dir is already configured with - Qt
    # Creator typically uses the Visual Studio generator, deploy.ps1's own fresh
    # configure uses the Ninja preset. Either is fine; we just build the existing
    # tree. Only configure from scratch (Ninja preset) when no cache exists, so
    # we never fight a generator Qt Creator set up.
    if (-not (Test-Path $cache)) {
        if (-not (Get-Command ninja -ErrorAction SilentlyContinue)) {
            if ($NinjaPath -and (Test-Path $NinjaPath)) {
                $env:PATH = (Split-Path -Parent $NinjaPath) + ";" + $env:PATH
            } else {
                throw "ninja not on PATH (needed for a fresh configure). Launch from a Developer PowerShell for VS 2022 (x64), set `$NinjaPath in config.ps1, or configure the project in Qt Creator first."
            }
        }
        Write-Host "Configuring (cmake --preset win-release)..." -ForegroundColor Cyan
        & cmake --preset win-release -S $RepoRoot
        if ($LASTEXITCODE -ne 0) { throw "cmake configure failed" }
    }
    # --config Release is honoured by multi-config generators and harmlessly
    # ignored by single-config Ninja, so this builds Release either way.
    Write-Host "Building (cmake --build, Release)..." -ForegroundColor Cyan
    & cmake --build $BuildDir --config Release
    if ($LASTEXITCODE -ne 0) { throw "cmake build failed" }
    Resolve-AppDir
    $script:Version = Resolve-Version
    if (-not $script:Version) { throw "Could not read WINNOW_VERSION from $VersionHeader" }
    Write-Host "   OK  Built Winnow $script:Version -> $script:AppDir" -ForegroundColor Green
}

# --- 1) Code-sign the deployed build output --------------------------
function Sign-Binaries {
    Resolve-AppDir
    if (-not $SignEnabled) {
        Write-Host "Signing disabled (SignEnabled = `$false). Binaries left unsigned." -ForegroundColor Yellow
        return
    }
    if (-not (Test-Path $SignToolPath)) { throw "signtool not found: $SignToolPath" }
    Write-Host "Signing binaries in $AppDir ..." -ForegroundColor Cyan
    $toSign = Get-ChildItem -Path $AppDir -Recurse -Include *.exe,*.dll
    foreach ($f in $toSign) {
        & $SignToolPath sign /fd SHA256 /tr $SignTimestampUrl /td SHA256 /n $SignCertSubject $f.FullName
        if ($LASTEXITCODE -ne 0) { throw "signtool failed for $($f.FullName)" }
    }
    Write-Host "   OK  Signed $($toSign.Count) files" -ForegroundColor Green
}

# --- 2) Build the installer (Inno Setup) -----------------------------
function Build-Installer {
    Resolve-AppDir
    Ensure-Version
    if (-not $script:Version) { throw "No version - run Build first." }
    if (-not (Test-Path $InnoSetupCompiler)) { throw "Inno Setup compiler not found: $InnoSetupCompiler" }
    New-Item -ItemType Directory -Force -Path $OutDir | Out-Null

    Write-Host "Building installer (Inno Setup) for Winnow $script:Version from $($script:AppDir) ..." -ForegroundColor Cyan
    & $InnoSetupCompiler "/DMyAppVersion=$script:Version" "/DSourceDir=$($script:AppDir)" "/DOutputDir=$OutDir" $Iss
    if ($LASTEXITCODE -ne 0) { throw "Inno Setup compilation failed" }

    $Setup = Setup-Path $script:Version
    if (-not (Test-Path $Setup)) { throw "Installer not produced: $Setup" }
    if ($SignEnabled) {
        if (-not (Test-Path $SignToolPath)) { throw "signtool not found: $SignToolPath" }
        & $SignToolPath sign /fd SHA256 /tr $SignTimestampUrl /td SHA256 /n $SignCertSubject $Setup
        if ($LASTEXITCODE -ne 0) { throw "signtool failed for $Setup" }
    }
    Write-Host "   OK  Installer: $Setup" -ForegroundColor Green
}

# --- 3) Archive a version-named copy of the installer ----------------
function Archive {
    Ensure-Version
    if (-not $script:Version) { throw "No version - run Build first." }
    $Setup = Setup-Path $script:Version
    if (-not (Test-Path $Setup)) { throw "No installer to archive - run Build installer first." }
    if (-not $ArchiveDir) { Write-Host "ArchiveDir unset; skipping archive." -ForegroundColor Yellow; return }
    New-Item -ItemType Directory -Force -Path $ArchiveDir | Out-Null
    $dated = Join-Path $ArchiveDir ("WinnowSetup-{0}_{1}.exe" -f $script:Version, (Get-Date -Format "yyyy-MM-dd"))
    Copy-Item -Force $Setup $dated
    Write-Host "   OK  Archived: $dated" -ForegroundColor Green
}

# --- 4) Plain upload to a test dir (separate from Promote) -----------
function Upload {
    if (-not $UploadEnabled) { Write-Host "Upload disabled (UploadEnabled = `$false)." -ForegroundColor Yellow; return }
    Ensure-Version
    if (-not $script:Version) { throw "No version - run Build first." }
    $Setup = Setup-Path $script:Version
    if (-not (Test-Path $Setup)) { throw "No installer to upload - run Build installer first." }
    Test-ServerConfig
    $script:DryRun = $false
    $target = "$ServerWebRoot/$UploadSubdir/"
    Write-Host "Uploading installer to ${ServerUser}@${ServerHost}:$target ..." -ForegroundColor Cyan
    Send-Scp $Setup "$ServerWebRoot/$UploadSubdir/WinnowSetup-$script:Version.exe"
    Write-Host "   OK  Uploaded" -ForegroundColor Green
}

# --- Verify Windows readiness ----------------------------------------
# No Gatekeeper/notarization on Windows; this is a structural readiness check:
# the exe and the essential Qt runtime pieces are present next to it, and (if
# signing is enabled) the exe carries a valid Authenticode signature.
function Verify {
    Resolve-AppDir
    $ok = $true
    Write-Host "Verifying deployed build output ($($script:AppDir))..." -ForegroundColor Cyan

    $required = @(
        $script:ExePath,
        (Join-Path $script:AppDir "Qt6Core.dll"),
        (Join-Path $script:AppDir "platforms\qwindows.dll")
    )
    foreach ($r in $required) {
        if (Test-Path $r) { Write-Host "   OK  present: $($r.Substring($script:AppDir.Length).TrimStart('\'))" -ForegroundColor Green }
        else { Write-Host "   X  MISSING: $r" -ForegroundColor Red; $ok = $false }
    }

    if ($SignEnabled) {
        $sig = Get-AuthenticodeSignature $script:ExePath
        if ($sig.Status -eq 'Valid') { Write-Host "   OK  Authenticode signature valid" -ForegroundColor Green }
        else { Write-Host "   X  Authenticode signature: $($sig.Status)" -ForegroundColor Red; $ok = $false }
    } else {
        Write-Host "   (signing disabled - skipping Authenticode check)" -ForegroundColor DarkGray
    }

    if ($ok) { Write-Host "   OK  Readiness checks passed" -ForegroundColor Green }
    else { throw "Readiness checks FAILED (see above)" }
}

# --- Validate the server settings Promote/Upload need ----------------
function Test-ServerConfig {
    foreach ($v in 'ServerUser','ServerHost','ServerSshKey','ServerWebRoot','WebBaseUrl') {
        if (-not (Get-Variable -Name $v -Scope Script -ValueOnly -ErrorAction SilentlyContinue) -and
            -not (Get-Variable -Name $v -ValueOnly -ErrorAction SilentlyContinue)) {
            throw "$v not set in config.ps1"
        }
    }
    if (-not $script:DryRun) {
        if (-not (Test-Path $ServerSshKey)) { throw "ServerSshKey not found: $ServerSshKey (must be an OpenSSH key, not a .ppk)" }
    }
}

# --- 5) Promote: publish a release to the server ---------------------
# Archives the current installer, uploads the new one to current/, repoints the
# landing-page download link, and publishes version.json. Outward-facing
# PRODUCTION writes: backs up index.html before editing, archives (never
# deletes) the old installer, and confirms before the first write.
#   Promote            interactive (detect old version, confirm, then publish)
#   Promote -DryRun    print every ssh/scp it WOULD run; touches nothing
function Promote {
    param([switch]$DryRun)
    $script:DryRun = [bool]$DryRun

    Test-ServerConfig

    # 1) NEW version + installer present
    Ensure-Version
    $New = $script:Version
    if ($script:DryRun -and -not $New) {
        $New = Read-Host "   [dry-run] new version (no built artifact found)"
    }
    if (-not $New) { throw "No version - run Build / Build installer first." }
    $Setup = Setup-Path $New
    if (-not $script:DryRun -and -not (Test-Path $Setup)) {
        throw "Installer not found: $Setup - run Build installer first."
    }

    # 2) Verify gate (skipped offline in dry-run; blocks publish on failure)
    if ($script:DryRun) {
        Write-Host "   [dry-run] would run Verify (publish blocked on failure)" -ForegroundColor DarkGray
        $Old = Read-Host "   [dry-run] old (currently published) version"
    } else {
        Write-Host "Verify gate..." -ForegroundColor Cyan
        Verify   # throws -> aborts publish

        # 3) Determine OLD version from the live landing page (the Windows link's
        #    version lives in the visible text "Windows: Winnow <ver>").
        Write-Host "Detecting current published version (downloading index.html)..." -ForegroundColor Cyan
        $tmp = Join-Path ([System.IO.Path]::GetTempPath()) ("winnow-index-{0}" -f ([guid]::NewGuid().ToString('N')))
        New-Item -ItemType Directory -Force -Path $tmp | Out-Null
        $localIndex = Join-Path $tmp "index.html"
        Receive-Scp $IndexPath $localIndex
        $indexText = Get-Content $localIndex -Raw
        $m = [regex]::Match($indexText, 'Windows:\s*Winnow\s*([0-9][0-9.]*)')
        $Old = ""
        if ($m.Success) { $Old = $m.Groups[1].Value }
        Write-Host "   index.html Windows link version: $(if ($Old) {$Old} else {'<not found>'})"
        $reply = Read-Host "   Old version detected as '$Old'. Enter to accept, or type the correct version"
        if ($reply) { $Old = $reply }
    }
    if (-not $Old -or -not $New) { throw "Need both OLD and NEW versions." }

    # Same version already on the server == "redeploy": replace the published
    # installer with this latest build WITHOUT bumping the version (useful for a
    # tiny fix where re-versioning isn't warranted). Redeploy skips the archive
    # (nothing to preserve - same version) and the index.html repoint (the link
    # already names this version); it just overwrites current\WinnowSetup-<ver>.exe
    # and re-publishes version.json.
    $Redeploy = $false
    if ($Old -eq $New) {
        if ($script:DryRun) {
            Write-Host "   OLD == NEW ($New): REDEPLOY mode (replace server installer, same version)." -ForegroundColor Yellow
            $Redeploy = $true
        } else {
            Write-Host "   Server already has Winnow $New." -ForegroundColor Yellow
            $rd = Read-Host "   Redeploy this build over it (same version, replace installer only)? Type 'yes'"
            if ($rd -eq 'yes') { $Redeploy = $true }
            else { throw "OLD ($Old) == NEW ($New); nothing to promote." }
        }
    }

    # 4) Plan + confirm
    Write-Host ""
    if ($Redeploy) {
        Write-Host "================ REDEPLOY PLAN  (version=$New, unchanged) ================"
        Write-Host "  1. upload  : $Setup  ->  current/WinnowSetup-$New.exe  (overwrite)"
        Write-Host "  2. version : re-publish $WinDir/version.json  ->  $New"
        Write-Host "  (no archive, no index.html change - version is unchanged)"
        Write-Host "  server     : ${ServerUser}@${ServerHost}:$WinDir"
    } else {
        Write-Host "================ PROMOTE PLAN  (old=$Old  new=$New) ================"
        Write-Host "  1. archive : move  current\WinnowSetup-$Old.exe (or current\*.exe)  ->  $Old\"
        Write-Host "  2. upload  : $Setup  ->  current/WinnowSetup-$New.exe"
        Write-Host "  3. index   : back up index.html, repoint Windows link  $Old -> $New"
        Write-Host "  4. version : publish $WinDir/version.json  ->  $New"
        Write-Host "  server     : ${ServerUser}@${ServerHost}:$WinDir"
    }
    if ($script:DryRun) { Write-Host "  MODE: DRY-RUN (nothing will be written)" -ForegroundColor Yellow }
    Write-Host "==================================================================="
    if (-not $script:DryRun) {
        $confirm = Read-Host "Type 'yes' to publish to PRODUCTION"
        if ($confirm -ne 'yes') { throw "Aborted." }
    }

    # 5) Archive old (move every current/*.exe into <old>/; never delete). The
    #    old installer may be the legacy fixed-name file, so archive by glob.
    #    Skipped on redeploy: the version is unchanged, so there is no prior
    #    version to preserve (and we'd archive the very file we're replacing).
    if ($Redeploy) {
        Write-Host "Redeploy: skipping archive (version unchanged)." -ForegroundColor DarkGray
    } else {
        Write-Host "Archiving current/*.exe -> $Old/ ..." -ForegroundColor Cyan
        $archiveCmd = "mkdir -p $WinDir/$Old && for f in $WinDir/current/*.exe; do [ -e `"`$f`" ] && mv `"`$f`" $WinDir/$Old/ || echo nothing-to-archive; done"
        Invoke-Ssh $archiveCmd | Out-Null
    }

    # 6) Upload new
    Write-Host "Uploading WinnowSetup-$New.exe -> current/ ..." -ForegroundColor Cyan
    Send-Scp $Setup "$WinDir/current/WinnowSetup-$New.exe"

    # 7) Update index.html (server-side backup, then download/edit/diff/upload).
    #    Skipped on redeploy: the version is unchanged, so the Windows link
    #    already names WinnowSetup-$New.exe with the right version text.
    if ($Redeploy) {
        Write-Host "Redeploy: skipping index.html update (version unchanged)." -ForegroundColor DarkGray
    } else {
    Write-Host "Updating index.html (Windows link $Old -> $New)..." -ForegroundColor Cyan
    $stamp = Get-Date -Format "yyyyMMdd-HHmmss"
    Invoke-Ssh "cp $IndexPath ${IndexPath}.bak-$stamp" | Out-Null
    if ($script:DryRun) {
        Write-Host "   [dry-run] download $IndexPath; repoint winnow_win/current/*.exe -> WinnowSetup-$New.exe and 'Windows: Winnow $Old' -> 'Windows: Winnow $New'; diff; upload" -ForegroundColor DarkGray
    } else {
        $before = Get-Content $localIndex -Raw
        # Repoint the Windows installer href (matches the legacy fixed name AND
        # any prior WinnowSetup-<ver>.exe) and bump the visible version text.
        $after = [regex]::Replace($before, 'winnow_win/current/[^"''<>\s]+\.exe', "winnow_win/current/WinnowSetup-$New.exe")
        $after = [regex]::Replace($after,  'Windows:\s*Winnow\s*[0-9][0-9.]*',   "Windows: Winnow $New")
        Write-Host "   --- index.html change ---"
        $changed = Show-Diff $before $after
        if ($changed) {
            $ic = Read-Host "   Apply this index.html change? (yes)"
            if ($ic -eq 'yes') {
                $newIndex = Join-Path $tmp "index.new.html"
                # Write without a BOM so the served HTML is byte-clean.
                [System.IO.File]::WriteAllText($newIndex, $after, (New-Object System.Text.UTF8Encoding($false)))
                Send-Scp $newIndex $IndexPath
                Write-Host "   OK  index.html updated (backup: ${IndexPath}.bak-$stamp)" -ForegroundColor Green
            } else {
                Write-Host "   skipped index.html (left unchanged)." -ForegroundColor Yellow
            }
        } else {
            Write-Host "   No matching Windows link found to update - check index.html manually." -ForegroundColor Yellow
        }
    }
    }

    # 7b) Publish version.json (consumed by the in-app "Check for updates")
    Write-Host "Updating $WinDir/version.json -> $New..." -ForegroundColor Cyan
    $downloadUrl = "$WebBaseUrl/winnow_win/current/WinnowSetup-$New.exe"
    $notesUrl    = "$WebBaseUrl/winnow/versions.html"
    $vjson = "{ ""version"": ""$New"", ""url"": ""$downloadUrl"", ""notes"": ""$notesUrl"" }"
    if ($script:DryRun) {
        Write-Host "   [dry-run] upload version.json: $vjson" -ForegroundColor DarkGray
    } else {
        $vtmp = Join-Path $tmp "version.json"
        [System.IO.File]::WriteAllText($vtmp, $vjson, (New-Object System.Text.ASCIIEncoding))
        Send-Scp $vtmp "$WinDir/version.json"
        Write-Host "   OK  version.json updated" -ForegroundColor Green
    }

    # 8) Post-publish check - informational ONLY. All production writes are done
    #    by this point, so a transient SSH/HTTP hiccup here must not abort with
    #    "Step failed" (and must not skip the temp cleanup below). Best-effort.
    Write-Host "Post-publish:" -ForegroundColor Cyan
    try {
        Invoke-Ssh "ls -l $WinDir/current $WinDir/$Old" -ReadOnly | ForEach-Object { Write-Host "   $_" }
    } catch {
        Write-Host "   (skipped server listing - $($_.Exception.Message))" -ForegroundColor Yellow
    }
    if (-not $script:DryRun) {
        Write-Host "   HTTP check: $downloadUrl"
        try { (Invoke-WebRequest -Uri $downloadUrl -Method Head -UseBasicParsing).StatusCode | ForEach-Object { Write-Host "     HTTP $_" } } catch { Write-Host "     $($_.Exception.Message)" -ForegroundColor Yellow }
        Write-Host "   HTTP check: $WebBaseUrl/winnow_win/version.json"
        try { (Invoke-WebRequest -Uri "$WebBaseUrl/winnow_win/version.json" -Method Head -UseBasicParsing).StatusCode | ForEach-Object { Write-Host "     HTTP $_" } } catch { Write-Host "     $($_.Exception.Message)" -ForegroundColor Yellow }
        if ($tmp -and (Test-Path $tmp)) { Remove-Item -Recurse -Force $tmp }
    }
    if ($Redeploy) { Write-Host "Redeploy complete (version=$New, installer replaced)." -ForegroundColor Green }
    else { Write-Host "Promote complete (old=$Old new=$New)." -ForegroundColor Green }
}

# --- Publish the installer to a GitHub Release -----------------------
# GitHub Releases is the primary download location. The Mac and Windows deploys
# run on different machines and share ONE release per version (tag v<version>):
# whichever runs first creates it, the other attaches its asset. The release is
# created as a DRAFT and made public only when every asset in Release-Assets is
# attached, so the notes' direct links (and the README's /releases/latest) never
# point at a file that is not there yet. It goes public as a full release, not a
# pre-release, because /releases/latest skips pre-releases. The tag is made (on
# publish) at the pushed HEAD commit, so push first.

# Assets a release must carry before it goes public.
function Release-Assets([string]$Ver) {
    @("Winnow$Ver.dmg", "Winnow${Ver}_Intel.dmg", "WinnowSetup-$Ver.exe")
}

# isDraft + assets of release $Tag (parsed here: --jq filters with embedded
# quotes do not survive PowerShell's native-argument passing).
function Get-ReleaseInfo([string]$Tag) {
    $json = & gh release view $Tag --repo $GitHubRepo --json isDraft,assets
    if ($LASTEXITCODE -ne 0) { throw "gh release view $Tag failed" }
    ($json -join "`n") | ConvertFrom-Json
}

# Attach $File to release $Tag. On a draft a plain --clobber is fine. On a public
# release --clobber DELETES the old asset before uploading the new one, so its
# link 404s for the whole upload; instead upload under a .part name (which the
# updater ignores), then delete the old asset and rename the new one into place.
function Upload-ReleaseAsset([string]$Tag, [string]$File) {
    $name = Split-Path $File -Leaf
    $info = Get-ReleaseInfo $Tag
    $old  = $info.assets | Where-Object { $_.name -eq $name }
    if ($info.isDraft -or -not $old) {
        & gh release upload $Tag $File --repo $GitHubRepo --clobber
        if ($LASTEXITCODE -ne 0) { throw "gh release upload failed" }
        return
    }
    $tmpDir = Join-Path ([System.IO.Path]::GetTempPath()) ("winnow-upload-" + [guid]::NewGuid())
    New-Item -ItemType Directory -Path $tmpDir | Out-Null
    try {
        $part = Join-Path $tmpDir "$name.part"
        Copy-Item $File $part
        & gh release upload $Tag $part --repo $GitHubRepo --clobber
        if ($LASTEXITCODE -ne 0) { throw "gh release upload failed" }
    } finally { Remove-Item -Recurse -Force $tmpDir -ErrorAction SilentlyContinue }
    $new = (Get-ReleaseInfo $Tag).assets | Where-Object { $_.name -eq "$name.part" }
    if (-not $new) { throw "Uploaded $name.part not found on $Tag" }
    & gh api -X DELETE $old.apiUrl | Out-Null
    if ($LASTEXITCODE -ne 0) { throw "Deleting the old $name failed" }
    & gh api -X PATCH $new.apiUrl -f "name=$name" | Out-Null
    if ($LASTEXITCODE -ne 0) { throw "Renaming $name.part to $name failed" }
}

function Release-Title { "Winnow " + ($script:Version -replace '_', ' ') }

function Changes-File { Join-Path $PackagingDir "changes\$script:Version.md" }

# The notes for release $Tag: packaging\release-notes.md with its {{...}} tokens
# filled in. {{CHANGES}} becomes a "Release Notes" section holding
# packaging\changes\<version>.md, or nothing when that file is absent or empty.
function Release-Notes([string]$Tag) {
    $template = Join-Path $PackagingDir "release-notes.md"
    if (-not (Test-Path $template)) { throw "Release notes template not found: $template" }
    $changes = ""
    $file = Changes-File
    if ((Test-Path $file) -and (Get-Item $file).Length -gt 0) {
        $changes = "## Release Notes`n`n" + (Get-Content -Raw -Encoding UTF8 $file).TrimEnd() + "`n"
    }
    $notes = Get-Content -Raw -Encoding UTF8 $template
    $notes = $notes.Replace('{{TITLE}}', (Release-Title)).Replace('{{REPO}}', $GitHubRepo)
    $notes = $notes.Replace('{{TAG}}', $Tag).Replace('{{VERSION}}', $script:Version)
    return $notes.Replace('{{CHANGES}}', $changes)
}

# Write $Notes to a temp file (UTF-8, no BOM) for gh --notes-file; returns its path.
function Write-ReleaseNotesFile([string]$Notes) {
    $notesFile = Join-Path ([System.IO.Path]::GetTempPath()) "winnow-release-notes.md"
    [System.IO.File]::WriteAllText($notesFile, $Notes, (New-Object System.Text.UTF8Encoding $false))
    return $notesFile
}

# Re-render the notes (template + changes\<version>.md) onto the EXISTING release
# for this version, draft or public. Assets are untouched. Replaces the whole
# body, so edits made on the GitHub web page are lost; make them in the files.
function Update-GitHubNotes {
    if (-not (Get-Command gh -ErrorAction SilentlyContinue)) { throw "gh (GitHub CLI) not installed: winget install GitHub.cli" }
    & gh auth status *> $null
    if ($LASTEXITCODE -ne 0) { throw "gh not logged in - run: gh auth login" }
    Ensure-Version
    if (-not $script:Version) { throw "No version - run Build first." }
    $tag = "v$script:Version"
    & gh release view $tag --repo $GitHubRepo *> $null
    if ($LASTEXITCODE -ne 0) { throw "No release $tag yet - G creates it (with these notes)." }
    $file = Changes-File
    if (-not ((Test-Path $file) -and (Get-Item $file).Length -gt 0)) {
        Write-Host "$file is missing or empty - the notes get no Release Notes section." -ForegroundColor Yellow
    }
    $notes = Release-Notes $tag

    Write-Host ""
    Write-Host "================ RELEASE NOTES  ($GitHubRepo $tag) ================"
    Write-Host $notes
    Write-Host "====================================================================="
    Write-Host "This REPLACES the release's notes (edits made on GitHub are lost)."
    $confirm = Read-Host "Type 'yes' to update the notes on GitHub"
    if ($confirm -ne 'yes') { throw "Aborted." }
    $notesFile = Write-ReleaseNotesFile $notes
    & gh release edit $tag --repo $GitHubRepo --notes-file $notesFile
    if ($LASTEXITCODE -ne 0) { throw "gh release edit failed" }
    Write-Host "   OK  Notes updated: https://github.com/$GitHubRepo/releases/tag/$tag" -ForegroundColor Green
}

function Publish-GitHub {
    if (-not (Get-Command gh -ErrorAction SilentlyContinue)) { throw "gh (GitHub CLI) not installed: winget install GitHub.cli" }
    & gh auth status *> $null
    if ($LASTEXITCODE -ne 0) { throw "gh not logged in - run: gh auth login" }
    Ensure-Version
    if (-not $script:Version) { throw "No version - run Build first." }
    $Setup = Setup-Path $script:Version
    if (-not (Test-Path $Setup)) { throw "Installer not found: $Setup - run Build installer first." }

    Write-Host "Verify gate..." -ForegroundColor Cyan
    Verify   # throws -> aborts publish

    $tag  = "v$script:Version"
    $head = (& git -C $RepoRoot rev-parse HEAD)
    if ($LASTEXITCODE -ne 0) { throw "git rev-parse HEAD failed" }
    & gh release view $tag --repo $GitHubRepo *> $null
    $exists = ($LASTEXITCODE -eq 0)

    if (-not $exists -and -not (& git -C $RepoRoot branch -r --contains $head 2>$null)) {
        throw "HEAD $($head.Substring(0,8)) is not pushed - push first so the tag $tag points at it."
    }

    Write-Host ""
    Write-Host "================ GITHUB RELEASE  ($GitHubRepo) ================"
    if ($exists) { Write-Host "  release : $tag  (exists - attach/replace installer)" }
    else         { Write-Host "  release : $tag  (NEW - tag at $($head.Substring(0,8)))" }
    Write-Host "  upload  : $Setup"
    Write-Host "==================================================================="
    $confirm = Read-Host "Type 'yes' to publish to GitHub"
    if ($confirm -ne 'yes') { throw "Aborted." }

    if (-not $exists) {
        $notesFile = Write-ReleaseNotesFile (Release-Notes $tag)
        & gh release create $tag --repo $GitHubRepo --target $head --title (Release-Title) `
            --notes-file $notesFile --draft
        if ($LASTEXITCODE -ne 0) { throw "gh release create failed" }
    }
    Upload-ReleaseAsset $tag $Setup

    # Go public only when every platform's asset is attached
    $info = Get-ReleaseInfo $tag
    if (-not $info.isDraft) {
        Write-Host "   OK  Updated public release: https://github.com/$GitHubRepo/releases/tag/$tag" -ForegroundColor Green
        return
    }
    $have    = @($info.assets | ForEach-Object { $_.name })
    $missing = @(Release-Assets $script:Version | Where-Object { $have -notcontains $_ })
    if ($missing.Count -gt 0) {
        Write-Host "   $tag stays a DRAFT (invisible to users) until these are attached:" -ForegroundColor Yellow
        $missing | ForEach-Object { Write-Host "     $_" }
        Write-Host "   It goes public automatically when the last one is published with G."
        $confirm = Read-Host "Type 'publish' to make it public now WITHOUT them (anything else = keep draft)"
        if ($confirm -ne 'publish') { return }
    }
    & gh release edit $tag --repo $GitHubRepo --draft=false --latest
    if ($LASTEXITCODE -ne 0) { throw "gh release edit (publish) failed" }
    Write-Host "   OK  Published: https://github.com/$GitHubRepo/releases/tag/$tag" -ForegroundColor Green
    Write-Host "       To change the notes later: edit $(Changes-File), then N."
}

# --- Full pipeline (build -> sign -> installer -> archive -> verify) --
function Full-Pipeline {
    Build
    Sign-Binaries
    Build-Installer
    Archive
    Verify
    if ($UploadEnabled) { Upload } else { Write-Host "Upload to test dir disabled (UploadEnabled = `$false)." -ForegroundColor Yellow }
    Write-Host "FULL PIPELINE complete (not published - run Promote to publish)." -ForegroundColor Green
}

function Print-Config {
    Ensure-Version
    Write-Host "REPO_ROOT          = $RepoRoot"
    Write-Host "BUILD_DIR          = $BuildDir"
    Write-Host "APP_DIR            = $AppDir"
    Write-Host "OUT_DIR            = $OutDir"
    Write-Host "INNO_SETUP         = $InnoSetupCompiler"
    Write-Host "SIGN_ENABLED       = $SignEnabled"
    Write-Host "SIGN_CERT          = $SignCertSubject"
    Write-Host "ARCHIVE_DIR        = $ArchiveDir"
    Write-Host "UPLOAD_ENABLED     = $UploadEnabled"
    Write-Host "UPLOAD_SUBDIR      = $UploadSubdir"
    Write-Host "SERVER             = ${ServerUser}@${ServerHost}:$ServerWebRoot"
    Write-Host "SERVER_SSH_KEY     = $ServerSshKey"
    Write-Host "WEB_BASE_URL       = $WebBaseUrl"
    Write-Host "GITHUB_REPO        = $GitHubRepo"
    Write-Host "VERSION (built)    = $(if ($script:Version) {$script:Version} else {'<not built>'})"
}

# --- Menu -------------------------------------------------------------
while ($true) {
    Clear-Host
    Write-Host "=================== Winnow Windows Deployment ==================="
    Write-Host "0) Build (cmake --build --preset win-release)"
    Write-Host "1) Sign binaries (deployed build output)"
    Write-Host "2) Build installer (Inno Setup)"
    Write-Host "3) Archive installer (local versioned copy)"
    Write-Host "4) Upload installer to test dir"
    Write-Host "5) Promote + publish to server (archive old -> upload new -> update index)"
    Write-Host "   (if server version == this build, offers REDEPLOY: replace installer only)"
    Write-Host "G) Publish installer to GitHub Release (main download location)"
    Write-Host "N) Update GitHub release notes (template + changes\<version>.md)"
    Write-Host "V) Verify Windows readiness"
    Write-Host "D) Promote DRY-RUN (print server commands, write nothing)"
    Write-Host "A) FULL PIPELINE (0-3 + verify; does NOT publish)"
    Write-Host "P) Print config"
    Write-Host "Q) Quit"
    Write-Host "----------------------------------------------------------------"
    $choice = Read-Host "Select option"

    try {
        switch ($choice.ToUpper()) {
            "0" { Build }
            "1" { Sign-Binaries }
            "2" { Build-Installer }
            "3" { Archive }
            "4" { Upload }
            "5" { Promote }
            "G" { Publish-GitHub }
            "N" { Update-GitHubNotes }
            "V" { Verify }
            "D" { Promote -DryRun }
            "A" { Full-Pipeline }
            "P" { Print-Config }
            "Q" { exit 0 }
            default { Write-Host "X Invalid choice" -ForegroundColor Red; Start-Sleep -Seconds 1 }
        }
    } catch {
        Write-Host ""
        Write-Host "X Step failed: $($_.Exception.Message)" -ForegroundColor Red
    }

    Write-Host ""
    Read-Host "Press Enter to return to menu"
}
