<#
  Tidy the Windows VM's source tree after packaging/release.sh syncs a commit into it.

    powershell -ExecutionPolicy Bypass -File packaging\windows\prune.ps1 -Manifest out\manifest.txt

  The sync is `git archive | tar -x`, which writes every file in the commit and removes
  none, so anything else in the tree stays there for good. Two consequences:

  - AppleDouble files ("._name", the extended-attribute companions a macOS archive or
    copy leaves) are DELETED. They are never legitimate, and they are not harmless: on
    Windows "._sample01.jpg" is an ordinary file, so the selftests loaded four of them as
    undecodable images and failed (2026-10-25).
  - Any other file that is not in the commit, outside the folders known to be untracked
    (build, Lib, out, release\et.exe, packaging\windows\config.ps1), is LISTED, not
    deleted: it may be something put there by hand, and CMake's globs can pick up a stale
    source file, so it is worth seeing.

  -Manifest is the commit's file list, one repo-relative path per line (git ls-tree -r
  --name-only). Always exits 0; the listing is a warning.
#>
param([Parameter(Mandatory)][string]$Manifest)

$RepoRoot = Split-Path -Parent (Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path))
$untrackedDirs  = @('build', 'Lib', 'out')
$untrackedFiles = @('release/et.exe', 'packaging/windows/config.ps1')

$tracked = New-Object 'System.Collections.Generic.HashSet[string]' ([StringComparer]::OrdinalIgnoreCase)
Get-Content $Manifest | ForEach-Object { [void]$tracked.Add($_) }

$removed = 0
$stale = @()
Get-ChildItem -Force -Recurse -File $RepoRoot | ForEach-Object {
    $rel = $_.FullName.Substring($RepoRoot.Length + 1).Replace('\', '/')
    if ($untrackedDirs -contains ($rel -split '/')[0]) { return }
    if ($_.Name.StartsWith('._')) { Remove-Item -Force $_.FullName; $removed++; return }
    if ($untrackedFiles -contains $rel) { return }
    if (-not $tracked.Contains($rel)) { $stale += $rel }
}

if ($removed) { Write-Host "Removed $removed AppleDouble (._*) files" }
if ($stale.Count) {
    Write-Host "WARNING: $($stale.Count) file(s) in the tree are not in the commit:" -ForegroundColor Yellow
    $stale | Select-Object -First 20 | ForEach-Object { Write-Host "   $_" }
    if ($stale.Count -gt 20) { Write-Host "   ... and $($stale.Count - 20) more" }
}
exit 0
