# Profile examples

SpaceCal stores the active profile in the Windows registry:

```
HKCU\Software\Classes\Local Settings\Software\OpenVR-SpaceCalibrator\Config
```

The overlay **Save profile** button writes here automatically. Files in this folder are **reference examples** only.

| File | Purpose |
|------|---------|
| [example-quest-lighthouse.json](example-quest-lighthouse.json) | **P4 winner** template — Quest → lighthouse head tracker |

**Apply P4 winner to your saved calibration (keeps devices + room transform):**

```powershell
.\scripts\apply-p4-winner.ps1
```

**Do not commit personal profiles** (serials, chaperone geometry, calibration offsets from your room).

### Import example (SteamVR stopped)

```powershell
$json = Get-Content "profiles\example-quest-lighthouse.json" -Raw
Set-ItemProperty 'HKCU:\Software\Classes\Local Settings\Software\OpenVR-SpaceCalibrator' -Name Config -Value $json
```

Replace `YOUR_QUEST_SERIAL` and `LHR-XXXXXXXX` after exporting your devices from the overlay, or calibrate fresh in VR.