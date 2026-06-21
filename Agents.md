# Agent context — OpenVR-SpaceCalibrator-fork

Quest SLAM + lighthouse **continuous calibration** fork for wireless Quest FBT.

## Memory (mandatory first action)

**Recall first**: On every user turn, call `memory_recall` as your **first** tool
before shell commands, file reads, searches, edits, subagents, or response text.
Build a short query from the user's goal; prefer `n_results=3`. Use
`include_knowledge=true` when consolidated knowledge is needed.

MCP project namespace: **`space-calibrator`** (isolated from other repos).

**Store**: Proactively call `memory_store` when you:
- Solve a calibration, guardian, or deploy problem
- Find a winning P4/P5 parameter set
- Learn VD latency or SteamVR driver layout quirks
- Complete a meaningful feature or fix

Use tags: `calibration`, `guardian`, `quest-slam`, `p4-tuning`, `deploy`, `ipc`.

**Drift and finish**: After substantial C++ edits, call `memory_detect_drift`.
Before the final response, call `memory_recall` again for the active scope.

## Key docs

- [README.md](README.md)
- [docs/ROADMAP.md](docs/ROADMAP.md)
- [docs/P4_TUNING.md](docs/P4_TUNING.md)
- [docs/DEPLOYMENT.md](docs/DEPLOYMENT.md)

## Agent tooling in this repo

| Path | Purpose |
|------|---------|
| `.cursor/mcp.json` | Cursor MCP — `consolidation_memory` on project `space-calibrator` |
| `.cursor/hooks.json` | Cursor memory-recall gate |
| `.grok/config.toml` | Grok MCP (same server) |
| `.grok/hooks/require_memory_recall.py` | Shared recall-first hook |

Verify MCP: `C:/Users/gore/miniconda3/python.exe -m consolidation_memory --project space-calibrator test`

## Build / deploy

- CMake MSVC; output in `bin/`
- Deploy: `scripts/deploy.ps1` → SteamVR `drivers/01spacecalibrator`
- Version: `src/common/Version.h`