# Archive or compare head-tracker A/B session logs (Tundra vs Vive 3.0).
param(
    [ValidateSet('tundra', 'vive3')]
    [string]$HeadTracker,
    [switch]$Compare
)

$ErrorActionPreference = "Stop"
$abDir = Join-Path $env:LOCALAPPDATA "..\LocalLow\SpaceCalibrator\ab-sessions"
$logDir = Join-Path $env:LOCALAPPDATA "..\LocalLow\SpaceCalibrator\Logs"
$compareScript = Join-Path $PSScriptRoot "compare-head-ab.ps1"

if (-not (Test-Path $abDir)) {
    New-Item -ItemType Directory -Path $abDir -Force | Out-Null
}

if ($Compare) {
    $tundra = Get-ChildItem -Path $abDir -Filter "tundra-latest.txt" -ErrorAction SilentlyContinue |
        Select-Object -First 1
    $vive3 = Get-ChildItem -Path $abDir -Filter "vive3-latest.txt" -ErrorAction SilentlyContinue |
        Select-Object -First 1
    if (-not $tundra) {
        $tundra = Get-ChildItem -Path $abDir -Filter "tundra-*.txt" -ErrorAction SilentlyContinue |
            Sort-Object LastWriteTime -Descending | Select-Object -First 1
    }
    if (-not $vive3) {
        $vive3 = Get-ChildItem -Path $abDir -Filter "vive3-*.txt" -ErrorAction SilentlyContinue |
            Sort-Object LastWriteTime -Descending | Select-Object -First 1
    }
    if (-not $tundra -or -not $vive3) {
        Write-Error "Need archived tundra and vive3 logs in $abDir. Run -HeadTracker tundra and -HeadTracker vive3 after each session."
    }
    & $compareScript -TundraLog $tundra.FullName -Vive3Log $vive3.FullName
    exit $LASTEXITCODE
}

if (-not $HeadTracker) {
    Write-Host "Head tracker A/B session helper" -ForegroundColor Cyan
    Write-Host ""
    Write-Host "Session 1 (Tundra head):  .\scripts\run-head-ab-session.ps1 -HeadTracker tundra"
    Write-Host "Session 2 (Vive 3.0 head):  .\scripts\run-head-ab-session.ps1 -HeadTracker vive3"
    Write-Host "Compare archived logs:    .\scripts\run-head-ab-session.ps1 -Compare"
    Write-Host ""
    Write-Host "Same room, same VD settings, same strap — only swap the head tracker."
    Write-Host "Archive dir: $abDir"
    exit 0
}

$latest = Get-ChildItem -Path $logDir -Filter "spacecal_log.*.txt" -ErrorAction SilentlyContinue |
    Sort-Object LastWriteTime -Descending |
    Select-Object -First 1
if (-not $latest) {
    Write-Error "No spacecal_log.*.txt in $logDir. Run a VR session with continuous cal first."
}

$stamp = Get-Date -Format "yyyy-MM-ddTHH-mm-ss"
$dest = Join-Path $abDir "$HeadTracker-$stamp.txt"
$latestLink = Join-Path $abDir "$HeadTracker-latest.txt"

Copy-Item -Path $latest.FullName -Destination $dest -Force
Copy-Item -Path $dest -Destination $latestLink -Force

Write-Host "Archived $HeadTracker session" -ForegroundColor Green
Write-Host "  source: $($latest.FullName)"
Write-Host "  saved:  $dest"
Write-Host "  link:   $latestLink"
Write-Host ""
Write-Host "Run -Compare after both sessions are archived."