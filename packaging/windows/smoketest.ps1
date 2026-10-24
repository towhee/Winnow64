<#
  Winnow Windows installer smoke test (run by packaging/release.sh over SSH).

    powershell -ExecutionPolicy Bypass -File packaging\windows\smoketest.ps1 -Version 3_beta4

  Installs out\WinnowSetup-<Version>.exe silently into a temp folder, runs the
  INSTALLED Winnow.exe --selftest on tests\fixtures\images, then uninstalls it.
  This is what catches a DLL or plugin the installer left out, which a test of
  the build tree cannot.

  The installer shares one AppId with every Winnow install, so on a machine that
  already has Winnow installed a silent install would upgrade that copy and the
  uninstall would remove it. In that case the install is skipped and only the
  build tree's Winnow.exe is self-tested; the result says so.

  Exit code 0 = passed.
#>
param([Parameter(Mandatory)][string]$Version, [int]$TimeoutSec = 300)

$ErrorActionPreference = "Stop"
$RepoRoot = Split-Path -Parent (Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path))
$Setup    = Join-Path $RepoRoot "out\WinnowSetup-$Version.exe"
$Fixtures = Join-Path $RepoRoot "tests\fixtures\images"
$AppKey   = '{2663CS48-9R00-4000-8000-77696E6E6F77}_is1'

function Run-SelfTest([string]$Exe) {
    Write-Host "Self-test: $Exe --selftest $Fixtures"
    $p = Start-Process -FilePath $Exe -ArgumentList "--selftest", "`"$Fixtures`"" -PassThru
    if (-not $p.WaitForExit($TimeoutSec * 1000)) {
        $p.Kill()
        throw "selftest did not finish in $TimeoutSec s"
    }
    if ($p.ExitCode -ne 0) { throw "selftest exit code $($p.ExitCode)" }
    Write-Host "   OK  selftest passed" -ForegroundColor Green
}

try {
    if (-not (Test-Path $Setup)) { throw "installer not found: $Setup" }
    $installed = foreach ($root in 'HKLM:\Software\Microsoft\Windows\CurrentVersion\Uninstall',
                                   'HKLM:\Software\WOW6432Node\Microsoft\Windows\CurrentVersion\Uninstall',
                                   'HKCU:\Software\Microsoft\Windows\CurrentVersion\Uninstall') {
        if (Test-Path (Join-Path $root $AppKey)) { $root }
    }
    if ($installed) {
        Write-Host "Winnow is already installed on this machine; install test SKIPPED so that copy is left alone." -ForegroundColor Yellow
        $exe = Get-ChildItem (Join-Path $RepoRoot "build\win-release\app") -Recurse -Filter Winnow.exe | Select-Object -First 1
        if (-not $exe) { throw "no build-tree Winnow.exe to self-test" }
        Run-SelfTest $exe.FullName
        exit 0
    }

    $dir = Join-Path $env:TEMP "WinnowSmoke"
    if (Test-Path $dir) { Remove-Item -Recurse -Force $dir }
    Write-Host "Installing silently into $dir ..."
    $p = Start-Process -FilePath $Setup -ArgumentList "/VERYSILENT", "/SUPPRESSMSGBOXES", "/NORESTART", "/DIR=`"$dir`"" -Wait -PassThru
    if ($p.ExitCode -ne 0) { throw "installer exit code $($p.ExitCode)" }
    $exe = Join-Path $dir "Winnow.exe"
    if (-not (Test-Path $exe)) { throw "installed Winnow.exe not found in $dir" }
    Write-Host "   OK  installed" -ForegroundColor Green

    try { Run-SelfTest $exe }
    finally {
        $unins = Join-Path $dir "unins000.exe"
        if (Test-Path $unins) {
            Write-Host "Uninstalling ..."
            Start-Process -FilePath $unins -ArgumentList "/VERYSILENT", "/SUPPRESSMSGBOXES", "/NORESTART" -Wait | Out-Null
            Write-Host "   OK  uninstalled" -ForegroundColor Green
        }
    }
    exit 0
} catch {
    Write-Host "X Smoke test failed: $($_.Exception.Message)" -ForegroundColor Red
    exit 1
}
