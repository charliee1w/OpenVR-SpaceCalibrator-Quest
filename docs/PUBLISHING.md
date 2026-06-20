# Publishing checklist

Use this before making the repository public on GitHub.

## Scrub personal data

- [x] No device serials in committed JSON (use `profiles/example-quest-lighthouse.json` only)
- [x] `docs/AGENT_HANDOFF.md` is gitignored (local operator notes)
- [x] `profiles/p4-*.json` gitignored
- [x] No absolute paths to user home directories in public docs

## Repository setup

1. Create new GitHub repo (suggested name: `OpenVR-SpaceCalibrator-Quest` or `SpaceCal-contcal`).
2. Do **not** push to `hyblocker/OpenVR-SpaceCalibrator` — this is a fork.
3. Update `README.md` clone URL (`YOUR_USER` placeholder).
4. Optional: add GitHub Topics: `steamvr`, `quest`, `virtual-desktop`, `full-body-tracking`, `lighthouse`, `openvr`.

```powershell
git remote rename origin upstream   # keep hyblocker as upstream optional
git remote add origin https://github.com/YOUR_USER/YOUR_REPO.git
git checkout -b contcal5-release
git add README.md docs/ profiles/ scripts/ src/ NOTICE LICENSE .gitignore
git commit -m "Release contcal5: Quest SLAM continuous calibration fork"
git push -u origin contcal5-release
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