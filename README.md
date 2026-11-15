# cosmic-browser
A privacy-focused, highly customizable Chromium-based desktop browser for Windows, Linux and Apple Silicon, built with C++20 and Qt 6.

## Download Cosmic

Visit the [Cosmic download site](https://francydg2.github.io/cosmic-browser/) for the latest installers:

| Platform | Package |
| --- | --- |
| Windows x64 | Installer (`.exe`) |
| Linux x64 | AppImage |
| macOS Apple Silicon (arm64) | Disk image (`.dmg`) |

You can also find every version on the [GitHub Releases page](https://github.com/francydg2/cosmic-browser/releases/latest). Cosmic checks for updates from **Settings → Updates**; when a newer version is available, it opens the download page so you can select the package for your system.

The Windows installer is unsigned and may trigger a SmartScreen warning. The macOS app is unsigned and not notarized, so Gatekeeper may warn when opening it.

## Building

The GitHub Actions workflow creates packages for all three platforms. Pushing a version tag that matches `project(VERSION ...)` in `CMakeLists.txt` (for example, `v0.1.1`) publishes the installers on a GitHub Release. Pull requests, branch pushes and manual workflow runs publish packages as temporary Actions artifacts.

The GitHub Pages site is deployed from `docs/` by `.github/workflows/pages.yml`.
