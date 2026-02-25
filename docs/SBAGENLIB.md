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

Phase 3.2
- Add keyframed time-varying program support in `SbxContext`.
- Add optional looped playback for keyframed programs.
- Keep CLI behavior unchanged while library runtime expands.

Phase 3.3
- Add minimal sequence text/file loader APIs on top of keyframes.
- Support simple line-based format: `<time> <tone-spec>`.
- Keep CLI behavior unchanged while increasing reusable library coverage.

Phase 3.4
- Add a thin subset loader for real `.sbg` timing-style lines.
- Support `HH:MM` and `HH:MM:SS` timing tokens with tone-specs.
- Keep CLI behavior unchanged while moving sequence parsing into the library.

Phase 3.5
- Add transition semantics on keyframe segments (`linear`/`step`).
- Handle boundary behavior at exact keyframe times deterministically.
- Add timing/interpolation edge-case validation in loaders and tests.

Phase 3.6
- Add keyframe introspection APIs on `SbxContext`.
- Expose keyframe count and indexed keyframe retrieval for adapter/front-end
  migration work.
- Add direct API tests for keyframe access behavior.

Phase 3.7
- Wire an initial `sbagenx` CLI bridge that can load sbagenlib keyframes from
  external files.
- Add `-p libseq` (minimal keyframe text) and `-p libsbg` (HH:MM subset) paths.
- Keep legacy runtime unchanged for existing `drop`/`sigmoid`/`curve`/`slide`
  flows while proving end-to-end library-to-CLI integration.

Phase 3.8
- Improve bridge fidelity for keyframe transition modes.
- Preserve `step`/`hold` intent by emitting hold markers ahead of the next
  keyframe boundary in the generated legacy timeline.
- Add optional bridge-level `loop` token handling for `-p libseq`/`-p libsbg`.

Phase 3.9
- Migrate built-in preprogram generators (`-p drop`, `-p sigmoid`,
  `-p curve`, `-p slide`) to keyframe generation + sbagenlib runtime render.
- Keep `-D` compatibility through a bridge path while validating runtime parity.

Phase 3.10
- Remove obsolete helper paths left over from legacy preprogram emission.
- Clean warnings and reduce migration-specific dead code.

Phase 3.11
- Add sbagenlib-runtime support for extra tone-spec overlays in preprogram
  modes by running auxiliary sbagenlib contexts in parallel.
- Keep `mix/<amp>` support and explicit errors for unsupported extras.

Phase 3.12 (current slice)
- Make `-D` for sbagenlib-backed paths library-native: dump keyframes directly
  instead of routing through legacy period construction.
- Keep unsupported-extra inspection available in `-D` output.

Phase 4
- Add optional bindings/frontends (Python, GUI, plugin/service use-cases).

Current API (Phase 3.12 Slice)
------------------------------

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
  - `sbx_context_load_sequence_text()`
  - `sbx_context_load_sequence_file()`
  - `sbx_context_load_sbg_timing_text()`
  - `sbx_context_load_sbg_timing_file()`
  - `sbx_context_keyframe_count()`
  - `sbx_context_get_keyframe()`
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
- `SbxProgramKeyframe.interp` sets segment behavior to next keyframe:
  - `SBX_INTERP_LINEAR`
  - `SBX_INTERP_STEP`

Minimal sequence text/file form (Phase 3.3):
- One keyframe per non-empty line:
  - `<time-token> <tone-spec> [linear|ramp|step|hold]`
- Time token:
  - seconds default (e.g. `30`), or suffix `s`, `m`, `h`
  - examples: `0s`, `45`, `0.5m`, `0.02h`
- Supported comments:
  - `# ...`
  - `; ...`
  - `// ...`
- Example file:
  - `examples/sbagenlib/minimal-keyframes.sbxseq`

SBG timing subset form (Phase 3.4):
- One keyframe line per non-empty line:
  - `<HH:MM> <tone-spec> [linear|ramp|step|hold]`
  - `<HH:MM:SS> <tone-spec> [linear|ramp|step|hold]`
- Supported comments:
  - `# ...`
  - `; ...`
  - `// ...`
- Example file:
  - `examples/sbagenlib/minimal-sbg-timing.sbg`

Notes
-----

- This is intentionally a minimal, stable core API.
- `sbagenx` now uses sbagenlib runtime for `-p libseq`, `-p libsbg`,
  and built-in preprogram generators (`drop/sigmoid/curve/slide`).
- Remaining work is focused on full legacy `.sbg` path parity and legacy
  non-core voice/effect migration.

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

Sequence Loader Test (Phase 3.3)
--------------------------------

```bash
gcc -O2 -I. tests/sbagenlib/test_sequence_loader_api.c sbagenlib.c -lm -o /tmp/test_sbx_seq_loader
/tmp/test_sbx_seq_loader
```

Expected output:

```text
PASS: sbagenlib sequence loader API checks
```

SBG Timing Loader Test (Phase 3.4)
----------------------------------

```bash
gcc -O2 -I. tests/sbagenlib/test_sbg_timing_loader_api.c sbagenlib.c -lm -o /tmp/test_sbx_sbg_loader
/tmp/test_sbx_sbg_loader
```

Expected output:

```text
PASS: sbagenlib sbg timing loader checks
```

Transition Semantics Note (Phase 3.5)
-------------------------------------

- `step`/`hold` keeps the current keyframe tone until the next keyframe time.
- At exact keyframe boundaries, the new keyframe tone is selected.

Keyframe Access API Test (Phase 3.6)
------------------------------------

```bash
gcc -O2 -I. tests/sbagenlib/test_keyframe_access_api.c sbagenlib.c -lm -o /tmp/test_sbx_keyframe_access_api
/tmp/test_sbx_keyframe_access_api
```

Expected output:

```text
PASS: sbagenlib keyframe access API checks
```

CLI Bridge Smoke Test (Phase 3.7)
---------------------------------

```bash
./dist/sbagenx-linux64 -D -p libseq examples/sbagenlib/minimal-keyframes.sbxseq
./dist/sbagenx-linux64 -D -p libsbg examples/sbagenlib/minimal-sbg-timing.sbg
```

Notes:

- `libseq` reads the Phase 3.3 line format: `<time> <tone-spec> [interp]`.
- `libsbg` reads the Phase 3.4 timing subset: `<HH:MM[:SS]> <tone-spec> [interp]`.
- Optional trailing `loop` token is accepted by both bridge commands.
- `-D` now prints sbagenlib keyframes directly (`time tone-spec interp`).
- Built-in programs (`drop/sigmoid/curve/slide`) now generate keyframes and
  render via sbagenlib runtime in normal playback mode.
- In sbagenlib runtime, extra tone-spec overlays that are parseable as
  sbagenlib tones are mixed as auxiliary library contexts.
