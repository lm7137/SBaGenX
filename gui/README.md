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
- a minimal Rust bridge command,
- buildable frontend and backend baselines.

The current UI is a scaffold, not a finished editor. The next major
steps are:

1. Monaco editor integration
2. file open/save
3. debounced `.sbg` / `.sbgf` validation via `sbagenxlib`
4. playback controls
5. export controls

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

Notes
-----

- The dev server is pinned to port `1420` to align with the Tauri
  configuration.
- The Rust/Tauri bridge currently exposes a minimal status command so
  the frontend is already wired to a real native boundary.
