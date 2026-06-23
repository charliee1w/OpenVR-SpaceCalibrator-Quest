# List committed starter profiles and latency tiers.
$repoRoot = Split-Path $PSScriptRoot -Parent
$profilesDir = Join-Path $repoRoot "profiles"

Write-Host "SpaceCal starter profiles" -ForegroundColor Cyan
Write-Host ""
Write-Host ("{0,-12} {1,-36} {2,-8} {3,-8} {4}" -f 'Preset', 'File', 'skew', 'offset', 'Reference')
Write-Host ("{0,-12} {1,-36} {2,-8} {3,-8} {4}" -f '------', '----', '----', '------', '---------')

$rows = @(
    @{ Preset = 'quest-vd';   File = 'starter-quest-vd-lighthouse.json';   Skew = '0.08'; Offset = '0.04'; Ref = 'oculus (VD)' },
    @{ Preset = 'pico-alvr';  File = 'starter-pico-alvr-lighthouse.json';  Skew = '0.06'; Offset = '0.03'; Ref = 'pico (ALVR)' },
    @{ Preset = 'quest-link'; File = 'starter-quest-link-lighthouse.json'; Skew = '0.05'; Offset = '0.02'; Ref = 'oculus (Link)' },
    @{ Preset = 'p4-winner';  File = 'example-quest-lighthouse.json';      Skew = 'code*'; Offset = 'code*'; Ref = 'oculus (tuned)' }
)

foreach ($row in $rows) {
    $path = Join-Path $profilesDir $row.File
    $ok = if (Test-Path $path) { 'ok' } else { 'MISSING' }
    Write-Host ("{0,-12} {1,-36} {2,-8} {3,-8} {4}  [{5}]" -f `
        $row.Preset, $row.File, $row.Skew, $row.Offset, $row.Ref, $ok)
}

Write-Host ""
Write-Host "* p4-winner uses runtime SLAM preset for skew/offset until saved profile overrides." -ForegroundColor DarkGray
Write-Host ""
Write-Host "Import:  .\scripts\import-profile.ps1 -Preset quest-vd"
Write-Host "Merge:   .\scripts\import-profile.ps1 -Preset p4-winner -Merge   # keeps room cal + devices"