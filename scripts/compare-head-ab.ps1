# Side-by-side metrics for Tundra vs Vive 3.0 head tracker A/B logs.
param(
    [Parameter(Mandatory = $true)]
    [string]$TundraLog,
    [Parameter(Mandatory = $true)]
    [string]$Vive3Log
)

$ErrorActionPreference = "Stop"

function Get-Median([double[]]$Values) {
    if ($Values.Count -eq 0) { return [double]::NaN }
    $sorted = $Values | Sort-Object
    $mid = [int][math]::Floor($sorted.Count / 2)
    if ($sorted.Count % 2 -eq 0) {
        return ($sorted[$mid - 1] + $sorted[$mid]) / 2
    }
    return $sorted[$mid]
}

function Convert-MetricDouble([string]$Text) {
    if ([string]::IsNullOrWhiteSpace($Text)) { return 0.0 }
    $t = $Text.Trim().ToLowerInvariant()
    if ($t -match 'nan|inf') { return [double]::NaN }
    return [double]$Text
}

function Get-LogSummary([string]$Path) {
    if (-not (Test-Path $Path)) {
        throw "Log not found: $Path"
    }
    $rows = @()
    foreach ($line in (Get-Content -Path $Path)) {
        if ($line.StartsWith('#') -or $line -match '^Timestamp,' -or [string]::IsNullOrWhiteSpace($line)) { continue }
        $cols = $line.Split(',')
        if ($cols.Count -lt 16) { continue }
        $rows += [pscustomobject]@{
            Timestamp       = Convert-MetricDouble $cols[0]
            ErrorCurrentCal = Convert-MetricDouble $cols[14]
            ErrorByRelPose  = Convert-MetricDouble $cols[15]
        }
    }
    if ($rows.Count -eq 0) {
        return [pscustomobject]@{
            Path            = $Path
            Samples         = 0
            DurationS       = 0
            ErrCalMedian    = [double]::NaN
            ErrRelMedian    = [double]::NaN
        }
    }
    $errCal = @($rows.ErrorCurrentCal | Where-Object { -not [double]::IsNaN($_) -and $_ -gt 0 })
    $errRel = @($rows.ErrorByRelPose | Where-Object { -not [double]::IsNaN($_) -and $_ -gt 0 })
    return [pscustomobject]@{
        Path            = $Path
        Samples         = $rows.Count
        DurationS       = $rows[-1].Timestamp - $rows[0].Timestamp
        ErrCalMedian    = Get-Median $errCal
        ErrRelMedian    = Get-Median $errRel
    }
}

$tundra = Get-LogSummary $TundraLog
$vive3 = Get-LogSummary $Vive3Log

Write-Host "Head tracker A/B comparison" -ForegroundColor Cyan
Write-Host ""
Write-Host ("{0,-18} {1,14} {2,14}" -f '', 'Tundra', 'Vive 3.0')
Write-Host ("{0,-18} {1,14:N0} {2,14:N0}" -f 'samples', $tundra.Samples, $vive3.Samples)
Write-Host ("{0,-18} {1,14:N0}s {2,14:N0}s" -f 'duration', $tundra.DurationS, $vive3.DurationS)
Write-Host ("{0,-18} {1,14:N2} {2,14:N2}" -f 'error_currentCal', $tundra.ErrCalMedian, $vive3.ErrCalMedian)
Write-Host ("{0,-18} {1,14:N2} {2,14:N2}" -f 'error_byRelPose', $tundra.ErrRelMedian, $vive3.ErrRelMedian)
Write-Host ""
Write-Host "Tundra log: $($tundra.Path)" -ForegroundColor DarkGray
Write-Host "Vive3 log:  $($vive3.Path)" -ForegroundColor DarkGray

if (-not [double]::IsNaN($tundra.ErrCalMedian) -and -not [double]::IsNaN($vive3.ErrCalMedian)) {
    $winner = if ($tundra.ErrCalMedian -lt $vive3.ErrCalMedian) { 'Tundra' } else { 'Vive 3.0' }
    Write-Host "`nLower error_currentCal on paper: $winner (confirm feel in VR)" -ForegroundColor $(if ($winner -eq 'Tundra') { 'Green' } else { 'Yellow' })
}