# Profile examples

SpaceCal stores the active profile in the Windows registry:

```
HKCU\Software\Classes\Local Settings\Software\OpenVR-SpaceCalibrator\Config
```

The overlay **Save profile** button writes here automatically. Files in this folder are **reference examples** only.

| File | Purpose |
|------|---------|
| [example-quest-lighthouse.json](example-quest-lighthouse.json) | **P4/P5 winner** — Quest → lighthouse head (Tundra-class) |
| [preset-quest-tundra-head.json](preset-quest-tundra-head.json) | Hardware preset — Tundra on strap |
| [preset-quest-vive3-head.json](preset-quest-vive3-head.json) | Hardware preset — Vive 3.0 on strap |

**Apply P4 winner to your saved calibration (keeps devices + room transform):**

```powershell
.\scripts\apply-p4-winner.ps1
.\scripts\apply-hardware-preset.ps1 -HeadTracker tundra   # or vive3
```

**Head tracker A/B (Tundra vs Vive 3.0):**

```powershell
.\scripts\run-head-ab-session.ps1 -HeadTracker tundra
.\scripts\run-head-ab-session.ps1 -HeadTracker vive3
.\scripts\run-head-ab-session.ps1 -Compare
```

See [docs/P4_TUNING.md](../docs/P4_TUNING.md).

**Do not commit personal profiles** (serials, chaperone geometry, calibration offsets from your room).

### Import example (SteamVR stopped)

```powershell
$json = Get-Content "profiles\example-quest-lighthouse.json" -Raw
Set-ItemProperty 'HKCU:\Software\Classes\Local Settings\Software\OpenVR-SpaceCalibrator' -Name Config -Value $json
```

Replace `YOUR_QUEST_SERIAL` and `LHR-XXXXXXXX` after exporting your devices from the overlay, or calibrate fresh in VR.