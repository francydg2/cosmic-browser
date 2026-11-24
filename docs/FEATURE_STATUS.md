# Feature status — 0.2.0 work

Status of the eight requested features, checked against the code and the tests.
Legend: IMPLEMENTED, PARTIALLY IMPLEMENTED, NOT IMPLEMENTED, NEEDS VERIFICATION (written but not run on that platform or device).

## Existing features checked first

| Feature | Status | Notes |
| --- | --- | --- |
| Workspaces (desktop) | IMPLEMENTED | Unchanged; the Tab Manager and Focus open it through the existing code. |
| Updater (desktop, Android) | IMPLEMENTED | Unchanged. Android updater still NEEDS VERIFICATION on a device. |
| Split view | BROKEN (known) | Fullscreen video inside the split panel crashed Cosmic on Linux/NVIDIA in 0.1.3. Not fixed. |
| Android downloads and notifications | PARTIALLY IMPLEMENTED → improved | Banner, history, speed, cancel, filters, system notification (Android 13+ asks once). Not verified on a device. |
| RAM Saver | IMPLEMENTED | Fixed: discarding a tab no longer shows the crash page. |

## New features

| Feature | Desktop | Android | Notes |
| --- | --- | --- | --- |
| Cosmic AI Assistant | IMPLEMENTED | NOT IMPLEMENTED | OpenAI, Anthropic, Ollama, OpenAI-compatible. Streaming, history, consent, key in vault. Tested against a fake local server; **not tested against the real providers** (no keys here). |
| Smart Tab Manager | PARTIALLY IMPLEMENTED | NOT IMPLEMENTED | Manual groups, suggestions, duplicates, search, inactive suspension, workspaces link. Missing: tab previews (engine limitation). |
| Universal Translator | PARTIALLY IMPLEMENTED | NOT IMPLEMENTED (Google Translate hand-off exists) | In-page translation needs an AI provider; falls back to Google Translate. Single detection heuristic, 14 languages. Pages that rewrite their text nodes after translation can undo it. |
| Download Center | IMPLEMENTED | PARTIALLY IMPLEMENTED | Desktop: speed, remaining time, pause/resume/cancel, search, filters, persistent history, notifications. Android: no pause/resume (a cancelled or failed download can be restarted) and downloads stop when the app closes. |
| Privacy Dashboard | IMPLEMENTED | NOT IMPLEMENTED | Statistics are real (blocking by host list only). No history to clear. Camera and microphone share one permission in WebEngine. |
| Performance Center | PARTIALLY IMPLEMENTED | NOT IMPLEMENTED | Linux measured and tested. macOS uses `ps`: NEEDS VERIFICATION. Windows: unsupported (shown as such). Per-tab memory needs Qt ≥ 6.8 (CI uses 6.7.3). |
| Web Inspector | IMPLEMENTED | NOT IMPLEMENTED | Hosts the engine's own DevTools (console, errors, network, elements, sources). Not a custom network inspector. Android WebView cannot host DevTools. |
| Cosmic Focus | IMPLEMENTED | NOT IMPLEMENTED | Profiles, timer, limited sites (main frame only), hidden chrome, workspace, End button. Does not survive restart. |

## Verification

* Desktop (Linux, Qt 6.12 locally): 21 CTest suites pass, including `tools` (integration, real
  `BrowserWindow`) and unit suites for each pure module.
* Android: `testDebugUnitTest`, `lintDebug` and `assembleDebug` pass; **nothing was run on a device**.
* Windows and macOS: not built or run for this work. The code avoids platform APIs except where
  guarded (`ProcessStats`), but CI has not run on this branch.

## Known limitations

See also the per-feature notes above and `docs/ARCHITECTURE_0.2.0.md`.
