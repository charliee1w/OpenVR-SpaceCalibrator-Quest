# Publishing checklist

Use this before making the repository public on GitHub.

## Scrub personal data

- [x] No device serials in committed JSON (use `profiles/example-quest-lighthouse.json` only)
- [x] `docs/AGENT_HANDOFF.md` is gitignored (local operator notes)
- [x] `profiles/p4-*.json` gitignored
- [x] No absolute paths to user home directories in public docs

## Repository setup

1. Repo: https://github.com/charliee1w/OpenVR-SpaceCalibrator-Quest
2. Do **not** push to `hyblocker/OpenVR-SpaceCalibrator` — this is a fork.
3. `upstream` → hyblocker; `origin` → charliee1w fork (see remotes below).
4. Optional: add GitHub Topics: `steamvr`, `quest`, `virtual-desktop`, `full-body-tracking`, `lighthouse`, `openvr`.

```powershell
git remote set-url origin https://github.com/charliee1w/OpenVR-SpaceCalibrator-Quest.git
git push -u origin contcal5-release
git push origin v1.5.1-contcal5
```

## First GitHub release

1. Tag `v1.5.1-contcal5` on the release commit.
2. Attach build artifacts (optional):
   - `SpaceCalibrator.exe`
   - `driver_01spacecalibrator.dll`
   - `scripts/deploy.ps1`
3. Paste changelog from `docs/CHANGELOG-contcal.md`.

## Post-publish

- Link to hyblocker upstream and #50 in release notes.
- State clearly: **not** an official hyblocker/Valve/Meta product.