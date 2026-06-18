<#
  Winnow Windows Deployment
    build -> (optional sign) -> Inno Setup installer -> (optional upload)

  Self-locating: derives all paths from its own location in the repo, so it
  works unchanged on any Windows machine after a `git clone`. Machine-specific
  values (Qt path, signing cert, Inno Setup path, upload target) live in
  ..\config.ps1 — copy ..\config.ps1.example to ..\config.ps1 and edit once.

  The CMake build already deploys the Qt runtime (windeployqt) and all
  third-party DLLs / the ONNX model / ExifTool / Winnet next to Winnow.exe as
  POST_BUILD steps (see CMakeLists.txt), so this script only builds, optionally
  signs, and packages that output into a Setup.exe.

  Usage (from a Developer PowerShell / VS x64 environment):
    .\deploy.ps1            # full pipeline
    .\deploy.ps1 -NoBuild   # package the existing build output
#>

param(
    [switch]$NoBuild,
    [switch]$NoInstaller
)

$ErrorActionPreference = "Stop"

# --- Locate ourselves in the repo ------------------------------------
$ScriptDir    = Split-Path -Parent $MyInvocation.MyCommand.Path   # packaging\windows
$PackagingDir = Split-Path -Parent $ScriptDir                     # packaging
$RepoRoot     = Split-Path -Parent $PackagingDir                  # repo root

# --- Load per-machine config -----------------------------------------
$ConfigFile = Join-Path $ScriptDir "config.ps1"
if (-not (Test-Path $ConfigFile)) {
    Write-Error "Missing $ConfigFile. Run: Copy-Item `"$ScriptDir\config.ps1.example`" `"$ConfigFile`" and edit it."
}
. $ConfigFile

# --- Derived paths ----------------------------------------------------
$BuildDir   = Join-Path $RepoRoot "build\win-release"
$OutDir     = Join-Path $RepoRoot "out"
# Visual Studio (multi-config) generator places the exe under \Release.
$AppDir     = Join-Path $BuildDir "Release"
$ExePath    = Join-Path $AppDir "Winnow.exe"

# --- Build ------------------------------------------------------------
if (-not $NoBuild) {
    Write-Host "Building (cmake --build --preset win-release)..." -ForegroundColor Cyan
    if (-not (Test-Path $BuildDir)) {
        cmake --preset win-release -S $RepoRoot
    }
    cmake --build --preset win-release --config Release
}
if (-not (Test-Path $ExePath)) { Write-Error "Build output not found: $ExePath" }

# --- Resolve version from the generated header (single source) --------
$VersionHeader = Join-Path $BuildDir "Main\winnow_version.h"
$Version = $null
if (Test-Path $VersionHeader) {
    $m = Select-String -Path $VersionHeader -Pattern '#define\s+WINNOW_VERSION\s+"([^"]+)"'
    if ($m) { $Version = $m.Matches[0].Groups[1].Value }
}
if (-not $Version) { Write-Error "Could not read WINNOW_VERSION from $VersionHeader" }
Write-Host "Winnow version: $Version" -ForegroundColor Green

# --- Optional code signing -------------------------------------------
if ($SignEnabled) {
    Write-Host "Signing binaries..." -ForegroundColor Cyan
    $toSign = Get-ChildItem -Path $AppDir -Recurse -Include *.exe,*.dll
    foreach ($f in $toSign) {
        & $SignToolPath sign /fd SHA256 /tr $SignTimestampUrl /td SHA256 `
            /n $SignCertSubject $f.FullName
        if ($LASTEXITCODE -ne 0) { Write-Error "signtool failed for $($f.FullName)" }
    }
    Write-Host "  Signed $($toSign.Count) files" -ForegroundColor Green
} else {
    Write-Host "Signing disabled (SignEnabled = `$false). Installer will be unsigned." -ForegroundColor Yellow
}

# --- Build installer (Inno Setup) ------------------------------------
if (-not $NoInstaller) {
    if (-not (Test-Path $InnoSetupCompiler)) { Write-Error "Inno Setup compiler not found: $InnoSetupCompiler" }
    New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
    $Iss = Join-Path $ScriptDir "winnow.iss"
    Write-Host "Building installer..." -ForegroundColor Cyan
    & $InnoSetupCompiler `
        "/DMyAppVersion=$Version" `
        "/DSourceDir=$AppDir" `
        "/DOutputDir=$OutDir" `
        $Iss
    if ($LASTEXITCODE -ne 0) { Write-Error "Inno Setup compilation failed" }
    $Setup = Join-Path $OutDir "WinnowSetup-$Version.exe"
    if ($SignEnabled -and (Test-Path $Setup)) {
        & $SignToolPath sign /fd SHA256 /tr $SignTimestampUrl /td SHA256 /n $SignCertSubject $Setup
    }
    Write-Host "Installer: $Setup" -ForegroundColor Green
}

# --- Optional upload --------------------------------------------------
if ($UploadEnabled) {
    $Setup = Join-Path $OutDir "WinnowSetup-$Version.exe"
    Write-Host "Uploading $Setup to $UploadTarget..." -ForegroundColor Cyan
    scp -i $UploadSshKey $Setup $UploadTarget
    Write-Host "  Uploaded" -ForegroundColor Green

    # Publish version.json consumed by the in-app "Check for updates" feature.
    if ($VersionJsonTarget) {
        if (-not $WebBaseUrl) { $WebBaseUrl = "https://winnow.ca" }
        $DownloadUrl = "$WebBaseUrl/winnow_win/current/WinnowSetup-$Version.exe"
        $NotesUrl    = "$WebBaseUrl/winnow/versions.html"
        $VersionJson = "{ ""version"": ""$Version"", ""url"": ""$DownloadUrl"", ""notes"": ""$NotesUrl"" }"
        $VTmp = Join-Path $OutDir "version.json"
        Set-Content -Path $VTmp -Value $VersionJson -Encoding ASCII
        Write-Host "Uploading version.json to $VersionJsonTarget..." -ForegroundColor Cyan
        scp -i $UploadSshKey $VTmp $VersionJsonTarget
        Write-Host "  version.json published ($Version)" -ForegroundColor Green
    } else {
        Write-Host "version.json not published (VersionJsonTarget not set)." -ForegroundColor Yellow
    }
} else {
    Write-Host "Upload disabled (UploadEnabled = `$false)." -ForegroundColor Yellow
}

Write-Host "Done." -ForegroundColor Green
