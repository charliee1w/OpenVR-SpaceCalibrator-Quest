# Generalization experiment (`experiment/generalization`)

**Goal:** Make this fork the obvious default for **SLAM reference + lighthouse FBT** users — not tied to one room, one headset serial, or one maintainer name.

**Branch:** `experiment/generalization` (worktree: `.worktrees/experiment-generalization`)

---

## Layers

| Layer | Status in branch | Notes |
|-------|------------------|-------|
| **Algorithm** | ✅ on main (contcal9) | `IsSlamTrackingSystem`, auto SLAM preset, diverged recovery |
| **Device preset dropdown (UI)** | 🧪 this branch | In-overlay selector; persists `device_preset` in profile |
| **Starter profiles** | 🧪 this branch | VD / ALVR / Link latency tiers in JSON + scripts |
| **Install/import scripts** | 🧪 this branch | `install.ps1`, `import-profile.ps1`, `list-starter-profiles.ps1` |
| **Release packaging** | 🧪 this branch | Zip includes scripts + profiles + QUICKSTART |
| **Neutral versioning** | 🧪 this branch | `1.5.1-slam-exp1` (drop `gore-` prefix) |
| **Self-tuning / upstream** | ⬜ future | Adaptive thresholds; hyblocker PR subset |

---

## Starter profile matrix

| Preset | Reference | Streaming path | `max_pose_time_skew` | `max_reference_pose_time_offset` |
|--------|-----------|----------------|----------------------|----------------------------------|
| `quest-vd` | `oculus` | Virtual Desktop WiFi | 0.08 | 0.04 |
| `pico-alvr` | `pico` | ALVR | 0.06 | 0.03 |
| `quest-link` | `oculus` | Wired Link | 0.05 | 0.02 |
| `p4-winner` | `oculus` | Tuned winner (merge) | from live/code | from live/code |

Fresh imports use placeholder serials — users **must** calibrate and save in VR.

---

## Merge vs fresh import

- **Fresh** (`import-profile.ps1 -Preset quest-vd`): new registry profile, zero room transform.
- **Merge** (`-Merge` or `apply-p4-winner.ps1`): tuning sliders only; keeps devices + room cal + chaperone.

---

## Promotion criteria (merge to `main`)

1. At least one stranger-equivalent test: fresh install script + starter preset + 10 min session log.
2. `validate-install.ps1` + `validate-cal-invariants.ps1` pass.
3. README / QUICKSTART reviewed without maintainer-specific paths.
4. Version string agreed (`slam-contcal10` vs keep `gore-`).

---

## Out of scope (same architecture ceiling)

- VDXR-only play without SteamVR
- Quest FBT without lighthouse head tracker
- Sub-mm tracking vs VD + SLAM physics