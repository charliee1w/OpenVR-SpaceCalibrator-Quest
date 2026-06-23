# Quick start — SLAM HMD + lighthouse FBT

**Audience:** New users with Quest/Pico + Virtual Desktop or ALVR + lighthouse head/body trackers.

**Requirement:** Virtual Desktop must use **SteamVR** as the OpenXR runtime — not VDXR. Space Cal hooks SteamVR device poses; VDXR bypasses that path.

---

## 1. Install (from repo or release zip)

```powershell
# SteamVR must be stopped (install.ps1 stops ghosts via deploy.ps1)
.\scripts\install.ps1 -Preset quest-vd
```

| Flag | Meaning |
|------|---------|
| `-Preset quest-vd` | Quest + Virtual Desktop latency defaults (default) |
| `-Preset pico-alvr` | Pico + ALVR |
| `-Preset quest-link` | Quest wired Link (lower latency) |
| `-Preset none` | Deploy only, keep existing registry profile |
| `-MergeProfile` | Import tuning only; keep your room cal + device serials |
| `-SkipProfile` | Do not touch registry |

List presets:

```powershell
.\scripts\list-starter-profiles.ps1
```

---

## 2. First VR session

1. Restart **SteamVR**.
2. Power on **head tracker only** first.
3. Open **Space Calibrator** from the SteamVR dashboard.
4. Confirm **reference** = your HMD (`oculus` or `pico`), **target** = head tracker (`lighthouse`).
5. **Copy Chaperone** once.
6. Run calibration (one-shot or continuous). Enable:
   - Continuous calibration
   - Lock relative position
   - Autostart continuous calibration
7. **Save profile** (updates device serials from live hardware).
8. Turn on body trackers; play normally for 10+ minutes.

If the UI shows a **VDXR / tracking system warning**, fix VD runtime before tuning sliders.

---

## 3. Check that it worked

```powershell
.\scripts\run-p4-validation.ps1 -AnalyzeOnly
```

**Good session:** duration ≥ 10 min, `error_currentCal` median < 15 mm, last 5 min < 25 mm.

**Logs:** `%LOCALAPPDATA%\..\LocalLow\SpaceCalibrator\Logs\spacecal_log.*.txt`

---

## 4. Tuning

| Symptom | First lever |
|---------|-------------|
| Feet lean / height | Tracker offset XYZ in overlay |
| Drift after 30+ min | Confirm continuous cal + check `DivergedRecoveryApply` in log |
| “Feels like lag” | Often high `error_byRelPose` — check VD WiFi, not GPU |
| Guardian resets | Expected brief recal; see guardian annotations in log |

Winner profile (validated tuning): `.\scripts\import-profile.ps1 -Preset p4-winner -Merge`

Full guide: [P4_TUNING.md](P4_TUNING.md)

---

## 5. Who should use upstream SpaceCal instead

- PCVR Index / Vive with a single tracking universe
- No lighthouse head tracker bridge
- SpaceOverride-only setups (different integration model)

This fork targets **wireless inside-out HMD + lighthouse FBT** in SteamVR.