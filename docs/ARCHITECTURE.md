SBaGenX Architecture Direction
==============================

Purpose
-------

SBaGenX is now developed with a library-first architecture target.
The long-term goal is for `sbagenxlib` to be the source of truth for
core behavior, with `sbagenx.c` acting as a thin host/frontend wrapper.

This does not mean every helper must be exposed as public API in
`sbagenxlib.h`. It means the library must own the behavior that affects
what audio/program output SBaGenX produces.

Project Rules
-------------

1. No new core audio/program logic in `sbagenx.c`.
2. Any behavior-affecting fix goes into `sbagenxlib` first.
3. `sbagenx.c` is limited to CLI/UI/platform orchestration.
4. Legacy execution is progressively removed rather than maintained in
   parallel.

Boundary
--------

`sbagenxlib` should own:

- tone parsing and normalization
- sequence semantics and timing
- built-in program generation
- `.sbgf` loading, preparation, and evaluation
- runtime activation and sampling
- DSP and signal-generation behavior
- export/container semantics

`sbagenx.c` may own:

- `argv` parsing and command dispatch
- user-facing stderr/stdout formatting
- live device output backends
- Python/Cairo plot backend orchestration
- `ffmpeg` subprocess orchestration
- installer/runtime environment concerns

Practical Rule Of Thumb
-----------------------

If a change affects:

- rendered audio
- sequence timing
- built-in program behavior
- parser semantics
- export behavior

then it belongs in `sbagenxlib` unless there is a strong, explicit
reason otherwise.

If a change affects:

- command-line ergonomics
- terminal interaction
- subprocess launching
- platform-specific device glue

then it may remain in `sbagenx.c`.

What This Means For Future Work
-------------------------------

- New functionality should be added in the library first, then exposed
  through the CLI.
- Bug fixes should preferentially remove duplicated CLI-side behavior
  rather than patching around it.
- Fallback to legacy execution should continue to shrink over time until
  it is no longer part of normal execution.

This document is the architectural reference point for ongoing
`sbagenxlib` migration work.
