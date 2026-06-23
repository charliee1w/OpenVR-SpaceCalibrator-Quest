# One-shot install: deploy binaries, validate, optional starter profile.
param(
    [ValidateSet('quest-vd', 'pico-alvr', 'quest-link', 'none')]
    [string]$Preset = 'quest-vd',
    [switch]$SkipProfile,
    [switch]$MergeProfile,
    [string]$SteamRoot = "${env:ProgramFiles(x86)}\Steam\steamapps\common"
)

$ErrorActionPreference = "Stop"
$scripts = $PSScriptRoot

Write-Host "SpaceCal SLAM fork — install" -ForegroundColor Cyan
Write-Host ""

& (Join-Path $scripts "deploy.ps1") -SteamRoot $SteamRoot
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

& (Join-Path $scripts "validate-install.ps1") -SteamRoot $SteamRoot
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

if (-not $SkipProfile -and $Preset -ne 'none') {
    $importArgs = @{ Preset = $Preset }
    if ($MergeProfile) { $importArgs.Merge = $true }
    & (Join-Path $scripts "import-profile.ps1") @importArgs
}

Write-Host ""
Write-Host "--- Before first VR session ---" -ForegroundColor Yellow
Write-Host "  1. Virtual Desktop -> Settings -> OpenXR runtime -> SteamVR (NOT VDXR)"
Write-Host "  2. Restart SteamVR"
Write-Host "  3. Power on head tracker first, then body trackers"
Write-Host "  4. SpaceCal dashboard -> Copy Chaperone -> Continuous Calibration -> Save profile"
Write-Host ""
Write-Host "Docs: docs\QUICKSTART.md"
Write-Host "Validate session: .\scripts\run-p4-validation.ps1 -AnalyzeOnly"