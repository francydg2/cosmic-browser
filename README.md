# cosmic-browser
A customizable Chromium-based desktop browser for Windows, Linux and Apple Silicon, built with C++20 and Qt 6. It includes configurable domain blocking, cookie controls, site permissions and local profiles.

## Download Cosmic

Visit the [Cosmic download site](https://francydg2.github.io/cosmic-browser/) for the latest installers and the [Privacy Policy](https://francydg2.github.io/cosmic-browser/privacy.html):

| Platform | Package |
| --- | --- |
| Windows x64 | Installer (`.exe`) |
| Linux x64 | AppImage (requires system NSS/NSPR libraries) |
| macOS Apple Silicon (arm64) | Disk image (`.dmg`) |

You can also find every version on the [GitHub Releases page](https://github.com/francydg2/cosmic-browser/releases/latest). In **Settings → Updates**, Cosmic can download a newer release and verify its GitHub SHA-256 digest before installing it. Linux updates the running x64 AppImage in place; Windows opens the installer with the existing installation directory; macOS replaces the Apple Silicon application in its current writable folder and keeps a backup in a hidden `.cosmic-update.*` folder. Other installation types, including Flatpak, must be updated by their package manager. Cosmic closes during installation; profile data is not replaced.

The Windows installer is unsigned and may trigger a SmartScreen warning. The macOS app is unsigned and not notarized, so Gatekeeper may warn when opening it.

## Page tools

The page tools menu offers tab search, an independent split view, a simplified text reader, page translation and QR sharing. The split view uses the current profile; its address is restored as a normal tab when the session is restored. The reader extracts text and may not work well on every page.

Saved workspaces in the same menu let you name and reopen a snapshot of the current tabs and groups. Opening one adds its tabs without closing existing ones. Workspaces are stored locally per profile and contain page addresses; split-view pages reopen as normal tabs.

Translation opens Google Translate after confirmation and sends Google the page address, including query parameters; pages requiring a login may not be accessible. QR codes are generated locally and also contain the address and query parameters.

## Flatpak

The Flatpak manifest and AppStream metadata are included for local testing. Cosmic is not yet published on Flathub. To build and install it locally (with Flatpak Builder and the Flathub remote configured):

```sh
flatpak-builder --user --install --force-clean build-dir io.github.francydg2.cosmic.json
flatpak run io.github.francydg2.cosmic
```

## Building

The GitHub Actions workflow creates packages for all three platforms. Pushing a version tag that matches `project(VERSION ...)` in `CMakeLists.txt` (for example, `v0.1.2`) publishes the installers on a GitHub Release. Pull requests, branch pushes and manual workflow runs publish packages as temporary Actions artifacts.

The GitHub Pages site is deployed from `docs/` by `.github/workflows/pages.yml`.
