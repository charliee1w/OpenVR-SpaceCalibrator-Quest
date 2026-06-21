param(
    [Parameter(Position = 0)]
    [string]$LogPath,
    [switch]$Latest
)

function Get-Median([double[]]$Values) {
    if ($Values.Count -eq 0) { return 0 }
    $sorted = $Values | Sort-Object
    $mid = [int][math]::Floor($sorted.Count / 2)
    if ($sorted.Count % 2 -eq 0) {
        return ($sorted[$mid - 1] + $sorted[$mid]) / 2
    }
    return $sorted[$mid]
}

function Get-Percentile([double[]]$Values, [double]$Percentile) {
    if ($Values.Count -eq 0) { return 0 }
    $sorted = $Values | Sort-Object
    $index = [int][math]::Ceiling(($Percentile / 100) * $sorted.Count) - 1
    $index = [math]::Max(0, [math]::Min($index, $sorted.Count - 1))
    return $sorted[$index]
}

function Convert-MetricDouble([string]$Text) {
    if ([string]::IsNullOrWhiteSpace($Text)) { return 0.0 }
    $t = $Text.Trim().ToLowerInvariant()
    if ($t -match 'nan|inf') { return [double]::NaN }
    return [double]$Text
}

$logDir = Join-Path $env:LOCALAPPDATA "..\LocalLow\SpaceCalibrator\Logs"

if ($Latest -or [string]::IsNullOrWhiteSpace($LogPath)) {
    $LogPath = Get-ChildItem -Path $logDir -Filter "spacecal_log.*.txt" -ErrorAction SilentlyContinue |
        Sort-Object LastWriteTime -Descending |
        Select-Object -First 1 -ExpandProperty FullName
}

if (-not $LogPath -or -not (Test-Path $LogPath)) {
    Write-Error "No log file found. Pass -LogPath or run with -Latest after a VR session."
    exit 1
}

Write-Host "Analyzing: $LogPath" -ForegroundColor Cyan

$lines = Get-Content -Path $LogPath
$annotations = @()
$rows = @()

foreach ($line in $lines) {
    if ($line.StartsWith("#")) {
        if ($line -match '# \[([0-9.]+)\] (.+)') {
            $annotations += [pscustomobject]@{ Time = [double]$matches[1]; Event = $matches[2] }
        }
        continue
    }
    if ($line -match '^Timestamp,') { continue }
    if ([string]::IsNullOrWhiteSpace($line)) { continue }

    $cols = $line.Split(',')
    if ($cols.Count -lt 16) { continue }

    $rows += [pscustomobject]@{
        Timestamp            = Convert-MetricDouble $cols[0]
        ErrorRawComputed     = Convert-MetricDouble $cols[13]
        ErrorCurrentCal      = Convert-MetricDouble $cols[14]
        ErrorByRelPose       = Convert-MetricDouble $cols[15]
        JitterRef            = if ($cols.Count -gt 18) { Convert-MetricDouble $cols[18] } else { 0 }
        JitterTarget         = if ($cols.Count -gt 19) { Convert-MetricDouble $cols[19] } else { 0 }
        CalibrationApplied   = if ($cols.Count -gt 20) { $cols[20] } else { '' }
    }
}

Write-Host "`n--- Annotations ($($annotations.Count)) ---" -ForegroundColor Yellow
$annotations | Group-Object Event | Sort-Object Count -Descending | ForEach-Object {
    Write-Host ("  {0,-28} {1}" -f $_.Name, $_.Count)
}

if ($rows.Count -eq 0) {
    Write-Host "`nNo metric rows (enable debug logs or start continuous calibration)." -ForegroundColor Red
    exit 0
}

$duration = $rows[-1].Timestamp - $rows[0].Timestamp

$contSegments = @()
$segStart = $null
foreach ($ann in ($annotations | Sort-Object Time)) {
    if ($ann.Event -eq 'StartContinuousCalibration') { $segStart = $ann.Time }
    elseif ($ann.Event -eq 'EndContinuousCalibration' -and $null -ne $segStart) {
        $contSegments += ($ann.Time - $segStart)
        $segStart = $null
    }
}
$contDuration = if ($contSegments.Count -gt 0) { ($contSegments | Measure-Object -Maximum).Maximum } else { 0 }

$err = $rows.ErrorByRelPose | Where-Object { -not [double]::IsNaN($_) -and $_ -gt 0 }

Write-Host "`n--- Metrics ($($rows.Count) samples, $([math]::Round($duration, 1))s row span) ---" -ForegroundColor Yellow
if ($contDuration -gt 0) {
    Write-Host ("  longest continuous-cal segment (annotations): {0:N0}s ({1:N1} min)" -f $contDuration, ($contDuration / 60))
}
# CSV values are already in mm (see CalibrationCalc.cpp: relPoseError * 1000)
$errMedian = Get-Median @($err)
$errP95 = Get-Percentile @($err) 95
$errMax = ($err | Measure-Object -Maximum).Maximum
Write-Host ("  error_byRelPose  median: {0:N2} mm  p95: {1:N2} mm  max: {2:N2} mm" -f `
    $errMedian, $errP95, $errMax)
Write-Host ("  error_currentCal median: {0:N2} mm" -f (Get-Median @($rows.ErrorCurrentCal)))
Write-Host ("  jitterRef        median: {0:N1}  max: {1:N1}" -f `
    (Get-Median @($rows.JitterRef)), (($rows.JitterRef | Measure-Object -Maximum).Maximum))

$calEvents = $rows | Where-Object { $_.CalibrationApplied -match 'FULL|STATIC' }
Write-Host ("  cal corrections: {0} (FULL: {1}, STATIC: {2})" -f `
    $calEvents.Count, `
    ($calEvents | Where-Object CalibrationApplied -eq 'FULL').Count, `
    ($calEvents | Where-Object CalibrationApplied -eq 'STATIC').Count)

$guardian = ($annotations | Where-Object Event -match 'Guardian').Count
Write-Host ("  guardian events: {0}" -f $guardian)

$durCheck = [math]::Max($duration, $contDuration)
$p4Pass = $errMedian -lt 15 -and $durCheck -ge 600
Write-Host "`n--- P4 exit check ---" -ForegroundColor Yellow
Write-Host ("  duration >= 10 min: {0}" -f ($durCheck -ge 600))
Write-Host ("  median error < 15mm: {0}" -f ($errMedian -lt 15))
Write-Host ("  P4 pass: {0}" -f $p4Pass) -ForegroundColor $(if ($p4Pass) { 'Green' } else { 'DarkYellow' })