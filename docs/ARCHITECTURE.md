# Cosmic — Architecture

## Layering

```
        ┌────────────┐
        │  main.cpp  │  composition root: only file that sees everything
        └─────┬──────┘
   ┌──────────┼───────────┐
   ▼          ▼           ▼
┌──────┐  ┌────────┐  ┌─────────┐
│  ui  │  │browser │  │   web   │   (ui may depend on all layers)
└──┬───┘  └───┬────┘  └────┬────┘
   │          │            │
   │          └─────┬──────┘
   │                ▼  browser/web/core never include ui/ headers
   │          ┌──────────┐
   └─────────►│   core   │  Qt Core/Gui only (no WebEngine, minimal widgets deps)
              └──────────┘
```

Rules enforced by include discipline and package boundaries:

1. `core/` — pure logic: `UrlUtils` (URL vs search), `Paths` (XDG),
   `ActionRegistry` (all user actions + shortcuts, incl.
   `removeDynamicActions(prefix)` for state-driven menu entries),
   `Logging`, `Settings` (`settings.json`, merge-with-defaults, change
   signals), `SearchEngines` (engine registry +
   template validation), `Session` (object model below),
   `UserCatalog` (`users.json`: user ids/names/colors, with one-time
   `contexts.json` migration).
2. `web/` — thin Qt WebEngine wrappers: `WebPage` (routes
   `createWindow` through `WebPageDelegate`), `WebView`,
   `BlocklistInterceptor` (profile-level request rules; enabled flag
   and stats are `core`-driven, no `ui/` knowledge).
   Never includes `ui/`.
3. `browser/` — `Browser` (window creation via injected
   `WindowFactory`, default-profile XDG configuration, owns `Settings`
   + `BlocklistInterceptor`, input resolution, session save/load,
   never includes `ui/`) and the abstract `AppWindow`.
4. `ui/` — all widgets. `BrowserWindow` implements both `AppWindow`
   and `WebPageDelegate`. `SettingsForm` binds widgets to
   `Browser::settings()` only, hosted in a tab (Chrome style).
5. `main.cpp` — the composition root: the factory lambda
   `[] { return new BrowserWindow(&browser); }` is the single place
   where `browser/` and `ui/` are tied together.

Why a factory? So testability and layering survive: `Browser` can be
unit-tested with a stub `AppWindow`, and `ui/` stays invisible to
`browser/`.

## Tabs

`BrowserWindow` owns a `QTabWidget` of `TabContent` widgets. Each
`TabContent` is a 3-page stack:

| Index | Page | Shown when |
| --- | --- | --- |
| 0 `Web` | the `WebView` | a navigation starts (urlChanged/loadStarted) |
| 1 `NewTab` | `NewTabPage` | fresh tab, empty URL |
| 2 `Crash` | local crash page | `renderProcessTerminated` for this page |

Crash handling is **per tab**: the terminated renderer only affects its
own `TabContent`; other tabs and the window keep running (spec §74).
Navigation away from the crash page (Reload button or any URL entry)
spawns a new renderer and reveals the web view.

Closing the last tab resets it to a fresh New Tab page (Chrome-like)
instead of closing the window: the closed page is still recorded for
Reopen, pins/groups are normalized, and the session always keeps one
restorable tab. A lone settings tab is replaced the same way.

## Window creation (window.open / target=_blank)

`QWebEnginePage::createWindow` is `final` in `WebPage` and delegates to
`WebPageDelegate::createPageForWindowType`:

| Chromium request | Cosmic behavior |
| --- | --- |
| `WebBrowserTab` | new tab, foreground, same window |
| `WebBrowserBackgroundTab` | new tab, current tab unchanged |
| `WebBrowserWindow` | new `BrowserWindow` |
| `WebDialog` | **regular window** (documented limitation) |
| `WebBrowserBackgroundWindow` | denied (nullptr) |

## Actions & shortcuts

Every user-facing action is registered once per window in
`ActionRegistry` (`core/`) with a stable string id (`tab.new`,
`tab.pin`, `nav.reload`, …), a label and a shortcut. Widgets and
toolbars only *bind* to those actions (`toolbar->addAction(...)`),
never define shortcuts themselves. The Phase 3 command palette will
enumerate the same ids.

State-driven menu entries (per-group commands) are *dynamic*
actions: rebuilt whenever the live state changes and removed by
prefix via `ActionRegistry::removeDynamicActions()`. This is the
one deliberate exception to "actions registered once" — the state is
in the window, so the menu must be rebuilt from it (the plain context
menus, whose items reflect live tab state directly, are the other
precedent). The user menu needs no dynamic actions: it builds live
from the catalog on every open.

The main toolbar is a single row: back / forward / reload, user
button, expanding address bar, then bookmarks (star menu with
per-site Open / Open in new tab / Copy address / Edit / Remove),
AI assistants menu, AI page-context toggle, RAM Saver menu, password
manager shortcut and the settings gear. There is no second bar;
the "+" new-tab button floats right after the last tab (clamped so
it never hides, both orientations).

Toolbar height is emergent, never fixed: `applyUILayout()` derives
icon size and URL-bar height from the `toolbar.height` model value
(`UiLayout::toolbarMetrics`) and lets the `QToolBar` layout size
itself. Never call `QToolBar::setFixedHeight` — with the dark QSS
active it corrupts the button layout (icons shrink to dots). The
same metrics drive the customizer preview so WYSIWYG holds.

Window-level `addAction()` association makes the shortcuts active in
the focused window (`Qt::WindowShortcut`).

## URL bar & input resolution

- `UrlUtils::looksLikeUrl / toNavigableUrl` (pure, unit-tested):
  scheme → as-is; loopback/IPv4 → `http://`; bare domain → `https://`;
  anything with a space or a single word → **configured search engine**
  (`Browser::resolveInput` passes `Settings::customSearchTemplate` or
  the selected builtin engine; `core/SearchEngines` owns the registry).
- `UrlBar::setPage` binds to the current tab's page; it is a **no-op
  when the binding is unchanged**, so text typed during a load is not
  overwritten by `updateChrome` events.
- All three input sources (address bar, New Tab search box,
  crash-page retry) emit one `inputSubmitted(QString)` signal; the
  window's `handleInput` resolves through `Browser::resolveInput` —
  one code path, no duplicated URL rules.

## Settings

`core/Settings` persists JSON (`~/.config/cosmic/settings.json`) via
`QSaveFile` (atomic replace). Rules: **read merges onto defaults**
(missing keys → default, unknown keys ignored), every setter emits a
typed change signal only on real change, writes happen only on
`save()`. `SettingsForm` is a seven-section tab page (General /
Search engine / Appearance / Privacy / Passwords / Users / Advanced),
opened via the toolbar gear, `Ctrl+,` or `cosmic://settings` and
reused while open: form fields apply on Save (Reset reloads), while
the user list and the vault operate on the live stores.
The interceptor enabled-flag follows `requestBlockingChanged` via
signal connection — `ui/` never reaches into `web/` internals directly.

## Request blocking

`web/BlocklistInterceptor` implements `QWebEngineUrlRequestInterceptor`
on the default profile. Matching: host suffix on a domain boundary
(`doubleclick.net` blocks `ad.doubleclick.net`, never
`notdoubleclick.net`); rules come from the bundled QRC list
(`resources/blocklists/default.txt`, 49 domains) plus an optional user
file (`~/.config/cosmic/blocklist.txt`), normalized (scheme/path/star
stripped, lowercased). The disable flag is atomic and checked per
request; every actual block is logged (`cosmic.debug=true`).

**Honest limits:** request-level only — Qt WebEngine cannot filter DOM
content or apply cosmetic rules, and Chromium extensions are not
exposed. Verified live on the real desktop: a local page referencing
`doubleclick.net` renders its text while the tracker request is
dropped (log: `blocked request to doubleclick.net`).

## Session restore

`core/Session` serializes per-user windows to
`~/.config/cosmic/session.json` (`version: 2`):

```json
{"version": 2, "current": "work", "users": {"work": [
  {"user": "work",
   "groups": [{"id": "g1", "name": "Research", "color": "#2ec4b6", "collapsed": false}],
   "tabs": [
     {"url": "https://example.com", "pinned": true,  "group": "",    "user": "work"},
     {"url": "https://doc.qt.io/qt-6/", "pinned": false, "group": "g1", "user": "work"}
   ]}
]}}
```

The loader still accepts the legacy format where a tab is a plain URL
string (an empty/invalid URL = New Tab page), so pre-object
`session.json` files keep working. Flow:

- save: `main()` calls `Browser::saveSession(windows)` after the event
  loop ends (always, so enabling restore later still has a session);
- load: `--restore` forces it, `--no-restore` suppresses it, otherwise
  the `session.restore` setting decides; a CLI URL beats the session;
- `AppWindow::sessionState()/restoreSessionState()` capture/apply
  window state: pinned order, group membership and owning user id;
  then the first slot reuses a pristine New Tab window only if
  its user matches (otherwise the pristine tab is closed). Per-user
  sessions (`{"current", "users": {id: [windows]}}`, version 2;
  version 1 migrates to the default user) persist on quit and on
  every user switch, so offline users keep their tabs.

## Single window

Exactly one window exists at a time. A second launch forwards its URL
(if any) to the running instance over a per-uid local socket
(`browser/SingleInstance`, QtNetwork only, no extra dependencies) and
exits; `COSMIC_ALLOW_MULTIPLE` disables the gate for automation. New
windows requested by pages (`window.open`) or `Ctrl+N` open as tabs
instead, and session restore merges the current user's stored windows
into the single one. `Browser::newWindow()` still exists for tests.

## Pinned tabs

`TabContent` carries `pinned/groupId/userId` as state;
`BrowserWindow::setTabPinned()` enforces the UI rules: pinned tabs
stay at the front, show no close button and no title text (icon-only),
and `updateChrome` keeps the `tab.pin` action label in sync
(`Ctrl+Shift+P`). The close button object is detached, kept alive and
reattached on unpin (Qt's `setTabButton(side, nullptr)` hides without
destroying the widget). Pinned state persists via `SessionTab`.

## Tab groups

Groups are plain data in `core` (`SessionGroup{id,name,color,collapsed}`)
owned per window by `BrowserWindow`:

- a `groupStrip` above the tab bar renders one chip per group
  (`QToolButton#groupChip`, colored left border, `(n)` count,
  checked = expanded); clicking toggles collapse;
- `setTabTextColor` tints member tabs with the group color and
  `setTabVisible` hides them when collapsed — no proxy widgets;
- groups auto-delete when their last tab leaves;
- the tab context menu's "Tab group" submenu (create/rename/color/
  collapse/assign) uses `NameColorDialog` (name + 6 preset colors,
  also used by user creation) and rebuilds its actions on demand
  from live state.

## Users

`core/UserCatalog` owns `users.json` (id, display name, color;
ids are slugs, `default` protected; `changed()` signal; a legacy
`contexts.json` is imported once, then removed). `Browser` maps
`userId → QWebEngineProfile` (one per user:
`dataDir/profiles/<id>` storage, `cacheDir/profiles/<id>` cache,
persistent cookies, the same request-blocker interceptor — in Qt 6.11
`setUrlRequestInterceptor` takes a raw pointer only, so the single
interceptor is parented to the default profile and serves all
profiles). Each window belongs to exactly one user
(`AppWindow::userId()`); the toolbar user button shows the current
user and its menu swaps the whole tab set (`switchToUser`, which
persists the outgoing set first) or opens Settings → Users.
Per-window dynamic action ids are only `tab.group.assign.<gid>`.

**Honest limit:** users isolate cookies and site storage per
profile. They do not partition Chromium's network stack or resist
fingerprinting — Qt WebEngine exposes no such API (documented in the
README, no fake claims).

## Styling

Fusion style + dark `QPalette` fallback + one bundled QSS
(`resources/styles/cosmic-dark.qss`, loaded from QRC by
`ui/Style::applyDark`). This is **not** a theme engine: one file, one
theme; Phase 3 replaces it with per-theme resources. No global
font-size is forced (system font and scaling stay in control).

The current look is a calm, Dia-inspired dark baseline: deep neutral
charcoal surfaces (`#0f1013` window, `#141519` chrome, `#1b1d24`
inputs/menus), hairline borders, one violet accent (`#7c6cf0`),
pill-shaped controls, and floating rounded tab pills. Three rules make
it honest with Qt's limits:

1. **Tab text colors live in code, not QSS.** A stylesheet color rule
   for `QTabBar::tab` would override `setTabTextColor()`, so
   `BrowserWindow::refreshTabColors()` computes current/ungrouped/
   grouped colors (white / `#9ba1ad` / group color) on every state
   change; the QSS carries no tab color rule by design.
2. **All control icons are bundled local SVGs** (`resources/icons/`:
   back, forward, reload, stop, gear, layers, globe, close ×, chevron,
   check), loaded from QRC — no CDN assets, no emoji, no platform
   icons that would clash with the dark theme. Group chip accent
   colors are applied per-chip via a tiny per-widget stylesheet.
3. **Widget identity is preserved for tests**: every style hook uses
   the existing objectNames (`urlBar`, `groupChip`, `settingsTabs`,
   `usersList`, …), so QSS changes can never break lookups.

The New Tab page ships an original local SVG (`resources/icons/saturn.svg`,
rendered by `QSvgRenderer`), never an emoji and never a CDN asset.

## Persistence (XDG, spec §55)

Application name is `"cosmic"` with **no organization name**, so Qt
yields:

- config: `~/.config/cosmic`
- data (profile): `~/.local/share/cosmic/profile`
- cache: `~/.cache/cosmic/http-cache`

`Browser::configureDefaultProfile` points the default
`QWebEngineProfile` there; tests override `XDG_*_HOME` before any
QApplication exists (and wipe their temp dir on every run) so they
never touch the real profile.

UI layout overrides live in `~/.config/cosmic/ui.json`
(`schemaVersion: 1`): toolbar zones with component ids, icon size,
spacing, tab position, sidebar placeholder. Missing files fall back
to the built-in default; invalid files are rejected with a log line
and never applied or written back.

## UI Customizer (Appearance → Customize Cosmic, steps A–C/E–F)

Separate window, separate model — never inside `BrowserWindow`:

- `src/core/UILayout.{h,cpp}` — the data layer (LayoutModel +
  LayoutSerializer in one): `UIConfiguration` (toolbar zones with
  component ids, icon size, spacing; tab position; sidebar
  placeholder), `defaultConfiguration()` (mirrors the shipped UI
  exactly), strict validation (known ids only, URL bar exactly once,
  supported schemaVersion), safe defaults for missing scalars,
  atomic `ui.json` load/save. No widgets, no UI dependency.
- `src/core/LayoutEditor.{h,cpp}` — working-copy operations
  (move/add/remove, icon size, spacing) with URL-bar protection and
  `changed`/`error` signals. Pure logic, unit-tested without widgets.
- `src/ui/customizer/` — `CustomizerWindow` (header with Edit Mode,
  Save/Discard/Close, splitter, status), `CustomizerPreview` (live
  mock chrome: real icons, real `NebulaTabBar` with two static tabs,
  Saturn placeholder for page content — the preview renders the
  model, it is not a second browser), `ComponentLibrary`
  (Navigation/Tabs/Browser UI/Content/Future; future items shown as
  "planned" and never draggable), `ComponentProperties` (live
  component data; fine-grained editors land in step D).
- `Browser` owns the persisted `UIConfiguration` (`ui.json`, XDG
  config); `BrowserWindow::applyUILayout()` rebuilds its toolbar
  from the model (invalid configs fall back to default, never break
  the real UI). Save applies to every open window via an explicit
  `BrowserWindow::allWindows()` registry (QPointer-swept, no blind
  global widget scans).
- Toolbar items reference components (`nav.back`, `urlbar`,
  `settings`, …); action-backed ones reuse the existing
  `ActionRegistry` ids — the customizer creates no shortcuts.
  Structure (zones, sizes, visibility) lives in the model; style
  (colors, padding, radius) stays in the QSS theme: the future
  Theme Engine separation point.

Honest limits (land in later steps, tracked as next work): per-
component property editors (D — landed for toolbar/URL/tabs metrics:
height, spacing, icon size, colors, radius, width, position, plus
visibility; button-level size/color stay future work), real drag
visuals polish (E is functional: press-drag between/inside zones,
library drag-in, move menu fallback), tab options (G — position,
height, radius, spacing, plus toggle all live), sidebar (H — model
fields exist but nothing renders/applies yet), presets (I),
undo/redo (J), import/export (K). `PresetManager`/`UndoRedoManager`
do not exist yet rather than existing as stubs.

## Known Qt WebEngine 6.11 limitations (honest list)

| Wanted | Qt WebEngine reality | Cosmic stance |
| --- | --- | --- |
| Chromium extensions | not exposed to embedders | impossible; out of scope until Qt exposes it |
| Tab sleeping / discarding | no API | not implemented |
| Per-site settings / storage partitioning | not exposed | not implemented |
| Fingerprinting resistance | only coarse (fonts/GL reported generically) | documented, no fake claims |
| Ad/tracker blocking | request interception possible (`QWebEngineUrlRequestInterceptor`), **no DOM/cosmetic filtering** | implemented request-level (Phase 2): 49 built-in domains + user rules; document as partial |
| Real popup windows (`WebDialog`) | no popup widget in WebEngine | mapped to a regular window |
| Per-profile WebEngine settings | mostly global (`QWebEngineSettings`) | window-scoped toggles where possible, engine limits otherwise |
| `renderProcessTerminated` delivery | **lost for crashed renderers embedded in tab widgets on this machine** (see below) | crash UI is wired correctly; test verifies the handler directly and QSKIPs the signal path |

### Environment findings on this machine (Qt 6.11.2, Wayland, software rendering)

Measured with standalone probes (not assumptions):

1. **Crash signal loss.** Navigating an *embedded* `QWebEngineView` (any
   layout/tab structure) to `chrome://crash` kills the renderer (Chromium
   logs it) but Qt never emits `renderProcessTerminated` — on both the
   offscreen and the Wayland platform. A *plain top-level* view reports
   the signal correctly. On offscreen+visible the embedded case can even
   SIGSEGV inside `libQt6Quick` (`QQuickRenderControlPrivate::windowDestroyed`)
   while Qt tears down the render delegate. Consequence: a crashed tab may
   show a dead view instead of our crash page until Qt fixes delivery.
   The TabContent handler itself is verified directly by the test.

2. **Teardown order matters.** QtWebEngine's internal QQuick items must be
   destroyed while `QApplication` is still fully alive. Both the tests and
   `main()` destroy windows explicitly after the event loop ends; leaking
   them into `~QApplication` risks the same `libQt6Quick` SIGSEGV.

3. **Console logging needs a terminal.** Qt 6.11 suppresses Qt message
   output on stderr when the process has no controlling terminal (verified:
   the same binary prints under a pty, is silent in a tty-less shell —
   `qWarning` included). Run Cosmic from a terminal normally; for
   automation set `QT_FORCE_STDERR_LOGGING=1` (the `QT_LOGGING_TO_CONSOLE`
   name is deprecated in 6.11).

Nothing above is simulated with fake UI.

## Testing strategy

- `smoke_webengine` — offscreen, OTR profile: proves real HTTPS
  navigation works on this machine (fails fast if the engine/network
  stack is broken).
- `test_urlutils` — pure unit tests, app-less.
- `test_settings`, `test_searchengines`, `test_blocklist`,
  `test_session`, `test_users` — unit tests of the `core`/`web`
  logic (defaults, persistence, merge, signals, matching rules, stats,
  session objects incl. legacy string tabs, per-user sessions,
  user id/slug rules, contexts.json migration).
- `test_browserwindow` — Qt Test + real `BrowserWindow`, offscreen:
  New Tab page, `openUrl` pristine-tab reuse, actions,
  open/close/reopen/reorder, all four `createWindow` types, resolver +
  interceptor wiring, the settings dialog (apply → memory + disk +
  interceptor flag, users CRUD), a session
  save/restore roundtrip through real windows (with pinned tabs and
  groups), pinned-tab UI rules, tab-group lifecycle (chip strip,
  collapse, rename/color, auto-delete), and crash isolation (real
  `chrome://crash`, with a direct `showCrash()` fallback + QSKIP
   because of finding 1 above).
- `desktop_entry` — `desktop-file-validate` on
  `resources/cosmic.desktop` (registered only when the binary from
  `desktop-file-utils` is available; 9 tests total).

9 tests, all passing (`ctest`, offscreen, XDG isolated and
wiped per run).

Desktop integration: `resources/cosmic.desktop` (`Exec=cosmic %u`,
`StartupWMClass=cosmic`) plus hicolor icons at 16–512 px + scalable
are installed by CMake under `${CMAKE_INSTALL_DATADIR}` (generated once
from `resources/icons/saturn.svg` with `rsvg-convert`, committed — no
build-time dependency). The app calls
`QGuiApplication::setDesktopFileName("cosmic")` and sets the window
icon from the theme (resource fallback) so Plasma associates windows
with the launcher.

Real-desktop verification (not simulated): launch on Wayland with
`--quit-after`, exit code 0; `QIcon::fromTheme("cosmic")` resolves to
the Saturn icon on the real platform (offscreen theme paths are
incomplete — see limitations); installed `~/.local/bin/cosmic` runs
cleanly and the window titlebar shows the Saturn icon (spectacle
capture of a live Cosmic window); session file written and restored
(`session restore requested: 1 window(s), 4 tab(s)` with pinned +
grouped tabs); tracker request dropped on a live page
(`blocked request to doubleclick.net`); screenshot of the restored
window shows the group strip chips with colored borders, the
icon-only pinned tab without close button and the user button
("Personal") in the toolbar. Styled widgets that no test exercises
visually (settings tab sections, context-menu popup, name/color
dialog) were verified with offscreen `grab()` renders of the real
widgets under the real QSS.

## What came next (Phase 2 + final round → upcoming)

Completed: search-engine registry + resolution, settings dialog,
request-level blocking, session restore/CLI flags, users
(`UserCatalog` + per-user `QWebEngineProfile` + tab-set switching),
pinned tabs,
tab groups, desktop entry + Saturn app icons (hicolor 16–512 +
scalable), 9-test suite (100%).

Upcoming: start page, theme engine, command
palette), then AI/workspaces per the specification.
