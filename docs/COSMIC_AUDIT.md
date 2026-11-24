# Cosmic audit — base 0.1.3

Audit of the repository at commit `fe893ab` (branch `francydg2-daily-usability`,
merged with `main`). Everything below was verified in code, not inferred from the UI.

## Stack

| Area | Finding |
| --- | --- |
| Desktop | C++20, Qt 6 Widgets + Qt WebEngine (Chromium). CMake, static library `cosmic_core` + executable `cosmic`. CI builds Qt **6.7.3** on Linux, Windows and macOS arm64. |
| Android | Separate Kotlin / Jetpack Compose project in `android/`, Android System WebView, min SDK 26, target 35. No shared code with desktop. |
| Layering | `core/` (no WebEngine) → `web/` → `browser/` → `ui/` → `main.cpp` (see `ARCHITECTURE.md`). `browser/` never includes `ui/`. |
| Persistence | JSON files in the XDG dirs: `settings.json`, `session.json`, `users.json`, `bookmarks`, `notes.json`, `ui.json`, encrypted password vault, workspaces per user. No database. No browsing-history store on desktop. |
| Tests | CTest, 14 suites, offscreen. `test_browserwindow` drives a real `BrowserWindow`. Android: JVM unit tests (`testDebugUnitTest`) and lint. |
| Packaging | AppImage (Linux), Inno Setup (Windows), DMG (macOS arm64), Flatpak manifest, GitHub Pages site. |

## Existing components relevant to the 0.2.0 features

| Component | Where | Notes |
| --- | --- | --- |
| Tabs, pinning, groups (name, colour, collapse) | `BrowserWindow`, `TabWidget`, `TabContent`, `Session` | Window-scoped groups; rename/colour already implemented. |
| Tab search | `BrowserWindow::showTabSearch` | Cross-window search by title/address. |
| Workspaces | `BrowserWindow::showWorkspaces/openWorkspace` | Saved per user as `Session` files. Open adds tabs. |
| Split view | `BrowserWindow::toggleSplitView` | Known crash: fullscreen video inside the split panel (Linux/NVIDIA observed). Unfixed. |
| RAM Saver | `discardBackgroundTabs`, `Settings::ramSaverEnabled` | Uses `QWebEnginePage::setLifecycleState(Discarded)`. Reused by the Performance Center. |
| Downloads | `browser/DownloadManager`, `ui/DownloadsDialog` | Session-only list, user picks destination, pause/resume (2 automatic retries), open file/folder. **No persistence, no speed/ETA, no search/filters, no completion notification.** |
| Updater | `browser/UpdateAsset`, `UpdateDownload` | SHA-256 + size verified, per-platform replacement. Not touched by 0.2.0. |
| Tracker blocking | `web/BlocklistInterceptor`, `ui/TrackerRadarDialog` | Real request cancellation by host list; counters and 500 recent events. Domain-level only, no cosmetic filtering. |
| Permissions | `PagePolicy`, `BrowserWindow::handleFeaturePermissionAsk` | Global ask/allow/block for location, media, notifications. **No per-site memory.** |
| Reader / translate / QR | `showReader`, `translateCurrentPage`, `showPageQrCode` | Translation opens Google Translate in a new tab (consent dialog). Not in-page. |
| "AI" toolbar button | `buildAiMenu` | Only opens ChatGPT/Claude/Gemini/Perplexity/Copilot websites. Not an assistant. |
| Password vault | `core/PasswordVault` | AES-256-GCM, PBKDF2 600k, master password ≥ 16. Reusable for secrets. |
| Android downloads | `DownloadCenter.kt` | In-process HTTP downloader to MediaStore; session-only list, no speed/cancel/persistence. |
| Android privacy | `privacy/HostBlocklist`, `RequestProtection` | Host-list blocking, no counters surfaced. |

## Not present at all

* AI assistant, provider abstraction, conversation store.
* In-page translation, language detection, per-site translation preferences.
* DevTools integration (no `setDevToolsPage` use anywhere).
* Process/CPU/RAM measurements.
* Focus mode.
* Duplicate detection, group suggestions, inactivity tracking.

## Platform facts that constrain the design

* Qt WebEngine 6.7.3 (CI) has **no** `renderProcessPid()` (Qt ≥ 6.8) → per-tab memory
  needs a version guard; without it only per-process totals are available.
* Qt WebEngine exposes **no** translation API and **no** network-inspector API; Chromium
  DevTools is available through `QWebEnginePage::setDevToolsPage`.
* There is no browsing-history database on desktop, so "clear history" has nothing to clear.
* Android WebView has no DevTools embedding, no extensions and no per-tab memory API.
* Per-process CPU/RAM is read from `/proc` (Linux) and `ps` (macOS); Windows has no
  implementation in this release.

## Risks

1. `BrowserWindow.cpp` is 3000+ lines; new features go into separate classes and only
   wire themselves in through small hooks.
2. Windows/macOS cannot be built or run locally; platform-specific code is isolated and
   guarded, and documented as unverified where applicable.
3. API keys: only the encrypted vault (or an environment variable) may hold them.
4. Page text sent to an external AI provider needs explicit, per-send consent.
