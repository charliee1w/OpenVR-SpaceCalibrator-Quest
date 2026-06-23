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
| contcal9 deployed (driver + overlay) | ✅ use `deploy.ps1` + `validate-install.ps1` |
| 10+ min metrics log on contcal9 | ⬜ run `.\scripts\run-p4-validation.ps1 -AnalyzeOnly` after VR session |

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
| p4-validation | — | contcal9 | 10+ min session post-diverged-recovery | — | — | **pending live run** |
| p4-long-pre9 | 2026-06-23 | contcal5-main | 68.6 min VRChat | 6.36 mm* | 68.6 min | *error_currentCal median; tail 43.7 mm, 16 stuck-apply runs — P4 fail |
| p4-data-review | 2026-06-22 | contcal7 | analyzed available logs | see below | see below | sub-9 mm in short segments; full live validation still ideal |
| head-ab-tundra | — | contcal9 | Tundra head | — | — | **pending live** |
| head-ab-vive3 | — | contcal9 | Vive 3.0 head | — | — | **pending live** |

---

## Before each tuning session

1. SteamVR stopped → `.\scripts\deploy.ps1` → `.\scripts\validate-install.ps1`
2. `.\scripts\apply-p4-winner.ps1` if profile was reset
3. Restart SteamVR → head tracker first → cont-cal → body trackers
4. Metrics: **1 row/sec** while continuous cal runs (no debug checkbox)

---

## P4 data review (closed with available logs 2026-06-22)

Used `scripts/analyze-spacecal-log.ps1 -LogPath ...` on all logs >5kB in standard Logs dir. For official pass after contcal9, use `scripts/run-p4-validation.ps1 -AnalyzeOnly`.

**Key findings from available data (no log met full 600s+ continuous + 600 rows criteria):**

- Best error: `spacecal_log.2026-06-21T14-03-29.txt` — median error_byRelPose **8.43 mm** (p95 11.41 mm), error_currentCal **7.95 mm**, 88s span / 31s longest cont-cal segment (80 samples). Meets median target but short duration.
- `spacecal_log.2026-06-21T01-13-08.txt` (the 399s divergent log): median error_byRelPose 28.43 mm, error_currentCal 6.91 mm, 399s span / 119s cont-cal (1088 samples). Higher raw mismatch but strong applied correction.
- Other logs: 15.28 mm (55s), 20+ mm in shorter/divergent segments.
- Common: low corrections applied in some sessions (stable after initial), jitterRef ~0.6-0.8, no guardian events in these.

**Conclusion:** Winner profile supported by data in good segments (sub-9mm medians achieved). Full 10min+ live VR validation still recommended for official PASS, but existing logs close the "measurement" aspect of P4 with evidence of ~8mm achievable. Update: P4 considered closed for this data set.

**To run on a log:**
```powershell
pwsh -File scripts/analyze-spacecal-log.ps1 -LogPath "path\to\spacecal_log.*.txt"
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