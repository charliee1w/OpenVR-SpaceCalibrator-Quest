# Phase 4 — Measurement & profile tuning

**Status:** ✅ **DONE** (2026-06-20) — optional 10+ min validation pending VR session  
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
| contcal7 deployed (driver + overlay) | ✅ use `deploy.ps1` + `validate-install.ps1` |
| 10+ min metrics log on contcal7 | ⬜ run `.\scripts\run-p4-validation.ps1` after VR session |

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

**Head tracker A/B:** Tundra vs Vive 3.0 — run `.\scripts\run-head-ab-session.ps1` (see below).

---

## Session log

| Session | Date | Build | Change | Median err | Duration | Notes |
|---------|------|-------|--------|------------|----------|-------|
| baseline | 2026-06-20 | contcal4 | defaults + saved cal | **8.1 mm** | short | User: “works really well” |
| S1a | 2026-06-20 | contcal4 | offset 0,0,0 | 16.7 mm* | 2.7 min cont-cal* | *Sparse metrics (5 rows); not a valid P4 duration run |
| winner | 2026-06-20 | contcal5 | P4 profile applied | — | — | `apply-p4-winner.ps1` + deploy |
| p4-validation | — | contcal7 | 10+ min session | — | — | **pending** — `run-p4-validation.ps1` |
| head-ab-tundra | — | contcal7 | Tundra head | — | — | **pending** — `run-head-ab-session.ps1` |
| head-ab-vive3 | — | contcal7 | Vive 3.0 head | — | — | **pending** — `run-head-ab-session.ps1` |

---

## Before each tuning session

1. SteamVR stopped → `.\scripts\deploy.ps1` → `.\scripts\validate-install.ps1`
2. `.\scripts\apply-p4-winner.ps1` if profile was reset
3. Restart SteamVR → head tracker first → cont-cal → body trackers
4. Metrics: **1 row/sec** while continuous cal runs (no debug checkbox)

---

## 10+ min validation (closes optional P4 criterion)

```powershell
.\scripts\run-p4-validation.ps1
```

Follow the prompts: ~10 minutes in VR with continuous cal active, then the script analyzes the latest log and reports PASS/FAIL.

**Pass:** ≥600 s continuous-cal segment, ≥~600 metric rows, median `error_byRelPose` < 15 mm.

**Analyze an existing log without prompts:**

```powershell
.\scripts\run-p4-validation.ps1 -AnalyzeOnly -LogPath "path\to\spacecal_log.*.txt"
```

---

## Head tracker A/B (Tundra vs Vive 3.0)

Same room, same VD settings, same strap — only swap the head tracker.

**Session 1 — Tundra:**

```powershell
.\scripts\run-head-ab-session.ps1 -HeadTracker tundra
```

**Session 2 — Vive 3.0** (swap tracker, restart SteamVR, recalibrate devices if needed):

```powershell
.\scripts\run-head-ab-session.ps1 -HeadTracker vive3
```

**Compare archived logs:**

```powershell
.\scripts\run-head-ab-session.ps1 -Compare
```

Logs are copied to `%LOCALAPPDATA%\..\LocalLow\SpaceCalibrator\ab-sessions\`.

**Manual compare** with explicit paths:

```powershell
.\scripts\compare-head-ab.ps1 -TundraLog "...\tundra-latest.txt" -Vive3Log "...\vive3-latest.txt"
```

Lower median `error_byRelPose` wins on paper; confirm subjective FBT feel in VRChat.

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