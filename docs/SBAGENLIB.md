SBaGenX Library Roadmap (sbagenlib)
==================================

This branch introduces a phased extraction of the SBaGenX engine into a reusable
C library (`sbagenlib`) while keeping the current CLI behavior stable.

Why
---

`sbagenlib` enables:

- multiple frontends (CLI, GUI, service, bindings) on one core engine,
- deterministic render APIs for automated regression testing,
- cleaner long-term platform/audio-backend evolution.

Phase Plan
----------

Phase 1 (current)
- Add public library API header and implementation.
- Ship build artifacts (`libsbagenx-*.a`) from existing build scripts.
- Keep `sbagenx` behavior unchanged.

Phase 2
- Extract shared DSP primitives into reusable internals (`sbagenlib_dsp.h`).
- Reuse those primitives from both `sbagenx.c` and `sbagenlib.c`.
- Add parity tests that compare shared primitives against legacy formulas.

Phase 3
- Move sequence loading/period engine into the library.
- Keep CLI as a thin frontend.

Phase 4
- Add optional bindings/frontends (Python, GUI, plugin/service use-cases).

Current API (Phase 1)
---------------------

Public header: `sbagenlib.h`

- API version/query:
  - `sbx_api_version()`
  - `sbx_version()`
- Engine lifecycle:
  - `sbx_default_engine_config()`
  - `sbx_engine_create()` / `sbx_engine_destroy()`
  - `sbx_engine_reset()`
- Tone setup and rendering:
  - `sbx_default_tone_spec()`
  - `sbx_engine_set_tone()`
  - `sbx_engine_render_f32()`

Supported tone modes in this first extraction:
- binaural,
- monaural,
- isochronic (simple gated model).

Notes
-----

- This is intentionally a minimal, stable first API.
- Existing `sbagenx` runtime still uses the current monolithic engine in this phase.
- Next phases will migrate existing behavior into the library behind compatibility tests.

Quick Smoke Test
----------------

On Linux:

```bash
gcc -O2 -I. sbagenlib.c examples/sbagenlib/sbx_smoke.c -lm -o /tmp/sbx_smoke
/tmp/sbx_smoke
```

Expected output includes the library version/API plus rendered RMS values.

DSP Parity Test (Phase 2)
-------------------------

```bash
gcc -O2 -I. tests/sbagenlib/test_dsp_parity.c -lm -o /tmp/test_sbx_dsp_parity
/tmp/test_sbx_dsp_parity
```

Expected output:

```text
PASS: sbagenlib DSP parity checks
```
