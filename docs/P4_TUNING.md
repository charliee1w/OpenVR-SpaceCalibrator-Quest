# Phase 4 — Measurement & profile tuning

**Status:** ✅ **DONE** (2026-06-20)  
**Winner template:** [`profiles/example-quest-lighthouse.json`](../profiles/example-quest-lighthouse.json)  
**Apply to live profile:** `.\scripts\apply-p4-winner.ps1` (SteamVR stopped or re-save in overlay)

---

## Goal

Turn subjective “works really well” into a **reproducible profile** with tuned sliders.

**P4 pass criteria:**

| Criterion | Result |
|-----------|--------|
| Winner slider profile documented | ✅ `example-quest-lighthouse.json` + `apply-p4-winner.ps1` |
| `error_byRelPose` median < 15 mm | ✅ 8.1 mm baseline (`spacecal_log.2026-06-20T20-53-47.txt`) |
| Subjective FBT locked | ✅ user-validated contcal4 session |
| contcal5 deployed (driver + overlay) | ✅ 2026-06-20 |
| 10+ min metrics log on contcal5 | ⬜ optional confirmation — prior logs were contcal4 / sparse 1 Hz |

---

## P4 winner settings (Quest Pro + VD + Tundra head)

| Parameter | Winner value | Notes |
|-----------|--------------|-------|
| `autostart_continuous_calibration` | **true** | Saved-only mode does not refine |
| `lock_relative_position` | **true** | Required for Quest (#50) |
| `jitter_threshold` | **0.15** | SLAM preset |
| `max_relative_error_threshold` | **0.008** | contcal5 SLAM preset |
| `continuousCalibrationThreshold` | **1.5** | Default — no change needed |
| `continuous_calibration_target_offset` XYZ | **0, 0, 0** | S1: baseline 8 mm — no mount offset tweak needed |
| `static_calibration` | true | |
| `quash_target_in_continuous` | true | |

**Not tuned (deferred):** Vive 3.0 head A/B, SpaceOverride A/B — optional P6 / ceiling work.

---

## Session log

| Session | Date | Build | Change | Median err | Duration | Notes |
|---------|------|-------|--------|------------|----------|-------|
| baseline | 2026-06-20 | contcal4 | defaults + saved cal | **8.1 mm** | short | User: “works really well” |
| S1a | 2026-06-20 | contcal4 | offset 0,0,0 | 16.7 mm* | 2.7 min cont-cal* | *Sparse metrics (5 rows); driver still contcal4 |
| **winner** | 2026-06-20 | **contcal5** | P4 profile applied | — | — | `apply-p4-winner.ps1` + deploy |

---

## Before each tuning session

1. Confirm deploy: driver log `1.5.1-gore-contcal5`
2. `.\scripts\apply-p4-winner.ps1` if profile was reset
3. Restart SteamVR → head tracker first → cont-cal → body trackers
4. Metrics: **1 row/sec** while continuous cal runs (no debug checkbox)

---

## Analyze a session

```powershell
.\scripts\analyze-spacecal-log.ps1 -Latest
```

**Key columns:**

| Column | Meaning |
|--------|---------|
| `error_byRelPose` | Primary metric (mm) |
| `error_currentCal` | Active correction (want ~0 at rest) |
| `jitterRef` | Quest SLAM jitter |
| `# GuardianDrift` | Guardian shift events |

---

## Profile management

**Live profile (registry):**

```
HKCU\Software\Classes\Local Settings\Software\OpenVR-SpaceCalibrator\Config
```

**Apply P4 winner (keeps your devices + room transform):**

```powershell
.\scripts\apply-p4-winner.ps1
```

**Export personal backup (local only — gitignored):**

```powershell
(Get-ItemProperty 'HKCU:\Software\Classes\Local Settings\Software\OpenVR-SpaceCalibrator').Config |
  Out-File 'profiles\p4-export.json' -Encoding utf8
```

---

## Optional post-P4 validation

One 10+ min VRChat session on **contcal5** with winner profile, then:

```powershell
.\scripts\analyze-spacecal-log.ps1 -Latest
```

Expect: ≥600 s continuous-cal segment, median `error_byRelPose` < 15 mm, few guardian events.