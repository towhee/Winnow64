<#
  Copy a freshly extracted commit into the Windows VM's source tree, touching only the
  files whose contents changed.

    powershell -ExecutionPolicy Bypass -File <stage>\packaging\windows\syncstage.ps1 -Stage <stage> -Repo <repo>

  packaging/release.sh extracts `git archive <commit>` into a staging folder first. Untarring
  straight into the tree stamped EVERY file with the commit time, so ninja saw the whole
  tree as changed and each release was a full rebuild. Here a file is copied only when it
  is new or its size or SHA-256 differs, and a copied file gets the current time so it is
  newer than any object built from its old contents. Unchanged files keep their old
  times and stay up to date.

  Never deletes from -Repo; prune.ps1 lists what is not in the commit. Exits 1 on a copy
  failure, else 0.
#>
param([Parameter(Mandatory)][string]$Stage, [Parameter(Mandatory)][string]$Repo)

$Stage = (Resolve-Path $Stage).Path
New-Item -ItemType Directory -Force $Repo | Out-Null
$Repo = (Resolve-Path $Repo).Path

$changed = 0
$total = 0
$now = Get-Date
foreach ($f in Get-ChildItem -Force -Recurse -File $Stage) {
    $total++
    $rel = $f.FullName.Substring($Stage.Length + 1)
    $dst = Join-Path $Repo $rel
    if (Test-Path -LiteralPath $dst -PathType Leaf) {
        $d = Get-Item -LiteralPath $dst -Force
        if ($d.Length -eq $f.Length -and
            (Get-FileHash -LiteralPath $dst -Algorithm SHA256).Hash -eq
            (Get-FileHash -LiteralPath $f.FullName -Algorithm SHA256).Hash) { continue }
    }
    try {
        New-Item -ItemType Directory -Force (Split-Path -Parent $dst) | Out-Null
        Copy-Item -LiteralPath $f.FullName -Destination $dst -Force -ErrorAction Stop
        (Get-Item -LiteralPath $dst -Force).LastWriteTime = $now
        $changed++
    } catch {
        Write-Host "ERROR: could not copy ${rel}: $_" -ForegroundColor Red
        exit 1
    }
}
Write-Host "Synced: $changed of $total file(s) changed"
exit 0
