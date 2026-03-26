SBaGenX GUI Scaffold
====================

This directory contains the third-generation SBaGenX GUI effort.

Architecture
------------

This GUI is intended to be:

- a desktop application,
- built with `Tauri 2`,
- using a `Svelte` frontend,
- backed directly by `sbagenxlib`,
- not a frontend for the `sbagenx` CLI.

Current Status
--------------

The current scaffold provides:

- a Tauri desktop shell,
- a Svelte/Vite frontend,
- a desktop-oriented multi-tab workspace shell,
- a direct Rust bridge into `sbagenxlib`,
- real `.sbg` / `.sbgf` validation,
- live `.sbg` playback through `sbagenxlib`,
- `.sbg` export through `sbagenxlib`.

The current UI is still incomplete, but it is no longer just a shell.
The next major work is packaging and broader runtime coverage.

Development Commands
--------------------

Install dependencies:

```bash
npm install
```

Run the frontend only:

```bash
npm run dev
```

Run the desktop app in development:

```bash
npm run tauri:dev
```

Build the frontend:

```bash
npm run build
```

Typecheck the frontend:

```bash
npm run check
```

Build the desktop app:

```bash
npm run tauri:build
```

Stage the current platform runtime libraries without building:

```bash
npm run prepare:runtime
```

Notes
-----

- The dev server is pinned to port `1420` to align with the Tauri
  configuration.
- `npm run tauri:build` now stages runtime libraries into
  `src-tauri/runtime-bundle/` before bundle creation.
- Runtime staging is platform-specific:
  - Linux stages the newest stable `libsbagenx.so.X.Y.Z` from `dist/`
    as `libsbagenx.so.3`
  - Windows stages the current `win32` / `win64` `sbagenxlib` DLL plus
    the codec/runtime DLLs it depends on
- The packaged GUI still needs a full Windows installer validation pass.
