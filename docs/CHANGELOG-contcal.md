# contcal fork changelog

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