SBaGenX GUI Reference
=====================

Purpose
-------

This document defines the initial reference architecture and feature
scope for the next SBaGenX GUI effort.

The GUI is not to be a frontend for the `sbagenx` CLI. It is to be a
desktop application built directly on `sbagenxlib`, with the CLI kept as
an independent host/frontend.

Primary Goals
-------------

1. Provide a polished desktop editor for `.sbg` and `.sbgf` files.
2. Use `sbagenxlib` directly for all core behavior.
3. Support real-time validation and diagnostics while editing.
4. Support direct playback and export from the loaded document.
5. Keep the architecture aligned with the library-first direction of
   the project.

Non-Goals
---------

The first GUI milestone is not intended to:

- embed the `sbagenx` CLI as its runtime engine,
- depend on a remote Node.js server,
- act as a general IDE,
- replace every existing CLI feature on day one,
- duplicate parser or renderer math outside `sbagenxlib`.

Core Rule
---------

The GUI must integrate with `sbagenxlib` directly for core behavior.

That includes:

- sequence loading/parsing,
- `.sbgf` loading and preparation,
- immediate-mode parsing where needed,
- built-in program generation,
- runtime activation,
- audio export,
- plot data generation.

The GUI must not shell out to `sbagenx` for these tasks.

CLI-only behavior may still remain host-side elsewhere in the project,
but the GUI is to be a true library-backed frontend.

Recommended Stack
-----------------

The recommended starting stack is:

- `Tauri 2`
- `Svelte`
- `Monaco Editor`
- Rust bridge layer calling `sbagenxlib`

Why this stack:

- `Tauri` gives a desktop shell without the weight of an IDE stack.
- `Svelte` keeps the frontend small and direct.
- `Monaco` gives a proven code-editor surface with syntax coloring,
  markers, and navigation.
- Rust is a good bridge layer for desktop commands and for binding to
  the C library.

Runtime Model
-------------

The packaged GUI is to be a self-contained desktop app.

- `node`/`npm` are build-time tools only.
- The installed app does not require a remote Node.js server.
- The installed app uses the bundled frontend assets, the Tauri host,
  and `sbagenxlib`.

Architecture
------------

The GUI should be split into three layers:

1. Frontend

- editor,
- diagnostics pane,
- transport controls,
- export controls,
- plot/inspection views,
- settings.

2. Native bridge

- Tauri commands/events,
- file dialogs,
- state coordination,
- `sbagenxlib` FFI boundary,
- host-side orchestration where still needed.

3. Core engine

- `sbagenxlib`,
- source of truth for parsing, timing, runtime behavior, rendering, and
  plot data.

Host-side orchestration may still be used for:

- Python/Cairo plotting backend invocation,
- `ffmpeg` invocation,
- desktop file dialogs and window behavior.

But those are to wrap `sbagenxlib`, not the CLI.

First Milestone
---------------

The first useful GUI version should be editor-centric.

Required features:

1. Document handling

- open `.sbg` files,
- open `.sbgf` files,
- support multiple open documents in tabs,
- save current document,
- save as,
- dirty-state tracking.

2. Editor

- syntax highlighting for `.sbg`,
- syntax highlighting for `.sbgf`,
- line numbers,
- monospace editing surface,
- sensible undo/redo behavior.

3. Validation

- real-time validation on a short debounce,
- diagnostics list,
- line-based navigation to errors,
- clear display of library/native parse failures.

4. Transport

- Play,
- Stop,
- Export.

5. Native integration

- validation performed through `sbagenxlib`,
- playback initiated through a library-backed runtime path,
- export performed through `sbagenxlib`,
- no CLI process execution for these actions.

Recommended v0.1 Additions
--------------------------

If the basic milestone lands cleanly, the next additions should be:

- plot preview for the active document,
- beat/frequency inspector,
- document tabs,
- recent files,
- export presets,
- structured status panel for runtime/export activity.

Editor Requirements
-------------------

The editor is the center of the first GUI version.

The GUI should treat `.sbg` and `.sbgf` as first-class editable
documents, not as files hidden behind forms.

Multi-document editing should be a first-class feature. In practical
terms, that means separate tabs for multiple open `.sbg` and `.sbgf`
files, each with its own editor model, diagnostics, path, and dirty
state.

Minimum editor expectations:

- syntax-colored text,
- fast scrolling,
- stable large-file editing behavior,
- visible current line/column,
- jump-to-diagnostic behavior,
- future support for hover/help is desirable but not required for v0.1.

Validation Requirements
-----------------------

Validation should run on a debounce rather than every keystroke.

Validation output should be structured as:

- severity,
- message,
- line,
- column when available,
- source document type (`.sbg` or `.sbgf`).

If `sbagenxlib` does not yet expose structured diagnostics for all
cases, the bridge layer may temporarily normalize library error strings
into editor markers. The long-term goal should still be library-owned
diagnostics where practical.

Playback Requirements
---------------------

The first playback goal is functional correctness, not a full transport
studio.

Required:

- Play current document,
- Stop playback,
- surface runtime errors clearly,
- avoid blocking the UI while playing.

Playback is to use `sbagenxlib` directly.

Export Requirements
-------------------

The first export goal is direct library-backed export of the active
document.

Required:

- export current document to audio,
- choose output path,
- surface progress/error state cleanly.

Advanced export options can follow later.

Plotting
--------

The GUI should use `sbagenxlib` as the source of plot data.

If host-side rendering is required at first, that is acceptable, but the
plot semantics and sampled data should still come from the library.

This preserves the project rule that core math should not be duplicated
in frontends.

Packaging
---------

The GUI should be a real desktop application on:

- Windows
- Linux

Windows presentation quality matters from the beginning. That means the
frontend should be designed as a desktop application, not as a generic
web dashboard dropped into a window.

Development Rules
-----------------

1. No core audio/program logic in the GUI layer.
2. No parser duplication in the GUI layer.
3. `sbagenxlib` remains the source of truth.
4. CLI behavior may be used as a reference during development, but not
   as a runtime dependency for the GUI.
5. Host-side utilities are acceptable only where they are truly host
   concerns.

Suggested Directory Layout
--------------------------

A reasonable starting layout is:

```text
gui/
  package.json
  src/
    app/
    components/
    editor/
    state/
    styles/
  src-tauri/
    src/
      commands/
      ffi/
      state/
```

Milestone Order
---------------

1. Scaffold `Tauri + Svelte`.
2. Integrate `Monaco`.
3. Add open/save for `.sbg` and `.sbgf`.
4. Add debounced validation bridge to `sbagenxlib`.
5. Add diagnostics panel.
6. Add Play/Stop.
7. Add Export.
8. Add plot preview.

Definition Of Success For The First Milestone
---------------------------------------------

The first GUI milestone is successful if a user can:

1. open an `.sbg` or `.sbgf` file,
2. keep multiple `.sbg` and `.sbgf` files open in tabs,
3. edit them in a syntax-colored editor,
4. see validation errors in real time,
5. play the active document,
6. export the active document,
7. do all of the above through `sbagenxlib`, not through the CLI.
