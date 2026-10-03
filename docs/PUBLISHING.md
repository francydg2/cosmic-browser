# Publishing Cosmic (alpha)

This repo builds and tests itself on every push through
`.github/workflows/build.yml` (Ubuntu / Windows / macOS, Qt 6.8 +
WebEngine). No local packaging tools are needed: everything runs in
GitHub Actions with stock runners plus Qt's own deploy tools.

## One-time repository setup

1. Create the repository (suggested name: `cosmic`) and push this
   tree to `main`. No secrets are required.
2. Make sure **Actions** are enabled for the repo
   (Settings → Actions → Allow all actions). The workflow uses only
   `actions/checkout`, `jurplel/install-qt-action` and
   `actions/upload-artifact`.
3. The first green run on all three OSes is the alpha gate. Download
   each artifact from the run summary and smoke-test it (below)
   before announcing anything.

## Making an alpha release

1. Bump `VERSION` in the top-level `CMakeLists.txt`
   (`project(Cosmic VERSION x.y.z ...)`).
2. Commit and push. Wait for all three builds to pass.
3. Open the finished run → **Artifacts** → download
   `cosmic-alpha-linux`, `cosmic-alpha-windows`, `cosmic-alpha-macos`.
4. Smoke-test each one (see below).
5. GitHub → **Releases** → **Draft a new release** → tag `vX.Y.Z`
   → attach the three archives → publish. Keep the summary of
   `docs/ARCHITECTURE.md` honest: list what works and the known
   Qt WebEngine limits.

## Smoke-testing an artifact

- **Linux** (`cosmic-alpha-linux.tar.gz`): needs system Qt 6.6+
  with WebEngine (`qt6-base qt6-webengine` or distro equivalent).
  Extract anywhere and run `./bin/cosmic --version`, then
  `./bin/cosmic --quit-after 5000` (must start and exit cleanly).
- **Windows** (`cosmic-alpha-windows.zip`): extract and run
  `bin\cosmic.exe`. All Qt/WebEngine DLLs are bundled by
  `windeployqt` — no Qt install needed on the test machine.
- **macOS** (`cosmic-alpha-macos.zip`): extract, open `Cosmic.app`
  (unsigned alpha: right-click → Open on first launch). Frameworks
  are bundled by `macdeployqt`.

## Local packaging

`cpack -G TGZ` (or `ZIP`) in an existing `build/` directory packs
the install tree exactly as CI does. Nothing beyond CMake is
installed or required.

## Known CI risks (alpha)

- **Windows OpenSSL discovery**: the workflow installs OpenSSL via
  Chocolatey. If `find_package(OpenSSL)` fails there, point
  `OPENSSL_ROOT_DIR` at the Chocolatey OpenSSL dir in the workflow.
- **Headless WebEngine tests**: the suite runs offscreen
  (`QT_QPA_PLATFORM=offscreen`). If the live-network smoke test
  flakes on a runner, re-run the job; rented runners have working
  egress, no proxy config is needed.
- **Single instance in CI**: the app refuses second instances
  unless `COSMIC_ALLOW_MULTIPLE` is set — irrelevant for tests
  (they never exec the app binary).
