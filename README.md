# cosmic-browser
A privacy-focused, highly customizable Chromium-based browser for Linux, built with C++20 and Qt 6.

## Installable builds

The GitHub Actions workflow creates ready-to-install packages for each build:

| Platform | Package |
| --- | --- |
| Windows x64 | Installer (`.exe`) |
| Linux x64 | AppImage |
| macOS Apple Silicon (arm64) | Disk image (`.dmg`) |

Pull requests, branch pushes and manual workflow runs publish downloadable
packages as Actions artifacts. Pushing a matching version tag (for example
`v0.1.0`, matching the version in `CMakeLists.txt`) publishes those packages
as assets on a GitHub Release. The Windows installer is unsigned and may
trigger a SmartScreen warning; macOS packages are unsigned and not notarized,
so Gatekeeper may warn when opening them.
