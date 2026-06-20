# Deployment & operations

**Version:** see `src/common/Version.h` (`SPACECAL_VERSION_STRING`)

---

## Build

```powershell
.\GenWin64.bat
cmake --build bin --config Release
```

## Deploy (SteamVR stopped)

```powershell
.\scripts\deploy.ps1
```

Manual paths: see [README.md](../README.md#deploy-to-steamvr).

## Verification

```powershell
# DLL present
Test-Path "$env:ProgramFiles(x86)\Steam\steamapps\common\SteamVR\drivers\01spacecalibrator\bin\win64\driver_01spacecalibrator.dll"

# IPC pipe (SteamVR running)
Test-Path "\\.\pipe\OpenVRSpaceCalibratorDriver"

# Driver log tail
Get-Content "$env:ProgramFiles(x86)\Steam\steamapps\common\SteamVR\drivers\01spacecalibrator\bin\win64\space_calibrator_driver.log" -Tail 15
```

Expect: `OpenVR-SpaceCalibratorDriver 1.5.1-gore-contcal5 loaded` and `IPC client connected`.

## Logs

| Log | Path |
|-----|------|
| Metrics CSV | `%LOCALAPPDATA%\..\LocalLow\SpaceCalibrator\Logs\spacecal_log.*.txt` |
| Driver | `SteamVR/drivers/01spacecalibrator/bin/win64/space_calibrator_driver.log` |
| SteamVR | `Steam/logs/vrserver.txt` |

## Common incidents

### Driver unavailable (FileNotFound 103)

`SteamVR\drivers\01spacecalibrator\bin\win64\` missing DLL or only `*.dll.stale` present. Redeploy driver; ensure `driver.vrdrivermanifest` at folder root.

### Wrong overlay binary

Legacy `C:\Program Files\SpaceCalibrator\` (pushrax) may autolaunch. Disable in `Steam/config/vrappconfig/pushrax.SpaceCalibrator.vrappconfig`. Use `steam.overlay.3368750` from `01spacecalibrator`.

### Autolaunch race

Overlay connects before IPC ready — fixed in contcal4+ via `IPCClient` retry loop (~15 s).

## Registry profile

```
HKCU\Software\Classes\Local Settings\Software\OpenVR-SpaceCalibrator\Config
```

Export/import examples: [profiles/README.md](../profiles/README.md).