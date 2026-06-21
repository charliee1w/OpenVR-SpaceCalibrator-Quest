# Guide + analyze a 10+ minute continuous-cal validation session (P4 optional criterion).
param(
    [switch]$AnalyzeOnly,
    [string]$LogPath,
    [int]$MinMinutes = 10
)

$ErrorActionPreference = "Stop"
$repoRoot = Split-Path $PSScriptRoot -Parent
$minSeconds = $MinMinutes * 60

function Invoke-Analyze([string]$Path) {
    & (Join-Path $PSScriptRoot "analyze-spacecal-log.ps1") -LogPath $Path
    return $LASTEXITCODE
}

if (-not $AnalyzeOnly) {
    Write-Host "=== P4 validation session (contcal7) ===" -ForegroundColor Cyan
    Write-Host ""
    Write-Host "Prerequisites:"
    Write-Host "  1. SteamVR stopped → .\scripts\deploy.ps1 → .\scripts\validate-install.ps1"
    Write-Host "  2. .\scripts\apply-p4-winner.ps1 (keeps your room cal + devices)"
    Write-Host "  3. Restart SteamVR"
    Write-Host ""
    Write-Host "In VR (~$MinMinutes+ minutes):"
    Write-Host "  - Power on head tracker first, then body trackers"
    Write-Host "  - Enable Continuous Calibration (or autostart)"
    Write-Host "  - Walk/dance normally in VRChat - metrics log at 1 Hz automatically"
    Write-Host "  - End session from overlay or exit SteamVR"
    Write-Host ""
    $r = Read-Host "Press Enter when the session is done (or type 'skip' to analyze latest log only)"
    if ($r -eq 'skip') { $AnalyzeOnly = $true }
}

if ([string]::IsNullOrWhiteSpace($LogPath)) {
    $logDir = Join-Path $env:LOCALAPPDATA "..\LocalLow\SpaceCalibrator\Logs"
    $LogPath = Get-ChildItem -Path $logDir -Filter "spacecal_log.*.txt" -ErrorAction SilentlyContinue |
        Sort-Object LastWriteTime -Descending |
        Select-Object -First 1 -ExpandProperty FullName
}

if (-not $LogPath -or -not (Test-Path $LogPath)) {
    Write-Error "No metrics log found. Run a VR session with continuous calibration first."
}

Write-Host "`nAnalyzing: $LogPath" -ForegroundColor Cyan
& (Join-Path $PSScriptRoot "analyze-spacecal-log.ps1") -LogPath $LogPath

# Re-parse for structured summary (analyze script prints but doesn't export)
$lines = Get-Content $LogPath
$rows = @()
$annotations = @()
foreach ($line in $lines) {
    if ($line -match '# \[([0-9.]+)\] (.+)') {
        $annotations += [pscustomobject]@{ Time = [double]$matches[1]; Event = $matches[2] }
        continue
    }
    if ($line -match '^Timestamp,' -or [string]::IsNullOrWhiteSpace($line)) { continue }
    $cols = $line.Split(',')
    if ($cols.Count -lt 16) { continue }
    $rows += [pscustomobject]@{ Timestamp = [double]$cols[0]; ErrorByRelPose = [double]$cols[15] }
}

$contDuration = 0
$segStart = $null
foreach ($ann in ($annotations | Sort-Object Time)) {
    if ($ann.Event -eq 'StartContinuousCalibration') { $segStart = $ann.Time }
    elseif ($ann.Event -eq 'EndContinuousCalibration' -and $null -ne $segStart) {
        $d = $ann.Time - $segStart
        if ($d -gt $contDuration) { $contDuration = $d }
        $segStart = $null
    }
}
$rowSpan = if ($rows.Count -gt 1) { $rows[-1].Timestamp - $rows[0].Timestamp } else { 0 }
$duration = [math]::Max($contDuration, $rowSpan)

$err = $rows.ErrorByRelPose | Where-Object { $_ -gt 0 }
$sorted = $err | Sort-Object
$median = if ($sorted.Count -gt 0) {
    $mid = [int][math]::Floor($sorted.Count / 2)
    if ($sorted.Count % 2 -eq 0) { ($sorted[$mid - 1] + $sorted[$mid]) / 2 } else { $sorted[$mid] }
} else { [double]::NaN }

$pass = $duration -ge $minSeconds -and $median -lt 15 -and $rows.Count -ge ($MinMinutes * 50)

Write-Host "`n=== Validation summary ===" -ForegroundColor Cyan
Write-Host ("  log file:        {0}" -f (Split-Path $LogPath -Leaf))
Write-Host ("  metric rows:     {0} (expect ~{1}+ at 1 Hz)" -f $rows.Count, ($MinMinutes * 60))
Write-Host ("  duration:        {0:N0}s ({1:N1} min) - need >= {2} min" -f $duration, ($duration / 60), $MinMinutes)
Write-Host ("  median error:    {0:N2} mm - need < 15 mm" -f $median)

if ($pass) {
    Write-Host "  RESULT:          P4 validation PASS" -ForegroundColor Green
    Write-Host "`nAppend this row to docs/P4_TUNING.md session log:"
    Write-Host ("  | p4-validation | {0} | contcal7 | 10+ min session | {1:N1} mm | {2:N1} min | PASS |" -f `
        (Get-Date -Format 'yyyy-MM-dd'), $median, ($duration / 60))
    exit 0
}

Write-Host "  RESULT:          P4 validation NOT YET PASS" -ForegroundColor DarkYellow
if ($rows.Count -lt ($MinMinutes * 50)) {
    Write-Host "  Hint: sparse metrics - confirm continuous cal was running full session (not saved-only)."
}
if ($duration -lt $minSeconds) {
    Write-Host "  Hint: session too short - stay in VR with continuous cal for $MinMinutes+ minutes."
}
exit 1