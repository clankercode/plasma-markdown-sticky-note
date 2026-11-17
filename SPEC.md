# Sticky Notes — Specification

A fast, native desktop sticky-notes app for KDE Plasma (Wayland first, X11 supported).
Notes are Markdown files with YAML front matter. Simple, does one thing well, zero lag.

App display name: **Sticky Notes**. Binary: `sticky-notes`. App ID / desktop file id:
`code.clanker.StickyNotes`. Repo: `github.com/clankercode/plasma-markdown-sticky-note`. License: MIT.

## Tech stack

- C++20, Qt 6 (>= 6.6): Core, Gui, Qml, Quick, QuickControls2, Widgets (tray only), Test.
- LayerShellQt (optional at build time, `find_package(LayerShellQt)`; feature-flag `HAVE_LAYERSHELL`).
  Used at runtime only when `QGuiApplication::platformName() == "wayland"` and the compositor supports it.
- CMake >= 3.21, builds with Ninja. No other third-party deps (front-matter parsing is hand-written).
- QML compiled in via `qt_add_qml_module` (qmlcachegen / AOT) for fast startup.

## Storage

- Notes dir default: `$XDG_DATA_HOME/sticky-notes/notes` (i.e. `QStandardPaths::GenericDataLocation + /sticky-notes/notes`).
  Overridable with `--notes-dir <path>` CLI option or `STICKY_NOTES_DIR` env var.
- One file per note: `<id>.md` where `id` is a short sortable unique id (e.g. `20261117-163455-a1b2`).
- Format:

```markdown
---
id: 20261117-163455-a1b2
title: Groceries            # optional; if empty, UI derives title from first non-empty line of body
color: yellow               # yellow|pink|green|blue|purple|orange|gray
created: 2026-11-17T16:34:55+11:00
modified: 2026-11-17T16:40:02+11:00
archived: false
order: 3                    # tab order in the notebook
popped_out: false           # shown as a separate desktop note
pinned: false               # popped-out note stays above windows (otherwise sits on desktop layer)
screen: DP-1                # screen name for popped-out note
x: 120                      # position of popped-out note, relative to the screen's top-left (logical px)
y: 80
width: 280
height: 260
---
- [ ] milk
- [x] eggs
```

- Parser: tolerant subset of YAML for flat `key: value` scalars (strings, quoted strings with escapes,
  ints, bools, ISO timestamps, `#` comments). Unknown keys MUST be preserved verbatim and in order on
  round-trip. A file without front matter is a valid note (all defaults; id derived from filename).
  A malformed front matter never loses body content.
- Writes: atomic (`QSaveFile`), debounced (~400 ms after last keystroke; geometry changes ~600 ms),
  flushed synchronously on quit. Only write when content actually changed.
- Archive = set `archived: true`, never delete. Delete (explicit, with confirmation) moves file to
  `<notes-dir>/.trash/` rather than unlinking.
- External changes: `QFileSystemWatcher` on the dir and files; reload a note when its file changes on disk
  and the in-app copy has no unsaved edits (ignore our own writes). New files appear; removed files vanish.
- Loading is fast: thousands of notes should load in well under 100 ms (only parse front matter + body; no rendering until shown).

## UI

### Notebook (main window)
- Looks like a sticky-note pad: rounded paper with the note's color, subtle shadow, no OS decorations.
- Vertical **tabs on the right edge** — one small colored tab per non-archived note that is not popped out
  (popped-out notes also get a tab, rendered dimmer with a "popped" glyph; clicking it raises/focuses the
  popped note). A **+** tab at the end creates a new note and selects it. Tabs reorderable by drag.
  Tab tooltip = note title.
- Header strip (drag handle to move the window): title (editable inline), color picker dot,
  pop-out button, archive button, overflow menu (Delete…, Show archived, Open notes folder, Settings/About, Quit).
- Body: Markdown. Shows rendered Markdown (Qt `TextEdit.MarkdownText`, GFM) when not focused;
  click / keyboard focus switches to raw plain-text editing in the same place (monospace-ish optional,
  proportional by default). Escape or focus-out returns to rendered view. Clicking a task-list checkbox
  in rendered view toggles `[ ]`/`[x]` in the source. Links open in browser with Ctrl+click (or plain click
  in rendered mode).
- Resizable from edges/corner. Window geometry persisted in app settings (QSettings), not in notes.
- "Show archived" view: list of archived notes with Unarchive / Delete actions.
- Keyboard: Ctrl+N new note, Ctrl+W archive current, Ctrl+Shift+P pop out current, Ctrl+Tab / Ctrl+Shift+Tab
  next/prev note, Ctrl+Q quit, Escape leave edit mode.

### Popped-out note (desktop note)
- Each popped note is its own small frameless window looking like a single sticky note with that note's color
  (same header + body components as the notebook, minus tabs). Header buttons: dock back into notebook,
  pin-on-top toggle, color, archive, menu.
- Drag the header to move it anywhere, including onto another screen. Resize from bottom-right grip.
- Position, size and screen persisted in front matter; restored on next launch.
- Wayland: implemented with LayerShellQt as a layer surface (layer `Bottom` = on the desktop under windows;
  `Top` when pinned), anchored top-left with margins = position; keyboard interactivity `OnDemand`;
  exclusive zone 0 (-1 not needed). Dragging = update margins from pointer delta (press point stays constant in
  surface-local coords). Crossing a screen edge while dragging moves the surface to the adjacent screen
  (recreate/assign `QWindow::setScreen`) using known screen geometries. Also a "Move to screen ▸" menu.
- X11 / no layer-shell: frameless `Qt.Tool` window, drag via `startSystemMove()`, positioned with x/y in global
  coords; pinned → `Qt.WindowStaysOnTopHint`, unpinned → `Qt.WindowStaysOnBottomHint`.
- If the stored screen no longer exists, place on primary screen, clamped to visible area.

### App behavior
- Single instance (QLocalServer named per user + notes dir). Launching again shows/raises the notebook.
- System tray icon (QSystemTrayIcon → SNI on Plasma): left-click toggles notebook; menu: New note,
  Show notebook, Show all desktop notes / Hide all desktop notes, Start on login (writes/removes
  `~/.config/autostart/code.clanker.StickyNotes.desktop`), Quit.
- Closing the notebook hides it (app stays in tray) unless no tray is available.
- `--minimized` start flag (used by autostart) starts with notebook hidden but popped notes shown.
- Colors: a warm, legible palette per color; text contrast meets WCAG AA. Fonts follow system font.
  Follows nothing else from the Plasma theme — it's paper.

## Performance requirements
- Cold start to first frame < 300 ms on a modern machine with 100 notes.
- No synchronous disk I/O on keystroke; typing never blocks on save.
- Idle CPU ~0. No timers running when idle.
- Rendering Markdown only for visible notes.

## Quality
- QtTest unit tests: front-matter parse/serialize round-trip (incl. unknown keys, comments, quoting,
  missing/malformed front matter, CRLF), note store (load dir, create, archive, delete-to-trash,
  debounce/flush, external change reload, own-write suppression), task-checkbox toggle, title derivation.
- QML smoke test runnable offscreen (`QT_QPA_PLATFORM=offscreen`).
- GitHub Actions CI on ubuntu-latest (or a Qt 6.6+ container) building and running tests.
- README with screenshot(s), features, install (build from source; cmake install), usage, file format, shortcuts.
- `.desktop` file, SVG app icon, AppStream metainfo, autostart support, `cmake --install` installs all.
