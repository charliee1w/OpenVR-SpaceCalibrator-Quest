# Run one leg of the Tundra vs Vive 3.0 head tracker A/B test.
param(
    [Parameter(Mandatory = $true)]
    [ValidateSet('tundra', 'vive3')]
    [string]$HeadTracker,
    [switch]$ApplyPresetOnly,
    [switch]$Compare,
    [string]$TundraLog,
    [string]$Vive3Log,
    [int]$MinMinutes = 10
)

$ErrorActionPreference = "Stop"
$abDir = Join-Path $env:LOCALAPPDATA "..\LocalLow\SpaceCalibrator\ab-sessions"
New-Item -ItemType Directory -Force -Path $abDir | Out-Null

if ($Compare) {
    if (-not $TundraLog) { $TundraLog = Join-Path $abDir "tundra-latest.txt" }
    if (-not $Vive3Log) { $Vive3Log = Join-Path $abDir "vive3-latest.txt" }
    & (Join-Path $PSScriptRoot "compare-head-ab.ps1") -TundraLog $TundraLog -Vive3Log $Vive3Log
    exit $LASTEXITCODE
}

$label = if ($HeadTracker -eq 'tundra') { 'Tundra' } else { 'Vive 3.0' }
Write-Host "=== Head A/B session: $label ===" -ForegroundColor Cyan

& (Join-Path $PSScriptRoot "apply-hardware-preset.ps1") -HeadTracker $HeadTracker
Write-Host "Applied preset-quest-$HeadTracker-head.json (kept room cal + serials)."

if ($ApplyPresetOnly) { exit 0 }

Write-Host ""
Write-Host "Before VR:"
Write-Host "  1. Restart SteamVR (preset is in registry)"
Write-Host "  2. Mount $label head tracker - same strap position as usual"
Write-Host "  3. Run Start Calibration once if devices changed"
Write-Host ""
Write-Host "In VR (~$MinMinutes+ min): continuous cal on, normal play, 1 Hz metrics auto-log."
Write-Host ""
$r = Read-Host "Press Enter when done (or 'skip' to skip archive)"

if ($r -ne 'skip') {
    $logDir = Join-Path $env:LOCALAPPDATA "..\LocalLow\SpaceCalibrator\Logs"
    $latest = Get-ChildItem $logDir -Filter "spacecal_log.*.txt" -ErrorAction SilentlyContinue |
        Sort-Object LastWriteTime -Descending | Select-Object -First 1
    if (-not $latest) {
        Write-Error "No metrics log found."
    }
    $dest = Join-Path $abDir "$HeadTracker-latest.txt"
    Copy-Item $latest.FullName $dest -Force
    Write-Host "Archived: $dest"
    & (Join-Path $PSScriptRoot "analyze-spacecal-log.ps1") -LogPath $dest
}

Write-Host ""
Write-Host "Next steps:"
if ($HeadTracker -eq 'tundra') {
    Write-Host "  .\scripts\run-head-ab-session.ps1 -HeadTracker vive3"
} else {
    Write-Host "  .\scripts\run-head-ab-session.ps1 -Compare"
}