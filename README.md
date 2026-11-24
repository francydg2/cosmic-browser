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

## Cosmic 0.2.0 tools

The page tools (pencil) menu also opens these desktop tools. Android gets the improved Download Center; the other tools are not available there yet (see [`docs/FEATURE_STATUS.md`](docs/FEATURE_STATUS.md)).

* **Cosmic AI Assistant** (Ctrl+Shift+Y) — streaming chat in a side panel with saved conversations. Providers: OpenAI, Anthropic, Ollama and OpenAI-compatible endpoints. Ask about the current page, the selected text or several tabs; page text is attached only when you ask and, for remote providers, only after you confirm. API keys are stored in the encrypted password vault (or read from `COSMIC_AI_<PROVIDER>_KEY`), never in the settings file.
* **Smart Tab Manager** (Ctrl+Shift+M) — search, duplicate detection, group suggestions by site, group rename/recolor, suspending inactive tabs, link to workspaces. Tab thumbnails are not available: Qt WebEngine cannot capture background tabs.
* **Universal Translator** — in-page translation with the configured AI provider: text nodes are replaced in place (layout and scripts stay intact) and **Show original** restores them. Per-site “always translate” rules, selected-text translation, language detection. Without an AI provider the page tools fall back to Google Translate in a new tab.
* **Download Center** (Ctrl+J) — live and saved downloads with progress, speed, time left, pause/resume/cancel, search, filters and a persistent history; completion notifications.
* **Privacy Dashboard** — real blocking statistics per site, tracker domain and category; protection switches; cookies by site; remembered per-site camera/microphone/location/notification decisions; data clearing. Cosmic has no browsing-history database, so there is nothing to clear there.
* **Performance Center** — measured memory and CPU of Cosmic's processes (Linux and, unverified, macOS), tab states, suspend actions, RAM Saver and a memory warning threshold. Per-tab memory needs Qt 6.8 or newer.
* **Web Inspector** (F12 or Ctrl+Shift+I) — the engine's Chromium DevTools (Console, Elements, Network, Sources…) docked at the bottom and following the current tab.
* **Cosmic Focus** — Study, Work and Custom profiles with a timer, a list of limited sites (main-frame navigation only), hidden extra interface and an optional workspace. Nothing is closed or deleted and the **End** button is always visible.

## Flatpak

The Flatpak manifest and AppStream metadata are included for local testing. Cosmic is not yet published on Flathub. To build and install it locally (with Flatpak Builder and the Flathub remote configured):

```sh
flatpak-builder --user --install --force-clean build-dir io.github.francydg2.cosmic.json
flatpak run io.github.francydg2.cosmic
```

## Building

The GitHub Actions workflow creates packages for all three platforms. Pushing a version tag that matches `project(VERSION ...)` in `CMakeLists.txt` (for example, `v0.2.0`) publishes the installers on a GitHub Release. Pull requests, branch pushes and manual workflow runs publish packages as temporary Actions artifacts.

The GitHub Pages site is deployed from `docs/` by `.github/workflows/pages.yml`.
