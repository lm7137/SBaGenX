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

Phase 1
- Add public library API header and implementation.
- Ship build artifacts (`libsbagenx-*.a`) from existing build scripts.
- Keep `sbagenx` behavior unchanged.

Phase 2
- Extract shared DSP primitives into reusable internals (`sbagenlib_dsp.h`).
- Reuse those primitives from both `sbagenx.c` and `sbagenlib.c`.
- Add parity tests that compare shared primitives against legacy formulas.

Phase 3.1
- Add a reusable `SbxContext` API that wraps engine lifecycle + load state.
- Add tone-spec parsing (`sbx_parse_tone_spec`) and context load helpers.
- Add block-render path via context (`sbx_context_render_f32`).
- Keep CLI behavior unchanged while preparing deeper runtime migration.

Phase 3.2 (current slice)
- Add keyframed time-varying program support in `SbxContext`.
- Add optional looped playback for keyframed programs.
- Keep CLI behavior unchanged while library runtime expands.

Phase 4
- Add optional bindings/frontends (Python, GUI, plugin/service use-cases).

Current API (Phase 3.2 Slice)
---------------------------

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
- Context + load API:
  - `sbx_parse_tone_spec()`
  - `sbx_context_create()` / `sbx_context_destroy()`
  - `sbx_context_reset()`
  - `sbx_context_set_tone()`
  - `sbx_context_load_tone_spec()`
  - `sbx_context_load_keyframes()`
  - `sbx_context_render_f32()`
  - `sbx_context_time_sec()`
  - `sbx_context_last_error()`

Supported tone modes in this first extraction:
- binaural,
- monaural,
- isochronic (simple gated model).

Supported tone-spec forms (for `sbx_parse_tone_spec`):
- `<carrier>+<beat>/<amp>` (binaural)
- `<carrier>-<beat>/<amp>` (binaural)
- `<carrier>M<beat>/<amp>` or `<carrier>m<beat>/<amp>` (monaural)
- `<carrier>@<pulse>/<amp>` (isochronic)
- `<carrier>/<amp>` (single tone, mapped to binaural with beat=0)
- Optional waveform name prefixes are accepted and ignored by the parser:
  `sine:`, `square:`, `triangle:`, `sawtooth:`

Keyframed program form (`sbx_context_load_keyframes`):
- Load an array of `SbxProgramKeyframe` with:
  - increasing `time_sec` (seconds),
  - one tone spec per keyframe,
  - a single tone mode across all keyframes.
- Rendering interpolates carrier/beat/amplitude/duty linearly between
  keyframes, with optional looping.

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

Context API Test (Phase 3)
--------------------------

```bash
gcc -O2 -I. tests/sbagenlib/test_context_api.c sbagenlib.c -lm -o /tmp/test_sbx_context_api
/tmp/test_sbx_context_api
```

Expected output:

```text
PASS: sbagenlib context API checks
```

Keyframe API Test (Phase 3.2)
-----------------------------

```bash
gcc -O2 -I. tests/sbagenlib/test_keyframes_api.c sbagenlib.c -lm -o /tmp/test_sbx_keyframes_api
/tmp/test_sbx_keyframes_api
```

Expected output:

```text
PASS: sbagenlib keyframe API checks
```
