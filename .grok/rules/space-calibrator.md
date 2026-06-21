# OpenVR-SpaceCalibrator-fork (Grok project rules)

## Memory first

See [00-memory-first.md](00-memory-first.md). `memory_recall` is the mandatory first
tool on every user turn when `consolidation_memory` MCP is available.

## Context

Quest SLAM + lighthouse continuous calibration fork. Core loop lives in
`src/overlay/Calibration*.cpp` and `GuardianDrift.*`. Read [docs/ROADMAP.md](../../docs/ROADMAP.md)
before large changes.

## Workflow

- One focused slice per session (calibration gate, guardian debounce, UI preset, deploy).
- Bump `src/common/Version.h` when IPC or driver protocol changes.
- Store tuning wins and deploy gotchas via `memory_store` with tags (`p4-tuning`, `deploy`, etc.).
- Run `memory_detect_drift` after multi-file C++ refactors.

## Do not

- Commit personal `profiles/p4-*.json` or `docs/AGENT_HANDOFF.md` (gitignored).
- Change calibration defaults without noting rationale in memory or commit message.