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

Build the Windows GUI bundle on a Windows host from the repo root:

```bash
bash windows-build-gui.sh
```

The Windows GUI build script now tries to bootstrap missing prerequisites:

- on MSYS2 shells, it prefers `pacman` for:
  - `node`
  - `rust`
  - the MinGW compilers required by `windows-build-sbagenx.sh`
- if `node` or `rust` are still missing, it falls back to `winget` where
  available
- it intentionally installs the actual toolchain/runtime rather than
  trying to install `nvm`
- on MSYS2 GNU shells such as `UCRT64`, it also auto-installs the
  matching native `@tauri-apps/cli-win32-x64-gnu` package when npm's
  optional dependency handling omits it

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
- `windows-build-gui.sh` is intentionally separate from
  `windows-build-sbagenx.sh`.
  - the CLI/library build remains its own path
  - the GUI build reuses the Windows `sbagenxlib` artifacts, then runs the
    Tauri bundle build
  - staged Windows GUI artifacts are copied into `dist/gui/`
- The packaged GUI still needs a full Windows installer validation pass.
