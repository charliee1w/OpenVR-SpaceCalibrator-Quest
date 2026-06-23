# Apply P4 winner slider settings into the live registry profile (merge mode).
# Wrapper around import-profile.ps1 for backwards compatibility.
param(
    [string]$WinnerPath,
    [switch]$WhatIf
)

$ErrorActionPreference = "Stop"
$import = Join-Path $PSScriptRoot "import-profile.ps1"

if ($WinnerPath) {
    $args = @{ ProfilePath = $WinnerPath; Merge = $true }
} else {
    $args = @{ Preset = 'p4-winner'; Merge = $true }
}
if ($WhatIf) { $args.WhatIf = $true }

& $import @args
exit $LASTEXITCODE