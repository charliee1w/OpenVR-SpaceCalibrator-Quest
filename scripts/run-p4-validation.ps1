# Run or analyze a 10+ minute P4 validation session.
param(
    [switch]$AnalyzeOnly,
    [string]$LogPath,
    [switch]$Latest
)

$ErrorActionPreference = "Stop"
$analyzer = Join-Path $PSScriptRoot "analyze-spacecal-log.ps1"

if ($AnalyzeOnly -or $LogPath -or $Latest) {
    if ($LogPath) {
        & $analyzer -LogPath $LogPath
    } elseif ($Latest) {
        & $analyzer -Latest
    } else {
        & $analyzer -Latest
    }
    exit $LASTEXITCODE
}

Write-Host "P4 validation — live session checklist" -ForegroundColor Cyan
Write-Host ""
Write-Host "Before play:"
Write-Host "  1. Virtual Desktop -> OpenXR runtime -> SteamVR (not VDXR)"
Write-Host "  2. SteamVR stopped -> .\scripts\deploy.ps1 -> .\scripts\validate-install.ps1"
Write-Host "  3. .\scripts\apply-p4-winner.ps1  (if profile was reset)"
Write-Host "  4. Restart SteamVR; head tracker on first; start continuous calibration"
Write-Host ""
Write-Host "During play (target >= 10 min):"
Write-Host "  - Normal VRChat / FBT play with continuous cal running"
Write-Host "  - Metrics log 1 row/sec automatically (no debug checkbox)"
Write-Host ""
Write-Host "After play:"
Write-Host "  pwsh -File .\scripts\run-p4-validation.ps1 -AnalyzeOnly"
Write-Host ""
Write-Host "Pass heuristic: duration >= 10 min, error_currentCal median < 15 mm,"
Write-Host "last 5 min median < 25 mm, no stuck-apply tail without corrections."
Write-Host ""
Write-Host "Log dir: $env:LOCALAPPDATA\..\LocalLow\SpaceCalibrator\Logs" -ForegroundColor DarkGray