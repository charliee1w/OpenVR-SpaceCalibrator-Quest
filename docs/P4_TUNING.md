# Phase 4 — Measurement & profile tuning

**Started:** 2026-06-20  
**Example profile:** [`profiles/example-quest-lighthouse.json`](../profiles/example-quest-lighthouse.json)
**Baseline log:** `spacecal_log.2026-06-20T20-53-47.txt` — 8.1 mm `error_byRelPose`, 1 sample (short session)

---

## Goal

Turn subjective “works really well” into a **reproducible profile** backed by 10+ minute metrics logs.

**P4 pass criteria:**

- Session duration ≥ 10 minutes of continuous calibration
- Median `error_byRelPose` < 15 mm
- User subjective: FBT feels locked, no visible foot slide

---

## Before each session

1. Confirm deploy: `1.5.1-gore-contcal5` (1 Hz metrics + algorithm fixes)
2. Start SteamVR → only head tracker on first → calibrate → turn on body trackers
3. Continuous calibration starts → metrics log auto-opens; **1 row/sec** while cont-cal runs (live pose snapshot)
4. Optional: click **Debug: Mark logs** at session start for a timestamp anchor

---

## Session protocol

| Phase | Duration | Action |
|-------|----------|--------|
| Warmup | 30 s | Stand still, let cont-cal settle |
| Guardian walk | 2 min | Walk perimeter, sit/stand once |
| FBT play | 10+ min | Normal VRChat movement — crouch, turn, dance |
| Cooldown | 30 s | Stand still, end session |

**Rule:** Change **one** slider per session. Save profile before exiting VR.

---

## Tuning queue (priority order)

| # | Parameter | Baseline | UI location | Why |
|---|-----------|----------|-------------|-----|
| **S1** | `continuous_calibration_target_offset` XYZ | 0, 0, 0 | Tracker offset sliders | Head tracker mount ≠ Quest head center; biggest lever for lean/slide |
| S2 | `continuousCalibrationThreshold` | 1.5 | Recalibration threshold | How aggressively solver applies corrections |
| S3 | `jitterThreshold` | 0.15 | Jitter threshold | SLAM noise gate (preset default) |
| S4 | `maxRelativeErrorThreshold` | 0.005 | Max relative error | Outlier rejection sensitivity |

### S1 — Tracker offset (first experiment)

The Tundra is mounted on the Quest Pro strap. Offset XYZ maps tracker pose → head center.

**Procedure:**

1. Run baseline session S1a with offsets at 0,0,0 (confirm metrics)
2. If feet slide forward/back in VRChat → adjust **Z** in ±5 mm steps
3. If body leans left/right → adjust **X**
4. If height feels wrong (crouch mismatch) → adjust **Y**
5. Re-run 10 min session after each change

**Save winning offsets** to `profiles/p4-s1-offset-<description>.json`.

---

## Analyze a session

```powershell
# Latest log
.\scripts\analyze-spacecal-log.ps1 -Latest

# Specific log
.\scripts\analyze-spacecal-log.ps1 -LogPath "$env:LOCALAPPDATA\..\LocalLow\SpaceCalibrator\Logs\spacecal_log.YYYY-MM-DDTHH-mm-ss.txt"
```

**Key columns:**

| Column | Meaning |
|--------|---------|
| `error_byRelPose` | Primary P4 metric (mm-scale; values are meters in CSV) |
| `error_currentCal` | Active correction magnitude (want ~0 when holding still) |
| `jitterRef` | SLAM reference jitter (Quest); typical ~40–50 |
| `calibrationApplied` | `FULL` or `STATIC` when solver fired |
| `# GuardianDrift` | Guardian shift events (want few) |

---

## Session log

| Session | Date | Change | Duration | Median err | p95 err | Guardian | Notes |
|---------|------|--------|----------|------------|---------|----------|-------|
| baseline | 2026-06-20 | contcal4 defaults | ~1 s | 8.1 mm | — | 0 | User: “works really well” |
| S1a | 2026-06-20 | offset 0,0,0 | ~19 min* | — | — | — | *Overlay IPC 14:57–15:16; **no metrics log** — cont-cal likely not started. Autostart enabled for next run. |

---

## Profile management

**Live profile (registry):**

```
HKCU\Software\Classes\Local Settings\Software\OpenVR-SpaceCalibrator\Config
```

**Export:**

```powershell
(Get-ItemProperty 'HKCU:\Software\Classes\Local Settings\Software\OpenVR-SpaceCalibrator').Config |
  Out-File 'profiles\p4-export.json' -Encoding utf8
```

**Import (SteamVR stopped):**

```powershell
$json = Get-Content 'profiles\p4-baseline-2026-06-20.json' -Raw
Set-ItemProperty 'HKCU:\Software\Classes\Local Settings\Software\OpenVR-SpaceCalibrator' -Name Config -Value $json
```

---

## After P4

When a session passes exit criteria:

1. Copy winning profile → `profiles/p4-winner.json`
2. Update `ROADMAP.md` Phase 4 → ✅
3. Begin Phase 5 (contcal5 adaptive polish) or optional SpaceOverride A/B