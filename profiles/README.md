# Profile examples

SpaceCal stores the active profile in the Windows registry:

```
HKCU\Software\Classes\Local Settings\Software\OpenVR-SpaceCalibrator\Config
```

The overlay **Save profile** button writes here automatically. Files in this folder are **reference examples** only.

| File | Preset id | Purpose |
|------|-----------|---------|
| [starter-quest-vd-lighthouse.json](starter-quest-vd-lighthouse.json) | `quest-vd` | Quest + Virtual Desktop (default for new users) |
| [starter-pico-alvr-lighthouse.json](starter-pico-alvr-lighthouse.json) | `pico-alvr` | Pico + ALVR |
| [starter-quest-link-lighthouse.json](starter-quest-link-lighthouse.json) | `quest-link` | Quest wired Link |
| [example-quest-lighthouse.json](example-quest-lighthouse.json) | `p4-winner` | Validated tuning — use with `-Merge` |

**Do not commit personal profiles** (serials, chaperone geometry, room offsets).

### Import (SteamVR stopped)

```powershell
.\scripts\list-starter-profiles.ps1
.\scripts\import-profile.ps1 -Preset quest-vd          # fresh install
.\scripts\import-profile.ps1 -Preset p4-winner -Merge  # tuning only, keep room cal
```

Full walkthrough: [docs/QUICKSTART.md](../docs/QUICKSTART.md)