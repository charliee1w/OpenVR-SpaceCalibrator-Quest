# Side-by-side metrics comparison for Tundra vs Vive 3.0 head tracker A/B.
param(
    [Parameter(Mandatory = $true)]
    [string]$TundraLog,
    [Parameter(Mandatory = $true)]
    [string]$Vive3Log
)

$ErrorActionPreference = "Stop"

function Get-LogStats([string]$Path) {
    if (-not (Test-Path $Path)) {
        Write-Error "Log not found: $Path"
    }

    $lines = Get-Content $Path
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
        $rows += [pscustomobject]@{
            Timestamp          = [double]$cols[0]
            ErrorByRelPose     = [double]$cols[15]
            JitterRef          = if ($cols.Count -gt 18) { [double]$cols[18] } else { 0 }
            CalibrationApplied = if ($cols.Count -gt 20) { $cols[20] } else { '' }
        }
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

    $err = @($rows.ErrorByRelPose | Where-Object { $_ -gt 0 } | Sort-Object)
    $median = if ($err.Count -gt 0) {
        $mid = [int][math]::Floor($err.Count / 2)
        if ($err.Count % 2 -eq 0) { ($err[$mid - 1] + $err[$mid]) / 2 } else { $err[$mid] }
    } else { [double]::NaN }
    $p95idx = [math]::Max(0, [math]::Min([int][math]::Ceiling(0.95 * $err.Count) - 1, $err.Count - 1))
    $p95 = if ($err.Count -gt 0) { $err[$p95idx] } else { [double]::NaN }

    $jitter = @($rows.JitterRef | Where-Object { $_ -gt 0 })
    $jitterMed = if ($jitter.Count -gt 0) {
        $s = $jitter | Sort-Object
        $mid = [int][math]::Floor($s.Count / 2)
        if ($s.Count % 2 -eq 0) { ($s[$mid - 1] + $s[$mid]) / 2 } else { $s[$mid] }
    } else { 0 }

    $corrections = ($rows | Where-Object { $_.CalibrationApplied -match 'FULL|STATIC' }).Count
    $guardian = ($annotations | Where-Object Event -match 'Guardian').Count

    return [pscustomobject]@{
        File        = Split-Path $Path -Leaf
        Rows        = $rows.Count
        DurationSec = [math]::Max($contDuration, $rowSpan)
        MedianMm    = $median
        P95Mm       = $p95
        MaxMm       = if ($err.Count -gt 0) { ($err | Measure-Object -Maximum).Maximum } else { [double]::NaN }
        JitterMed   = $jitterMed
        Corrections = $corrections
        Guardian    = $guardian
    }
}

$t = Get-LogStats $TundraLog
$v = Get-LogStats $Vive3Log

function Fmt([double]$v, [string]$suffix = '') {
    if ([double]::IsNaN($v)) { return 'n/a' }
    return ('{0:N2}{1}' -f $v, $suffix)
}

Write-Host "`n=== Head tracker A/B comparison ===" -ForegroundColor Cyan
Write-Host ("{0,-22} {1,-12} {2,-12}" -f '', 'Tundra', 'Vive 3.0')
Write-Host ("{0,-22} {1,-12} {2,-12}" -f 'log', $t.File, $v.File)
Write-Host ("{0,-22} {1,-12} {2,-12}" -f 'metric rows', $t.Rows, $v.Rows)
Write-Host ("{0,-22} {1,-12} {2,-12}" -f 'duration (min)', ('{0:N1}' -f ($t.DurationSec / 60)), ('{0:N1}' -f ($v.DurationSec / 60)))
Write-Host ("{0,-22} {1,-12} {2,-12}" -f 'error median (mm)', (Fmt $t.MedianMm), (Fmt $v.MedianMm))
Write-Host ("{0,-22} {1,-12} {2,-12}" -f 'error p95 (mm)', (Fmt $t.P95Mm), (Fmt $v.P95Mm))
Write-Host ("{0,-22} {1,-12} {2,-12}" -f 'error max (mm)', (Fmt $t.MaxMm), (Fmt $v.MaxMm))
Write-Host ("{0,-22} {1,-12} {2,-12}" -f 'jitterRef median', (Fmt $t.JitterMed), (Fmt $v.JitterMed))
Write-Host ("{0,-22} {1,-12} {2,-12}" -f 'corrections', $t.Corrections, $v.Corrections)
Write-Host ("{0,-22} {1,-12} {2,-12}" -f 'guardian events', $t.Guardian, $v.Guardian)

$winner = 'inconclusive'
if (-not [double]::IsNaN($t.MedianMm) -and -not [double]::IsNaN($v.MedianMm)) {
    if ($t.MedianMm -lt $v.MedianMm - 1) { $winner = 'Tundra' }
    elseif ($v.MedianMm -lt $t.MedianMm - 1) { $winner = 'Vive 3.0' }
    else { $winner = 'tie (within 1 mm)' }
}

Write-Host "`nLower median error_byRelPose wins (subjective FBT feel still matters)." -ForegroundColor Yellow
Write-Host ("Recommendation from metrics: {0}" -f $winner) -ForegroundColor $(if ($winner -eq 'Vive 3.0') { 'Green' } elseif ($winner -eq 'Tundra') { 'Green' } else { 'DarkYellow' })

$bothValid = $t.DurationSec -ge 600 -and $v.DurationSec -ge 600 -and $t.Rows -ge 300 -and $v.Rows -ge 300
if (-not $bothValid) {
    Write-Host "`nWarning: one or both sessions are short or sparse - rerun 10+ min per tracker." -ForegroundColor DarkYellow
    exit 1
}
exit 0