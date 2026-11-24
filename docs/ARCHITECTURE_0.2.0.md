# Architecture additions in 0.2.0

All additions follow the existing layering (`core` → `web` → `browser` → `ui`). New
logic is written as pure `core/` modules with unit tests; `ui/` classes only present it.
`BrowserWindow.cpp` was not extended: window-level wiring for the new tools lives in
`ui/BrowserWindowTools.cpp` (same class, separate translation unit), and each tool is its
own dialog/panel class.

```
core/   AiProvider  AiPrompt  AiConversations  Translator  TabAnalysis
        DownloadHistory  ProcessStats  Focus            (pure, no WebEngine)
web/    PagePolicy (+ per-site permissions, Focus rule)  BlocklistInterceptor (+ cumulative stats)
        WebPage (enforces Focus on main-frame navigation, per-site permissions)
browser/ AiClient  AiService  PerformanceMonitor  FocusController  DownloadManager (+ history)
ui/     AiSidebar AiSettingsDialog PageTranslator TabManagerDialog PrivacyDashboardDialog
        PerformanceDialog FocusDialog FocusPill Toast DownloadsDialog BrowserWindowTools.cpp
```

## Shared building blocks

* **Settings sections** (`Settings::section/setSection`): `ai`, `translation`, `focus`,
  `performance`, `sitePermissions`. Each feature validates its own JSON; unknown sections are
  ignored and a corrupt file still falls back to defaults.
* **Toast** (`ui/Toast`): non-modal notification used by downloads, performance warnings,
  translation and Focus.
* **Vault for secrets**: AI keys are `VaultEntry` records of type `ai-key`
  (site `cosmic-ai:<provider>`). They are excluded from autofill and from the vault list in
  Settings. If the vault is missing or locked no key can be stored; `COSMIC_AI_<PROVIDER>_KEY`
  is the alternative.

## Cosmic AI Assistant

`Ai::buildRequest` turns provider settings and messages into an HTTP request (OpenAI and
compatible: `/chat/completions` with Bearer; Anthropic: `/messages` with `x-api-key`; Ollama:
`/api/chat`). `Ai::StreamParser` decodes SSE or NDJSON incrementally. `AiClient` streams with
`QNetworkAccessManager`, restarts an inactivity timeout on each chunk, never follows
redirects (they could carry the key to another host). The key is held only in memory for the
request and is never logged; 401/403/429 replies are replaced by fixed messages so a provider
cannot echo the key back into the UI.
Remote endpoints must be HTTPS; loopback endpoints may use HTTP and never need consent.
`AiSidebar` builds each turn with `Ai::buildMessages`: page text is quoted inside `<page>` tags
with a system rule to treat it as untrusted data, addresses lose credentials/query/fragment,
and titles that Chromium derived from the address are cleaned the same way. Page context is
never stored in conversations or replayed on later turns.

## Universal Translator

Qt WebEngine has no translation API, so translation is done by the configured AI provider.
`PageTranslator` collects visible text nodes (skipping script, style, code, inputs and
`translate="no"`), sends them in batches as a JSON array and writes the reply back into the
same nodes; the originals stay in the page (`window.__cosmicTr`) so **Show original** restores
them exactly. Navigating away ends the state. Without a usable provider, “Translate page”
keeps the previous behaviour (Google Translate in a new tab after confirmation).
`Translator::detectLanguage` is a heuristic (Unicode scripts + stop words) used only to avoid
translating a page into its own language.

## Smart Tab Manager

`TabAnalysis` works on plain `TabInfo` values. Duplicates compare normalized addresses
(scheme-insensitive, no `www.`, no tracking parameters). Suggestions group free, unpinned tabs
by registrable domain approximation. Inactivity uses `TabContent::lastActiveMs()` (touched when
a tab is shown). Suspending uses `QWebEnginePage::setLifecycleState(Discarded)`; a discarded
page's normal renderer exit no longer shows the crash page.

## Download Center

`DownloadManager` assigns each request a stable id, samples speed (`SpeedTracker`), and on
completion/cancel/failure writes a `DownloadRecord` to `DownloadHistory`. `DownloadsDialog` merges
live requests and saved records (live ones win), filters them and refreshes immediately when rows
change and at most every 150 ms for progress. Android has its own downloader
(`DownloadCenter.kt`) that now shares the same ideas (history file, speed tracker, filters) but not
the code.

## Privacy Dashboard

Statistics come from `BlocklistInterceptor` (counts by site, domain and list category; total
requests are not counted, so no “percentage blocked” is shown). Cookies are read from every
profile's `QWebEngineCookieStore`. Per-site permission decisions live in `PagePolicy::sites`
(synchronised with the `sitePermissions` settings section); `WebPage` consults them before the
global ask/allow/block policy.

## Performance Center

`ProcessStats` reads `/proc` on Linux (CPU from tick deltas, so the first sample has no CPU)
and `ps` on macOS (not verified in CI); other systems report “unsupported” instead of guessing.
`PerformanceMonitor` samples every 30 s only to compare total memory with the warning threshold
(default 3 GB, warns at most every 10 min) and only while the warning is enabled; the dialog
samples every 2 s while visible. Per-tab memory uses `renderProcessPid()` (Qt ≥ 6.8).

## Web Inspector

A bottom dock hosts a `QWebEngineView` whose page is the DevTools page of the current tab. The
engine cannot detach a DevTools page, so rebinding creates a new DevTools page and destroys the
old one. It follows tab switches and navigation between page modes.

## Cosmic Focus

`FocusController` is application-wide. While active it sets `PagePolicy::focusActive` and the
blocked hosts; `WebPage::acceptNavigationRequest` refuses main-frame navigation to those hosts and
shows an explanatory page. Windows hide their tab bar, group strip, status bar, side panels and
split panel (restored exactly afterwards) and show `FocusPill`. A session does not survive
restarting Cosmic.
