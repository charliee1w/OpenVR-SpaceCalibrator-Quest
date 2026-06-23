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
        PosOffsetCurrentCalX = if ($cols.Count -gt 4) { Convert-MetricDouble $cols[4] } else { 0 }
        PosOffsetCurrentCalY = if ($cols.Count -gt 5) { Convert-MetricDouble $cols[5] } else { 0 }
        PosOffsetCurrentCalZ = if ($cols.Count -gt 6) { Convert-MetricDouble $cols[6] } else { 0 }
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

$errRel = @($rows.ErrorByRelPose | Where-Object { -not [double]::IsNaN($_) -and $_ -gt 0 })
$errCal = @($rows.ErrorCurrentCal | Where-Object { -not [double]::IsNaN($_) -and $_ -gt 0 })
$slamSession = ($annotations | Where-Object Event -eq 'SlamReferencePreset').Count -gt 0

Write-Host "`n--- Metrics ($($rows.Count) samples, $([math]::Round($duration, 1))s row span) ---" -ForegroundColor Yellow
if ($contDuration -gt 0) {
    Write-Host ("  longest continuous-cal segment (annotations): {0:N0}s ({1:N1} min)" -f $contDuration, ($contDuration / 60))
}
if ($slamSession) {
    Write-Host "  session type: Quest SLAM reference (lock-relative tuning)" -ForegroundColor DarkCyan
}
# CSV values are already in mm (see CalibrationCalc.cpp: relPoseError * 1000)
$errRelMedian = Get-Median $errRel
$errRelP95 = Get-Percentile $errRel 95
$errRelMax = if ($errRel.Count -gt 0) { ($errRel | Measure-Object -Maximum).Maximum } else { 0 }
$errCalMedian = Get-Median $errCal
$errCalP95 = Get-Percentile $errCal 95
$errCalMax = if ($errCal.Count -gt 0) { ($errCal | Measure-Object -Maximum).Maximum } else { 0 }
Write-Host ("  error_byRelPose  median: {0:N2} mm  p95: {1:N2} mm  max: {2:N2} mm (diagnostic)" -f `
    $errRelMedian, $errRelP95, $errRelMax)
Write-Host ("  error_currentCal median: {0:N2} mm  p95: {1:N2} mm  max: {2:N2} mm (applied cal)" -f `
    $errCalMedian, $errCalP95, $errCalMax)
Write-Host ("  jitterRef        median: {0:N1}  max: {1:N1}" -f `
    (Get-Median @($rows.JitterRef)), (($rows.JitterRef | Measure-Object -Maximum).Maximum))

$calEvents = $rows | Where-Object { $_.CalibrationApplied -match 'FULL|STATIC' }
Write-Host ("  cal corrections: {0} (FULL: {1}, STATIC: {2})" -f `
    $calEvents.Count, `
    ($calEvents | Where-Object CalibrationApplied -eq 'FULL').Count, `
    ($calEvents | Where-Object CalibrationApplied -eq 'STATIC').Count)

$driftApply = ($annotations | Where-Object Event -eq 'LockRelDriftApply').Count
Write-Host ("  lock-rel drift applies: {0}" -f $driftApply)

$divergedApply = ($annotations | Where-Object Event -eq 'DivergedRecoveryApply').Count
$divergedTrim = ($annotations | Where-Object Event -eq 'DivergedRecoveryTrimSamples').Count
$lockRelRejects = @($annotations | Where-Object { $_.Event -like 'LockRelReject:*' })
Write-Host ("  diverged recovery applies: {0}" -f $divergedApply)
Write-Host ("  diverged sample trims: {0}" -f $divergedTrim)
Write-Host ("  lock-rel reject annotations: {0}" -f $lockRelRejects.Count)
if ($lockRelRejects.Count -gt 0) {
    $lockRelRejects | ForEach-Object { $_.Event -replace '^LockRelReject:', '' } |
        Group-Object | Sort-Object Count -Descending | Select-Object -First 5 | ForEach-Object {
            Write-Host ("    {0,-36} {1}" -f $_.Name, $_.Count)
        }
}

$guardian = ($annotations | Where-Object Event -match 'Guardian').Count
Write-Host ("  guardian events: {0}" -f $guardian)

# Detect frozen applied offset with rising error (stuck-apply signature from long sessions)
$stuckRuns = @()
$runStart = $null
$runOffsetKey = ''
$runStartErr = 0.0
$runLen = 0
foreach ($row in $rows) {
    $offsetKey = ('{0:N3},{1:N3},{2:N3}' -f $row.PosOffsetCurrentCalX, $row.PosOffsetCurrentCalY, $row.PosOffsetCurrentCalZ)
    $hasOffset = [math]::Abs($row.PosOffsetCurrentCalX) + [math]::Abs($row.PosOffsetCurrentCalY) + [math]::Abs($row.PosOffsetCurrentCalZ) -gt 0.001
    if ($hasOffset -and $offsetKey -eq $runOffsetKey) {
        $runLen++
    } else {
        if ($runLen -ge 30 -and $null -ne $runStart) {
            $endErr = $rows | Where-Object { $_.Timestamp -ge $runStart.Timestamp } | Select-Object -Last 1
            if ($endErr -and $endErr.ErrorCurrentCal -gt ($runStartErr + 5)) {
                $stuckRuns += [pscustomobject]@{
                    StartS   = $runStart.Timestamp
                    Length   = $runLen
                    StartErr = $runStartErr
                    EndErr   = $endErr.ErrorCurrentCal
                }
            }
        }
        $runStart = $row
        $runStartErr = $row.ErrorCurrentCal
        $runOffsetKey = $offsetKey
        $runLen = 1
    }
}
if ($runLen -ge 30 -and $null -ne $runStart) {
    $endErr = $rows | Select-Object -Last 1
    if ($endErr.ErrorCurrentCal -gt ($runStartErr + 5)) {
        $stuckRuns += [pscustomobject]@{
            StartS   = $runStart.Timestamp
            Length   = $runLen
            StartErr = $runStartErr
            EndErr   = $endErr.ErrorCurrentCal
        }
    }
}
if ($stuckRuns.Count -gt 0) {
    Write-Host ("  stuck-apply runs: {0}" -f $stuckRuns.Count) -ForegroundColor DarkYellow
    $stuckRuns | Select-Object -First 3 | ForEach-Object {
        Write-Host ("    +{0:N0}s for {1}s, errCal {2:N1} -> {3:N1} mm" -f $_.StartS, $_.Length, $_.StartErr, $_.EndErr)
    }
} else {
    Write-Host "  stuck-apply runs: 0"
}

$tailRows = @($rows | Where-Object { $_.Timestamp -ge ($rows[-1].Timestamp - 300) })
if ($tailRows.Count -gt 0) {
    $tailErr = @($tailRows.ErrorCurrentCal | Where-Object { -not [double]::IsNaN($_) -and $_ -gt 0 })
    if ($tailErr.Count -gt 0) {
        $tailSorted = $tailErr | Sort-Object
        $tailMid = $tailSorted[[int][math]::Floor($tailSorted.Count / 2)]
        Write-Host ("  last 5 min error_currentCal median: {0:N2} mm (session median: {1:N2} mm)" -f $tailMid, $errCalMedian)
    }
}

$durCheck = [math]::Max($duration, $contDuration)
$primaryMetric = if ($slamSession) { 'error_currentCal' } else { 'error_byRelPose' }
$primaryMedian = if ($slamSession) { $errCalMedian } else { $errRelMedian }
$tailMedian = if ($tailRows.Count -gt 0) {
    $te = @($tailRows.ErrorCurrentCal | Where-Object { -not [double]::IsNaN($_) -and $_ -gt 0 } | Sort-Object)
    if ($te.Count -gt 0) { $te[[int][math]::Floor($te.Count / 2)] } else { 0 }
} else { 0 }
$stuckTail = $stuckRuns.Count -gt 0 -and ($tailMedian -gt 25 -or $calEvents.Count -eq 0)
$p4Pass = $primaryMedian -lt 15 -and $durCheck -ge 600 -and ($tailMedian -eq 0 -or $tailMedian -lt 25) -and -not $stuckTail
Write-Host "`n--- P4 exit check ---" -ForegroundColor Yellow
Write-Host ("  duration >= 10 min: {0}" -f ($durCheck -ge 600))
Write-Host ("  primary metric: {0}" -f $primaryMetric)
Write-Host ("  median error < 15mm ({0}): {1}" -f $primaryMetric, ($primaryMedian -lt 15))
if ($slamSession) {
    Write-Host ("  error_byRelPose median (diagnostic only): {0:N2} mm" -f $errRelMedian)
}
if ($tailMedian -gt 0) {
    Write-Host ("  last 5 min median < 25mm: {0}" -f ($tailMedian -lt 25))
}
if ($stuckTail) {
    Write-Host "  stuck-apply tail with no corrections: True" -ForegroundColor DarkYellow
}
Write-Host ("  P4 pass: {0}" -f $p4Pass) -ForegroundColor $(if ($p4Pass) { 'Green' } else { 'DarkYellow' })