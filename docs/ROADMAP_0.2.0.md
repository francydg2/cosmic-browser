# Roadmap 0.2.0

Order follows dependencies: shared building blocks first, then the features that use them.

| # | Work item | Depends on | Notes |
| --- | --- | --- | --- |
| 1 | Toast notifications (`ui/Toast`) | — | Used by downloads, performance warnings and Focus. |
| 2 | Download Center (desktop + Android) | 1 | Extends `DownloadManager`/`DownloadsDialog` and `DownloadCenter.kt`; adds persistent history. |
| 3 | AI provider layer (`AiProvider`, `AiClient`, key store) | vault | OpenAI, Anthropic, Ollama and OpenAI-compatible endpoints; streaming. |
| 4 | Cosmic AI Assistant sidebar | 3 | Page / selection / multi-tab context with consent; saved conversations. |
| 5 | Universal Translator | 3 | In-page translation with restore, language detection, per-site preferences. Google Translate remains the fallback. |
| 6 | Smart Tab Manager | — | Pure analysis (`TabAnalysis`) + dialog; reuses groups, discard and workspaces. |
| 7 | Privacy Dashboard | — | Extends the tracker statistics; cookie and permission views. |
| 8 | Performance Center | 1 | Process measurements, tab states, suspend, thresholds. |
| 9 | Web Inspector | — | Embeds native Chromium DevTools. |
| 10 | Cosmic Focus | 1 | Profiles, timer, blocked sites, chrome hiding. |
| 11 | Version 0.2.0, documentation, tests | all | Release is **not** published by this work. |

## Out of scope for this iteration (documented, not faked)

* Android versions of the assistant, translator, tab manager, privacy dashboard,
  performance centre, inspector and Focus. Only the Download Center is extended on
  Android; the rest are listed as not implemented in `FEATURE_STATUS.md`.
* Tab thumbnails: Qt WebEngine cannot capture hidden tabs reliably.
* Network inspector beyond what Chromium DevTools provides.
* Windows per-process measurements.
