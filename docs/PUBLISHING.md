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
git push -u origin main
git push origin v1.5.1-contcal7
```

## GitHub release (contcal7)

1. Build Release, then package:

```powershell
cmake --build bin --config Release
.\scripts\package-release.ps1
```

2. Tag and publish:

```powershell
git tag -a v1.5.1-contcal7 -m "1.5.1-gore-contcal7 — UI polish + deploy hardening"
git push origin v1.5.1-contcal7
gh release create v1.5.1-contcal7 bin/release/OpenVR-SpaceCalibrator-1.5.1-contcal7.zip `
  --title "1.5.1-gore-contcal7" --notes-file docs/CHANGELOG-contcal.md
```

3. Zip contains: `SpaceCalibrator.exe`, `driver_01spacecalibrator.dll`, `driver.vrdrivermanifest`, `deploy.ps1`, `validate-install.ps1`.

## Post-publish

- Link to hyblocker upstream and #50 in release notes.
- State clearly: **not** an official hyblocker/Valve/Meta product.