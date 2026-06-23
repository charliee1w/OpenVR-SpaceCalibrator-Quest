# contcal fork changelog

## 1.5.1-gore-contcal9 (2026-06-22)

Long-session robustness — diverged recovery, VDXR path warning, log tooling.

| Change | Problem addressed | File(s) |
|--------|-------------------|---------|
| **Diverged-state recovery** | Applied offset frozen while `error_currentCal` climbed (69 min session tail) | `CalibrationCalc.cpp`, `CalibrationCalc.h` |
| **Lock-rel reject annotations** | Silent lock-relative rejects hard to diagnose in logs | `CalibrationCalc.cpp` |
| **VDXR / SteamVR path warning** | VD OpenXR runtime bypasses SteamVR; partial cal apply | `Calibration.cpp`, `UserInterface.cpp` |
| **Analyzer stuck-tail detection** | P4 pass missed frozen-offset tails | `scripts/analyze-spacecal-log.ps1` |
| **Housekeeping scripts** | Docs referenced scripts missing from tree | `apply-p4-winner.ps1`, `run-p4-validation.ps1`, `run-head-ab-session.ps1`, `compare-head-ab.ps1` |

Rebuild overlay + driver; `deploy.ps1` with SteamVR stopped. Validate diverged recovery in a 60+ min VR session before declaring P4 tail pass.

## 1.5.1-gore-contcal8 (2026-06-21)

Stability release — VR usability, tracking fidelity, log tooling.

| Change | Problem addressed | File(s) |
|--------|-------------------|---------|
| **SeedFromProfile** | Continuous cal jumped trackers sideways on start | `CalibrationCalc.*`, `Calibration.cpp`, `CalibrationChain.cpp` |
| **Catastrophic regression guard** | ~100 mm error spikes on bad rel-pose frames | `CalibrationCalc.cpp` |
| **ReferenceJitter NaN fix** | Half of session logs showed `-nan(ind)` jitterRef | `CalibrationCalc.cpp` |
| **VR UI fixes** | Calibration modal stuck; laser blocked by decorative widgets | `UserInterface.cpp`, `imgui_extensions.cpp` |
| **ImGui assert tooltip** | Red "enable asserts" box in VR dashboard | `SpaceCalibrator.cpp`, `CalibrationDebug.cpp` |
| **Full tracking apply path** | Perf shortcuts reverted — no debounced save or skipped applies | `Calibration.cpp` |
| **Adaptive dashboard FPS** | 30 FPS idle / 90 FPS on interaction (overlay only) | `SpaceCalibrator.cpp` |
| **Log analyzer** | `analyze-spacecal-log.ps1` crashed on `-nan(ind)` | `scripts/analyze-spacecal-log.ps1` |

No driver protocol changes. Rebuild overlay after pull; `deploy.ps1` with SteamVR stopped.

## 1.5.1-gore-contcal7 (2026-06-20)

UI polish — industrial VR theme and reorganized overlay.

| Change | Problem addressed | File(s) |
|--------|-------------------|---------|
| **Dark cockpit theme** | Hard to read status in VR overlay | `ui_theme.cpp`, `imgui_extensions.cpp` |
| **Device status cards** | Reference/target state buried in menus | `UserInterface.cpp` |
| **Live metrics on Status tab** | P4 tuning required external log analysis | `UserInterface.cpp`, `CalibrationMetrics.h` |
| **Collapsible Quest/SLAM settings** | SLAM tuning sliders cluttered main view | `UserInterface.cpp` |
| **Deploy hardening** | `deploy.ps1` omitted `driver.vrdrivermanifest` | `scripts/deploy.ps1`, `scripts/validate-install.ps1` |

No IPC or driver protocol changes — overlay-only release atop contcal6 calibration logic.

## 1.5.1-gore-contcal6 (2026-06-20)

P5 adaptive polish — completes remaining roadmap items from contcal5.

| Change | Problem addressed | File(s) |
|--------|-------------------|---------|
| **Per-axis alignment speed** | Yaw corrections too aggressive vs translation on SLAM refs | `Protocol.h`, `IsometryTransform.h`, `ServerTrackedDeviceProvider.cpp`, `Calibration.h` |
| **Exposed SLAM tuning** | Spike/guardian thresholds only in code | `Configuration.cpp`, `UserInterface.cpp`, profile JSON |
| **Hardware presets** | No starting point for Tundra vs Vive 3.0 head | `profiles/preset-quest-*.json`, `apply-hardware-preset.ps1` |

SLAM preset sets `align_rot_speed_scale` 0.45 (slow yaw, faster translation).

## 1.5.1-gore-contcal5 (2026-06-20)

Research-driven algorithm fixes for Quest/VD SLAM → lighthouse cont-cal.

| Change | Problem addressed | File(s) |
|--------|-------------------|---------|
| **Lock-relative gate** | `lockRelativePosition` applied FULL correction every tick | `CalibrationCalc.cpp` |
| **Adaptive spike threshold** | Fixed 50 mm spike gate too tight/loose vs live `jitterRef` | `Calibration.h`, `Calibration.cpp` |
| **VD guardian debounce** | 20 mm / 3° fired on VD micro-shifts | `GuardianDrift.cpp`, `Calibration.h` |
| **Bad-frame sample prune** | Issue #50 burst drift from dropout samples in deque | `CalibrationCalc.cpp`, `Calibration.cpp` |
| **Skip tick on bad VD frames** | Latency/skew/spike returned `true` without sample → stale compute | `Calibration.cpp` |
| **Degraded SLAM skip** | `willDriftInYaw` / high `poseTimeOffset` frames still computed | `Calibration.cpp` |
| **1 Hz live metrics** | P4 logs sparse after first FULL | `Calibration.cpp`, `CalibrationCalc.cpp` |
| **SLAM preset tuning** | `maxRelativeErrorThreshold` 0.008, guardian 35 mm / 5° / 3 confirms | `Calibration.h` |

## 1.5.1-gore-contcal4

P3 guardian drift, multi-chain, calibration chain UI.

## Earlier (contcal1–3)

SLAM preset, CollectSample fix, trustTargetYaw, spike rejection, frozen offset, IPC retry (overlay).