SBaGenX Library Roadmap (sbagenxlib)
==================================

This branch introduces a phased extraction of the SBaGenX engine into a reusable
C library (`sbagenxlib`) while keeping the current CLI behavior stable.

Why
---

`sbagenxlib` enables:

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
- Extract shared DSP primitives into reusable internals (`sbagenxlib_dsp.h`).
- Reuse those primitives from both `sbagenx.c` and `sbagenxlib.c`.
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
- Wire an initial `sbagenx` CLI bridge that can load sbagenxlib keyframes from
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
  `-p curve`, `-p slide`) to keyframe generation + sbagenxlib runtime render.
- Keep `-D` compatibility through a bridge path while validating runtime parity.

Phase 3.10
- Remove obsolete helper paths left over from legacy preprogram emission.
- Clean warnings and reduce migration-specific dead code.

Phase 3.11
- Add sbagenxlib-runtime support for extra tone-spec overlays in preprogram
  modes by running auxiliary sbagenxlib contexts in parallel.
- Keep `mix/<amp>` support and explicit errors for unsupported extras.

Phase 3.12
- Make `-D` for sbagenxlib-backed paths library-native: dump keyframes directly
  instead of routing through legacy period construction.
- Keep unsupported-extra inspection available in `-D` output.

Phase 3.13
- Route immediate mode (`-i`) through sbagenxlib runtime whenever all provided
  tokens are sbagenxlib-parseable.
- Keep automatic fallback to legacy immediate parsing for legacy-only tokens.

Phase 3.14
- Extend sbagenxlib tone model with white/pink/brown noise modes.
- Make noise tone-specs parseable in sbagenxlib contexts and runtime overlays.

Phase 3.15
- Improve runtime diagnostics for unsupported extra tone-specs in preprogram
  sbagenxlib paths with explicit supported-token guidance.

Phase 3.16
- Add waveform-aware tone handling in sbagenxlib (`sine/square/triangle/sawtooth`)
  for binaural, monaural, and isochronic tones.
- Preserve waveform in parser output, keyframe evaluation, runtime rendering,
  and sbagenx adapter formatting (`-D` dumps).
- Apply CLI global waveform default (`-w`) to sbagenxlib-backed immediate and
  preprogram tone generation when no explicit waveform prefix is provided.

Phase 3.17
- Add context-level default waveform control via
  `sbx_context_set_default_waveform()`.
- Apply the default waveform to unprefixed tones loaded through
  `sbx_context_load_tone_spec()`, `sbx_context_load_sequence_*()`, and
  `sbx_context_load_sbg_timing_*()`.
- Wire `sbagenx -p libseq` / `-p libsbg` to pass CLI global `-w` as the
  sbagenxlib context default waveform.

Phase 3.18
- Add opportunistic sbagenxlib routing for normal CLI `seq-file` input:
  if a sequence file matches the sbagenxlib timing/tone subset loaders, run it
  directly via sbagenxlib runtime.
- Keep automatic fallback to the legacy parser/runtime for full-feature `.sbg`
  constructs that are outside current sbagenxlib subset support.

Phase 3.19
- Add `SBAGENX_SEQ_BACKEND=auto|legacy|sbagenxlib` runtime selector for
  `seq-file` loading:
  - `auto` (default): try sbagenxlib subset route, then fall back to legacy.
    For `-D` dumps, `auto` keeps legacy output format by default.
  - `legacy`: force existing legacy parser/runtime path.
  - `sbagenxlib`: require sbagenxlib subset compatibility (error otherwise).
    Back-compat alias: `sbagenlib`.

Phase 3.20
- Preserve default legacy `-D` semantics for normal `seq-file` usage in
  `SBAGENX_SEQ_BACKEND=auto`, while keeping explicit sbagenxlib dump behavior
  available via `SBAGENX_SEQ_BACKEND=sbagenxlib` (or `-p libseq` / `-p libsbg`).

Phase 3.21
- Surface `SBAGENX_SEQ_BACKEND` in CLI help output so backend selection is
  discoverable during migration/testing.

Phase 3.22
- Complete repository/library naming migration from `sbagenlib` to
  `sbagenxlib`, with compatibility aliases retained where needed.

Phase 3.23
- Add spin-noise tone support to sbagenxlib:
  - `spin:` (pink base), `bspin:` (brown base), `wspin:` (white base)
  - waveform-aware spin modulation via existing waveform prefixes/defaults.
- Extend sbagenx/sbagenxlib bridge formatting and extra-tone diagnostics to
  include spin-family tones in library-native runtime paths.

Phase 3.24
- Add bell tone support to sbagenxlib:
  - parse `bell<carrier>/<amp>` (with waveform prefixes/defaults),
  - render bell envelope decay in the library runtime,
  - include bell in sbagenx/sbagenxlib bridge formatting and runtime
    extra-tone diagnostics.

Phase 3.25
- Extend sbagenx/sbagenxlib runtime adapter to accept preprogram extra mix
  effects in library-backed paths:
  - `mixspin`, `mixpulse`, `mixbeat` token parsing,
  - runtime mix-stream effect processing in `outChunkSbx`,
  - validation requiring both a mix input stream and explicit `mix/<amp>`.

Phase 3.26
- Extend sbagenx/sbagenxlib runtime adapter immediate mode (`-i`) to accept
  extra mix effects:
  - `mixspin`, `mixpulse`, `mixbeat` token parsing for `-i`,
  - same validation rules (`-m`/`-M` active and explicit `mix/<amp>`),
  - include parsed mix effects in `-D` immediate output.

Phase 3.27
- Add sbagenxlib context-level auxiliary tone mixing:
  - new API to set/get/clear aux tones on a context,
  - aux tones render in the same context clock/loop domain as the main tone,
  - sbagenx runtime adapter now uses context aux tones directly instead of
    maintaining separate aux contexts/buffers in the CLI layer.

Phase 3.28
- Add shared tone-spec formatting API to sbagenxlib:
  - new `sbx_format_tone_spec()` function for canonical string emission,
  - replace duplicate sbagenx-side formatter usage with the library API for
    keyframe and immediate `-D` output paths.

Phase 3.29
- Normalize sbagenxlib-backed `-D` extra-token diagnostics:
  - remove stale "legacy timeline bridge" wording in preprogram dumps,
  - report unsupported extras as textual-preservation-only for `-D`,
  - clarify that curve `mixamp` expressions are ignored in keyframe dump mode.

Phase 3.30
- Expose default-waveform-aware tone parsing in sbagenxlib:
  - new `sbx_parse_tone_spec_ex(spec, default_waveform, out)` API,
  - switch sbagenx runtime extra/immediate parsing to this API,
  - remove duplicated sbagenx-side waveform-prefix application helper.

Phase 3.31
- Remove adapter/library aux-limit drift:
  - switch sbagenx runtime adapter aux limits to `SBX_MAX_AUX_TONES`
    from `sbagenxlib.h`,
  - eliminate duplicate local constant definitions.

Phase 3.32
- Move mix-effect runtime DSP ownership into sbagenxlib:
  - add shared mix-effect spec/API (`SbxMixFxSpec`, parser, context set/get),
  - add `sbx_context_apply_mix_effects()` with internal phase/Hilbert state,
  - switch sbagenx runtime adapter to call sbagenxlib for mix-effect
    processing instead of running mix-effect DSP in `outChunkSbx`.

Phase 3.33
- Move mix-amplitude keyframe profile ownership into sbagenxlib:
  - add shared mix-amp keyframe API (`SbxMixAmpKeyframe`, set/query helpers),
  - switch sbagenx runtime adapter to use `sbx_context_mix_amp_at()` for
  runtime mix gain and remove adapter-owned mix keyframe state.

Phase 3.34
- Unify runtime extra-token parsing in sbagenxlib:
  - add `sbx_parse_extra_token()` for `mix/<amp>`, mix effects, and tone specs,
  - switch sbagenx adapter immediate/extra parsing to this API,
  - reduce duplicated token-classification logic in `sbagenx.c`.

Phase 3.35
- Start reusing sbagenxlib parsers in the legacy voice parser:
  - replace duplicated `mixspin/mixpulse/mixbeat` `sscanf` chains with
    `sbx_parse_mix_fx_spec()` mapping,
  - keep legacy runtime output behavior unchanged while reducing parser drift.

Phase 3.36
- Continue parser convergence in legacy `readNameDef()` voice parsing:
  - replace duplicated tone `sscanf` matrix (binaural/isochronic/noise/bell/
    spin-family) with `sbx_parse_tone_spec_ex()` + shared legacy voice mapping,
  - keep existing legacy-only tokens (`mix/<amp>`, `waveNN:...`) and behavior.

Phase 3.37
- Continue parser convergence in legacy `readNameDef()` by routing token classes
  through `sbx_parse_extra_token()`:
  - parse `mix/<amp>`, mix effects, and sbagenxlib tone-specs from one helper,
  - preserve sequence-level checks (`checkMixInSequence`) and legacy voice
    type mapping/output behavior.

Phase 3.38
- Remove sbagenx runtime adapter mix-effect mirror state:
  - rely on context-owned mix effects only (`sbx_context_set_mix_effects()` and
    `sbx_context_mix_effect_count()`),
  - remove duplicate adapter-side mixfx arrays/counters.

Phase 3.39
- Remove adapter runtime fields that became write-only during migration:
  - drop `sbx_runtime_loop` and `sbx_runtime_mix_amp_pct`,
  - keep runtime timing/state derived from context/keyframes only.

Phase 3.40
- Normalize unsupported-extra diagnostics for sbagenxlib preprogram paths:
  - add a shared helper for `-p drop` / `-p sigmoid` / `-p curve` / `-p slide`,
  - keep existing `-D` textual-preservation behavior and curve-specific notes.

Phase 3.41
- Add a dedicated legacy parser bridge smoke test script:
  - validates that legacy `.sbg` voice tokens now routed through sbagenxlib
    parsers still produce expected `-D` output forms,
  - covers tone, isochronic, bell, spin-family, noise, and mixbeat tokens.

Phase 3.42
- Consolidate legacy voice-token parsing behind one helper:
  - add `parse_legacy_voice_token()` wrapper for `waveNN`, `mix/<amp>`,
    mix-effects, and sbagenxlib tone parsing,
  - simplify `readNameDef()` to a small dispatch path.

Phase 3.43
- Add shared mix-effect formatter API:
  - new `sbx_format_mix_fx_spec()` for canonical `mixspin/mixpulse/mixbeat`
    string emission,
  - switch immediate `-D` mix-effect output to this API.

Phase 3.44
- Tighten migration test hygiene:
  - remove stale warnings in `test_context_api` so the expanded parser/mixfx
    regression checks run warning-clean under `-Wall -Wextra`.

Phase 3.45
- Add context-duration API ownership in sbagenxlib:
  - new `sbx_context_duration_sec()` to expose keyframed program duration,
  - switch sbagenx runtime activation paths to query duration from the
    library instead of manually reading the last keyframe.

Phase 3.46
- Move mix-stream sample processing ownership into sbagenxlib:
  - add `sbx_context_mix_stream_sample()` to combine context mix amplitude
    profiles and mix effects on one sample path,
  - switch `outChunkSbx()` to call this library API instead of duplicating
    mix gain/effect math in `sbagenx.c`.

Phase 3.47
- Add runtime-extras orchestration API in sbagenxlib:
  - new `sbx_context_configure_runtime()` wraps mix-amp, mix-effects, and
    aux-tone setup in one library entry point,
  - switch sbagenx runtime activation paths to this API and remove adapter-side
    extra-setup helpers.

Phase 3.48
- Expand context API regression coverage:
  - add tests for `sbx_context_duration_sec()` in keyframed and static modes,
  - add tests for `sbx_context_configure_runtime()` and
    `sbx_context_mix_stream_sample()` paths.

Phase 3.49
- Allow mixed tone modes in keyframed sbagenxlib programs:
  - remove same-mode keyframe restriction in `sbx_context_load_keyframes()`,
  - force deterministic step behavior across mode/waveform changes at segment
    boundaries.

Phase 3.50
- Extend sbagenxlib SBG timing subset loader with named tone-sets:
  - support inline named definitions (`name: <tone-spec>` / `name: -`),
  - support timeline references to named tone-sets plus legacy transition
    tokens (`->`, `==`, etc.) mapped to sbagenxlib interpolation behavior.

Phase 3.51
- Unify SBG clock-token parsing between sbagenx and sbagenxlib:
  - add shared `sbx_parse_sbg_clock_token()` API (`HH:MM` / `HH:MM:SS`),
  - switch legacy `sbagenx.c` `readTime()` to use the library parser.

Phase 3.52
- Add explicit off-tone parity in sbagenxlib formatting/bridging:
  - parse/format `-` as `SBX_TONE_NONE`,
  - add `SBAGENX_SEQ_BACKEND=sbagenxlib` smoke test for named SBG subset
    dump output including off-keyframes.

Phase 3.53 (current slice)
- Improve sbagenxlib SBG named-subset discoverability:
  - add `examples/sbagenxlib/minimal-sbg-named.sbg`,
  - surface named-subset example in CLI `-p` usage examples and docs.

Phase 4
- Add optional bindings/frontends (Python, GUI, plugin/service use-cases).

Current API (Phase 3.53 Slice)
------------------------------

Public header: `sbagenxlib.h`

- API version/query:
  - `sbx_api_version()`
  - `sbx_version()`
- Engine lifecycle:
  - `sbx_default_engine_config()`
  - `sbx_engine_create()` / `sbx_engine_destroy()`
  - `sbx_engine_reset()`
- Tone setup and rendering:
  - `sbx_default_tone_spec()`
  - `sbx_format_tone_spec()`
  - `sbx_engine_set_tone()`
  - `sbx_engine_render_f32()`
- Context + load API:
  - `sbx_parse_tone_spec()`
  - `sbx_parse_tone_spec_ex()`
  - `sbx_parse_sbg_clock_token()`
  - `sbx_format_mix_fx_spec()`
  - `sbx_parse_mix_fx_spec()`
  - `sbx_parse_extra_token()`
  - `sbx_context_create()` / `sbx_context_destroy()`
  - `sbx_context_reset()`
  - `sbx_context_set_tone()`
  - `sbx_context_set_default_waveform()`
  - `sbx_context_load_tone_spec()`
  - `sbx_context_load_keyframes()`
  - `sbx_context_load_sequence_text()`
  - `sbx_context_load_sequence_file()`
  - `sbx_context_load_sbg_timing_text()`
  - `sbx_context_load_sbg_timing_file()`
  - `sbx_context_set_aux_tones()`
  - `sbx_context_aux_tone_count()`
  - `sbx_context_get_aux_tone()`
  - `sbx_context_set_mix_effects()`
  - `sbx_context_mix_effect_count()`
  - `sbx_context_get_mix_effect()`
  - `sbx_context_apply_mix_effects()`
  - `sbx_context_mix_stream_sample()`
  - `sbx_context_set_mix_amp_keyframes()`
  - `sbx_context_configure_runtime()`
  - `sbx_context_mix_amp_at()`
  - `sbx_context_keyframe_count()`
  - `sbx_context_get_keyframe()`
  - `sbx_context_duration_sec()`
  - `sbx_context_render_f32()`
  - `sbx_context_time_sec()`
  - `sbx_context_last_error()`

Supported tone modes in this first extraction:
- binaural,
- monaural,
- isochronic (simple gated model),
- white/pink/brown noise,
- bell,
- spin-noise tones (`spin`, `bspin`, `wspin`).

Supported tone-spec forms (for `sbx_parse_tone_spec`):
- `<carrier>+<beat>/<amp>` (binaural)
- `<carrier>-<beat>/<amp>` (binaural)
- `<carrier>M<beat>/<amp>` or `<carrier>m<beat>/<amp>` (monaural)
- `<carrier>@<pulse>/<amp>` (isochronic)
- `bell<carrier>/<amp>` (bell)
- `-` (off / no tone)
- `<carrier>/<amp>` (single tone, mapped to binaural with beat=0)
- `white/<amp>`, `pink/<amp>`, `brown/<amp>` (noise tones)
- `spin:<width-us><spin-hz>/<amp>` (pink-noise spin)
- `bspin:<width-us><spin-hz>/<amp>` (brown-noise spin)
- `wspin:<width-us><spin-hz>/<amp>` (white-noise spin)
- Optional waveform name prefixes are parsed and stored in
  `SbxToneSpec.waveform`: `sine:`, `square:`, `triangle:`, `sawtooth:`
- When loading through a context, unprefixed tonal specs use that context's
  default waveform (set by `sbx_context_set_default_waveform()`), otherwise
  they default to sine.

Keyframed program form (`sbx_context_load_keyframes`):
- Load an array of `SbxProgramKeyframe` with:
  - increasing `time_sec` (seconds),
  - one tone spec per keyframe.
- Rendering interpolates carrier/beat/amplitude/duty linearly between
  keyframes (within compatible mode/waveform segments), with optional looping.
- When adjacent keyframes change tone mode or waveform, sbagenxlib applies
  step behavior at that boundary.
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
  - `examples/sbagenxlib/minimal-keyframes.sbxseq`

SBG timing subset form (Phase 3.4):
- One keyframe line per non-empty line:
  - `<HH:MM> <tone-spec> [linear|ramp|step|hold]`
  - `<HH:MM:SS> <tone-spec> [linear|ramp|step|hold]`
- Extended subset additions (Phase 3.50):
  - named tone-set definition lines:
    - `<name>: <tone-spec>`
    - `<name>: -`
  - timeline entries may reference named tone-sets:
    - `<HH:MM[:SS]> <name>`
    - with optional transition/interp tokens, e.g. `==` / `->` / `step`.
- Supported comments:
  - `# ...`
  - `; ...`
  - `// ...`
- Example file:
  - `examples/sbagenxlib/minimal-sbg-timing.sbg`
  - `examples/sbagenxlib/minimal-sbg-named.sbg`

Notes
-----

- This is intentionally a minimal, stable core API.
- `sbagenx` now uses sbagenxlib runtime for `-p libseq`, `-p libsbg`,
  built-in preprogram generators (`drop/sigmoid/curve/slide`), and
  compatible direct `seq-file` inputs.
- Remaining work is focused on full legacy `.sbg` path parity and legacy
  non-core voice/effect migration.

Quick Smoke Test
----------------

On Linux:

```bash
gcc -O2 -I. sbagenxlib.c examples/sbagenxlib/sbx_smoke.c -lm -o /tmp/sbx_smoke
/tmp/sbx_smoke
```

Expected output includes the library version/API plus rendered RMS values.

DSP Parity Test (Phase 2)
-------------------------

```bash
gcc -O2 -I. tests/sbagenxlib/test_dsp_parity.c -lm -o /tmp/test_sbx_dsp_parity
/tmp/test_sbx_dsp_parity
```

Expected output:

```text
PASS: sbagenxlib DSP parity checks
```

Context API Test (Phase 3)
--------------------------

```bash
gcc -O2 -I. tests/sbagenxlib/test_context_api.c sbagenxlib.c -lm -o /tmp/test_sbx_context_api
/tmp/test_sbx_context_api
```

Expected output:

```text
PASS: sbagenxlib context API checks
```

Keyframe API Test (Phase 3.2)
-----------------------------

```bash
gcc -O2 -I. tests/sbagenxlib/test_keyframes_api.c sbagenxlib.c -lm -o /tmp/test_sbx_keyframes_api
/tmp/test_sbx_keyframes_api
```

Expected output:

```text
PASS: sbagenxlib keyframe API checks
```

Sequence Loader Test (Phase 3.3)
--------------------------------

```bash
gcc -O2 -I. tests/sbagenxlib/test_sequence_loader_api.c sbagenxlib.c -lm -o /tmp/test_sbx_seq_loader
/tmp/test_sbx_seq_loader
```

Expected output:

```text
PASS: sbagenxlib sequence loader API checks
```

SBG Timing Loader Test (Phase 3.4)
----------------------------------

```bash
gcc -O2 -I. tests/sbagenxlib/test_sbg_timing_loader_api.c sbagenxlib.c -lm -o /tmp/test_sbx_sbg_loader
/tmp/test_sbx_sbg_loader
```

Expected output:

```text
PASS: sbagenxlib sbg timing loader checks
```

Transition Semantics Note (Phase 3.5)
-------------------------------------

- `step`/`hold` keeps the current keyframe tone until the next keyframe time.
- At exact keyframe boundaries, the new keyframe tone is selected.

Keyframe Access API Test (Phase 3.6)
------------------------------------

```bash
gcc -O2 -I. tests/sbagenxlib/test_keyframe_access_api.c sbagenxlib.c -lm -o /tmp/test_sbx_keyframe_access_api
/tmp/test_sbx_keyframe_access_api
```

Expected output:

```text
PASS: sbagenxlib keyframe access API checks
```

CLI Bridge Smoke Test (Phase 3.7)
---------------------------------

```bash
./dist/sbagenx-linux64 -D -p libseq examples/sbagenxlib/minimal-keyframes.sbxseq
./dist/sbagenx-linux64 -D -p libsbg examples/sbagenxlib/minimal-sbg-timing.sbg
```

Legacy Parser Bridge Smoke Test (Phase 3.41)
---------------------------------------------

```bash
tests/sbagenxlib/test_legacy_sequence_parser_bridge.sh
```

Seq Backend Named-Subset Bridge Smoke Test (Phase 3.52)
-------------------------------------------------------

```bash
tests/sbagenxlib/test_seq_backend_named_subset.sh
```

Notes:

- `libseq` reads the Phase 3.3 line format: `<time> <tone-spec> [interp]`.
- `libsbg` reads the Phase 3.4 timing subset: `<HH:MM[:SS]> <tone-spec> [interp]`.
- Optional trailing `loop` token is accepted by both bridge commands.
- `-D` now prints sbagenxlib keyframes directly (`time tone-spec interp`).
- Built-in programs (`drop/sigmoid/curve/slide`) now generate keyframes and
  render via sbagenxlib runtime in normal playback mode.
- In sbagenxlib runtime, extra tone-spec overlays that are parseable as
  sbagenxlib tones are mixed as auxiliary library contexts.
- In sbagenxlib-backed preprogram runtime, extra mix effects
  (`mixspin`/`mixpulse`/`mixbeat`) are now handled by the sbagenx adapter
  when mix input is active and `mix/<amp>` is present.
- Immediate mode (`-i`) now uses sbagenxlib runtime when all provided tone-specs
  are sbagenxlib-parseable, including adapter-handled mix effects
  (`mixspin`/`mixpulse`/`mixbeat`).
