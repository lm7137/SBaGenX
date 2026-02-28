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

Phase 3.53
- Improve sbagenxlib SBG named-subset discoverability:
  - add `examples/sbagenxlib/minimal-sbg-named.sbg`,
  - surface named-subset example in CLI `-p` usage examples and docs.

Phase 3.54 (current slice)
- Extend sbagenxlib SBG timing subset with legacy-style timeline token forms:
  - support `NOW` timeline anchors,
  - support relative `+HH:MM[:SS]` timeline offsets using last-absolute-time
    semantics compatible with legacy sequence parsing.

Phase 3.55
- Extend sbagenxlib SBG timing subset with block definitions and expansion:
  - support block definitions: `<name>: { ... }`,
  - support timeline block invocation: `<time-spec> <block-name>`,
  - require block-internal time tokens to be relative (`+HH:MM[:SS][+...]`).

Phase 3.56
- Extend sbagenxlib block subset with nested block invocation:
  - allow block entries to invoke previously defined blocks,
  - reject self-referential block recursion,
  - preserve deterministic non-decreasing expanded block timing.

Phase 3.57
- Add first-class developer API reference for `sbagenxlib`:
  - `docs/SBAGENXLIB_API.md` as canonical function/ownership/error/threading
    reference for front-end and binding authors.

Phase 3.58
- Add onboarding docs for frontend/binding developers:
  - `docs/SBAGENXLIB_QUICKSTART.md` (5-minute compile/load/render path),
  - `docs/SBAGENXLIB_DOTNET_INTEROP.md` (P/Invoke interop guidance).

Phase 3.59
- Make `sbagenxlib.h` self-documenting with grouped public API comments to
  keep markdown/docs and header-level API intent aligned.

Phase 3.60
- Link sbagenxlib developer docs from `README.md` so integrators can find
  API/quickstart/interop references directly from the project landing page.

Phase 3.61
- Add context tone-sampling API for plotting/frontends:
  - `sbx_context_sample_tones()` samples evaluated tone values over a caller
    requested time range,
  - supports static and keyframed sources (including looped keyframes),
  - enables GUI/host plotting without duplicating curve math outside library.

Phase 3.62
- Extend the native `.sbg` timing loader with multivoice named tone-sets:
  - allow up to 16 tone tokens on `<name>: ...` definition lines,
  - preserve those voice lanes through block expansion and runtime render,
  - keep the existing public single-keyframe API stable by exposing the primary
    voice in `sbx_context_get_keyframe()` while render mixes every active lane.

Phase 3.63
- Add native `waveNN` support to `sbagenxlib` `.sbg` loading:
  - accept legacy waveform definition lines such as `wave00: 0 1 0 ...`,
  - allow `waveNN:` tone tokens in native `.sbg` timing/name/block parsing,
  - render those tones as custom-envelope binaural voices without falling back
    to the legacy parser/runtime.

Phase 3.64
- Extend native `.sbg` loading with keyframed mix-side tokens:
  - accept legacy `mix/<amp>` plus `mixspin` / `mixpulse` / `mixbeat` /
    `mixam` tokens in named tone-sets, timeline entries, and block entries,
  - allow those tokens to coexist with normal tone tokens on the same native
    `.sbg` line,
  - evaluate `mix/<amp>` and mix effects over time inside `sbagenxlib`
    instead of dropping back to the legacy CLI runtime.

Phase 3.65
- Remove the native `.sbg` loader's fixed six-token ceiling for timeline
  entries and block entries.
- Let direct `.sbg` token lists scale to the same multivoice/mix-slot limits
  already enforced by `SBX_MAX_SBG_VOICES` and `SBX_MAX_SBG_MIXFX`.
- Add loader and CLI bridge regression coverage for long multivoice lines so
  real `.sbg` content is no longer rejected by an artificial parser limit.

Phase 3.66
- Add library-side mix-content introspection:
  - `sbx_context_has_mix_amp_control()`
  - `sbx_context_has_mix_effects()`
- Use those hooks in the CLI/runtime bridge so native-loaded `.sbg` / `libsbg`
  content gets the same mix-stream validation path as legacy sequence loading.
- Add regression coverage for native `sbagenxlib` sequence files that carry
  mix effects but are run without an active mix input stream.

Phase 3.67
- Add public multivoice keyframe introspection:
  - `sbx_context_voice_count()`
  - `sbx_context_get_keyframe_voice()`
- Keep the existing primary-keyframe API stable while letting frontends inspect
  secondary voice lanes loaded from native multivoice `.sbg` content.
- Extend `sbagenx -D` on sbagenxlib-backed paths to emit extra comment lines
  for secondary voice lanes, improving observability without breaking the
  existing primary dump line format.

Phase 3.68
- Add public native mix-timeline introspection:
  - `sbx_context_mix_amp_keyframe_count()`
  - `sbx_context_get_mix_amp_keyframe()`
  - `sbx_context_timed_mix_effect_keyframe_count()`
  - `sbx_context_timed_mix_effect_slot_count()`
  - `sbx_context_get_timed_mix_effect_keyframe_info()`
  - `sbx_context_get_timed_mix_effect_slot()`
- Expose the actual library-side `mix/<amp>` and timed `mixbeat` /
  `mixpulse` / `mixspin` / `mixam` timelines loaded from native `.sbg`
  content instead of forcing frontends to infer them from render behavior.
- Extend `sbagenx -D` on sbagenxlib-backed paths to emit comment lines for
  native mix keyframes and timed mix-effect slots, matching the observability
  approach already used for multivoice lanes.

Phase 3.69
- Extend the direct sequence-file sbagenxlib bridge with a safe legacy option
  preamble subset:
  - accept leading `-S`, `-E`, `-SE`, `-T <time>`, and `-m <file>` lines
    before native `.sbg` content,
  - strip those lines before handing the remaining sequence text to
    `sbagenxlib`,
  - apply their runtime meaning in the CLI bridge only after native loading
    succeeds.
- This keeps file-level execution controls (`-SE`, `-T`, mix input selection)
  in the frontend layer while letting many historical `.sbg` examples route
  through the library without falling back to the legacy parser.

Phase 3.70
- Add a real-example native bridge smoke corpus for `.sbg` files already known
  to route through `sbagenxlib`.
- Cover representative historical examples for:
  - multivoice named tone-sets,
  - legacy `waveNN` custom envelopes,
  - direct example files shipped in the repository.
- Use this corpus as the regression floor while investigating the remaining
  historical examples that still require fallback.

Phase 3.71
- Extend the direct sequence-file bridge with option-only wrapper handling:
  - accept historical `.sbg` files whose non-comment payload is only one or
    more option lines,
  - if one of those lines contains `-p` or `-i`, dispatch through the existing
    CLI preprogram/immediate path instead of rejecting the file as
    "not compatible with sbagenxlib subset loader".
- This unlocks wrapper examples such as `prog-drop-00d.sbg` and
  `prog-slide-alpha-10.sbg`, whose generated runtime is already backed by
  `sbagenxlib`.

Phase 3.72
- Harden the direct sequence-file bridge's safe legacy preamble stripping:
  - treat each candidate preamble line as a real single logical line before
    handing it to the safe-option parser,
  - preserve the original buffer after inspection so the remaining native
    `.sbg` text can still be loaded verbatim,
  - inspect option-only wrapper files in two passes so non-wrapper files do
    not execute CLI options speculatively during classification,
  - cover both split and inline forms such as `-SE`, `-T ...`, and
    `-SE -m river1.ogg`.
- This closes a real bridge defect where some historical files with valid
  leading option lines were being rejected before `sbagenxlib` even saw the
  native sequence body. Representative examples such as
  `ch-awakened-mind.sbg`, `ch-aspirin.sbg`, and `prog-drop-old-demo.sbg`
  now route through the native sbagenxlib path.

Phase 3.73
- Add day-wrap semantics to native `.sbg` timing clocks:
  - plain `HH:MM[:SS]` timeline entries are now lifted into a monotonically
    increasing absolute timeline,
  - when a later clock token is numerically earlier than the previous absolute
    time, it is treated as the next day rather than as an error,
  - top-level block expansion now keeps absolute time instead of wrapping
    expanded entries modulo 24 hours.
- This matches the historical `.sbg` behavior used by overnight and
  around-midnight example files, and it retires the largest remaining
  incompatibility bucket in the forced-native corpus.

Phase 3.74
- Relax native keyframe validation from strictly increasing timestamps to
  non-decreasing timestamps:
  - preserve adjacent equal-time keyframes loaded from historical `.sbg`
    files,
  - keep the evaluation rules already present for zero-duration segments,
  - apply the same tolerance to derived native mix timelines.
- This restores another legacy sequencing pattern used in historical block and
  chord-transition examples, where one state ends and the next begins at the
  same wall-clock instant.

Phase 3.75
- Normalize two remaining historical tone-spec idioms in native `.sbg` loads:
  - accept signed `bell` carriers such as `bell-4000/30` by preserving the
    magnitude and discarding the sign,
  - treat `0+0/0` placeholder voice lanes as explicit silence rather than as
    invalid binaural tones.
- This restores compatibility with older multivoice and bell-heavy example
  files that use these shorthand forms for structural reasons rather than for
  distinct DSP behavior.

Phase 3.76
- Extend the safe sequence-file preamble bridge to accept additional runtime
  options that occur in historical `.sbg` wrappers:
  - `-r <rate>`
  - `-F <fade-ms>`
  - `-q <mult>`
- Their effects remain in the CLI/frontend layer, but they no longer force a
  fallback away from native sbagenxlib loading.

Phase 3.77
- Promote the forced-native `.sbg` example sweep to a first-class regression
  test:
  - iterate across every `.sbg` example shipped in the repository,
  - force `SBAGENX_SEQ_BACKEND=sbagenxlib`,
  - require the direct `-D` path to succeed for the full corpus.
- Current milestone at the time of landing: `150/150` repository examples
  route through `sbagenxlib` without falling back to the legacy parser.

Phase 3.78
- Promote `sbagenxlib` from a static-only build byproduct to a consumable
  developer/runtime artifact:
  - Linux build now emits `libsbagenx.so.<version>` plus SONAME symlinks,
  - Windows build now emits `sbagenxlib-*.dll` plus import libraries,
  - macOS build now emits a versioned `.dylib`,
  - all build scripts now bundle `dist/pkgconfig/sbagenxlib.pc`.
- Extend Debian packaging so the generated `.deb` installs:
  - the CLI binary,
  - the shared and static `sbagenxlib` artifacts,
  - public headers,
  - multiarch `pkg-config` metadata.
- Add regression checks for shipped shared-library artifacts and Debian
  package payload contents.

Phase 3.79
- Add library-native plotting sample APIs so frontends can obtain plot data
  directly from `sbagenxlib` instead of shelling out to the CLI or reimplementing
  the plot math:
  - `sbx_context_sample_program_beat(...)`
  - `sbx_sample_mixam_cycle(...)`
  - `sbx_sample_isochronic_cycle(...)`
  - `sbx_default_iso_envelope_spec(...)`
- Refactor mixam envelope/gain evaluation into reusable pure helpers shared by
  runtime DSP and the new cycle-sampling API.
- Add direct regression coverage for program-beat sampling, raised-cosine
  mixam sampling, and isochronic single-cycle sampling.

Phase 3.80
- Add explicit transport/seek support for frontend hosts:
  - `sbx_context_set_time_sec(...)`
- Seeking resets internal oscillator/effect phase/state and restarts playback
  from the requested timeline time. This is the right contract for GUI
  scrubbing and transport controls because it is deterministic and does not
  depend on preserving hidden runtime integrator state from earlier playback.
- Add regression coverage for:
  - seeking on static-tone contexts,
  - seeking on loaded keyframe programs,
  - invalid negative seek rejection.

Phase 4
- Add optional bindings/frontends (Python, GUI, plugin/service use-cases).

Current API (Phase 3.80 Slice)
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
  - `sbx_context_mix_amp_keyframe_count()`
  - `sbx_context_get_mix_amp_keyframe()`
  - `sbx_context_has_mix_amp_control()`
  - `sbx_context_has_mix_effects()`
  - `sbx_context_timed_mix_effect_keyframe_count()`
  - `sbx_context_timed_mix_effect_slot_count()`
  - `sbx_context_get_timed_mix_effect_keyframe_info()`
  - `sbx_context_get_timed_mix_effect_slot()`
  - `sbx_context_keyframe_count()`
  - `sbx_context_voice_count()`
  - `sbx_context_get_keyframe()`
  - `sbx_context_get_keyframe_voice()`
  - `sbx_context_duration_sec()`
  - `sbx_context_set_time_sec()`
  - `sbx_context_sample_tones()`
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
    - `<name>: <tone-spec> [<tone-spec> ...]`
    - `<name>: - [<tone-spec> ...]`
    - up to 16 voice slots are preserved in definition order (Phase 3.62)
    - native `.sbg` named tone-sets may also include:
      - `mix/<amp>`
      - `mixspin:<width><spin>/<amp>`
      - `mixpulse:<pulse>/<amp>`
      - `mixbeat:<beat>/<amp>`
      - `mixam:<rate-or-beat>[:params]`
  - legacy waveform definition lines (Phase 3.63):
    - `waveNN: <sample0> <sample1> ...`
    - at least two non-identical floating-point samples are required
    - `waveNN:` tone tokens are rendered as custom-envelope binaural tones
  - timeline entries may reference named tone-sets:
    - `<HH:MM[:SS]> <name>`
    - with optional transition/interp tokens, e.g. `==` / `->` / `step`.
  - direct timeline and block-entry token lists may mix normal tone tokens
    with native `mix/<amp>` / mix-effect tokens on the same line (Phase 3.64).
  - direct timeline and block-entry tokenization is no longer capped at six
    whitespace tokens; long multivoice lines now scale to the native loader's
    voice/effect capacities (Phase 3.65).
- Extended timeline time forms (Phase 3.54):
  - `NOW`
  - `NOW+HH:MM[:SS][+HH:MM[:SS]...]`
  - `+HH:MM[:SS]` (relative to previous absolute/NOW base line)
- Block definition subset (Phase 3.55):
  - define block:
    - `<block-name>: {`
    - `  +HH:MM[:SS][+...] <tone-spec-or-named-tone> [transition/interp]`
    - `  ...`
    - `}`
  - invoke block:
    - `<time-spec> <block-name>`
  - block entry timings must be non-decreasing and relative (`+...`).
  - block entries may invoke previously defined blocks (Phase 3.56).
- Supported comments:
  - `# ...`
  - `; ...`
  - `// ...`
- Example file:
  - `examples/sbagenxlib/minimal-sbg-timing.sbg`
  - `examples/sbagenxlib/minimal-sbg-named.sbg`
  - `examples/sbagenxlib/minimal-sbg-block.sbg`
  - `examples/sbagenxlib/minimal-sbg-nested-block.sbg`
  - `examples/sbagenxlib/minimal-sbg-multivoice.sbg`
  - `examples/sbagenxlib/minimal-sbg-wave-custom.sbg`
  - `examples/sbagenxlib/minimal-sbg-mixfx.sbg`

Notes
-----

- This is intentionally a minimal, stable core API.
- `sbagenx` now uses sbagenxlib runtime for `-p libseq`, `-p libsbg`,
  built-in preprogram generators (`drop/sigmoid/curve/slide`), and
  compatible direct `seq-file` inputs.
- Native `.sbg` timing support now includes multivoice named tone-sets,
  block-preserved voice lanes, legacy `waveNN` custom-envelope definitions,
  and keyframed mix-side tokens (`mix/<amp>`, `mixspin`, `mixpulse`,
  `mixbeat`, `mixam`).
- Direct timeline entries and block entries are no longer capped at six
  whitespace tokens; they now scale to the native multivoice/mix-slot
  capacities enforced by `SBX_MAX_SBG_VOICES` and `SBX_MAX_SBG_MIXFX`.
- Library consumers can now ask whether a loaded context carries explicit
  mix-level control or mix-effect content via
  `sbx_context_has_mix_amp_control()` and `sbx_context_has_mix_effects()`.
  The `sbagenx` CLI uses those hooks so native-loaded `.sbg`/`libsbg`
  content gets the same mix-stream validation path as the legacy runtime.
- Native mix effects in `.sbg` require explicit `mix/<amp>` control on the
  same line or in the referenced named tone-set, matching the existing legacy
  expectation that mix effects are anchored to a declared mix stream level.
- Once native `.sbg` loading sees any `mix/<amp>` token, frames without an
  explicit/referenced `mix/<amp>` are treated as mix-off in the native runtime.
- Remaining work is focused on broader real-world `.sbg` parity validation and
  the historical edge cases that still require CLI fallback.
- `sbx_context_get_keyframe()` intentionally exposes the primary voice lane so
  existing frontends/tests keep a stable view while runtime render mixes all
  active multivoice lanes loaded from `.sbg`.
- `sbx_context_voice_count()` / `sbx_context_get_keyframe_voice()` expose those
  secondary lanes explicitly for frontends, tests, and richer native dump paths.
- `sbx_context_mix_amp_keyframe_count()` /
  `sbx_context_get_mix_amp_keyframe()` expose the explicit native `mix/<amp>`
  timeline loaded from `.sbg`.
- `sbx_context_timed_mix_effect_keyframe_count()` /
  `sbx_context_timed_mix_effect_slot_count()` /
  `sbx_context_get_timed_mix_effect_keyframe_info()` /
  `sbx_context_get_timed_mix_effect_slot()` expose the native timed mix-effect
  slots loaded from `.sbg`, including empty slot detection for sparse frames.

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

Plot Sampling API Test (Phase 3.61)
-----------------------------------

```bash
gcc -O2 -I. tests/sbagenxlib/test_plot_sampling_api.c sbagenxlib.c -lm -o /tmp/test_sbx_plot_sampling_api
/tmp/test_sbx_plot_sampling_api
```

Expected output:

```text
PASS: sbagenxlib plot sampling API checks
```

CLI Bridge Smoke Test (Phase 3.7)
---------------------------------

```bash
./dist/sbagenx-linux64 -D -p libseq examples/sbagenxlib/minimal-keyframes.sbxseq
./dist/sbagenx-linux64 -D -p libsbg examples/sbagenxlib/minimal-sbg-timing.sbg
./dist/sbagenx-linux64 -D -p libsbg examples/sbagenxlib/minimal-sbg-block.sbg
./dist/sbagenx-linux64 -D -p libsbg examples/sbagenxlib/minimal-sbg-nested-block.sbg
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

Seq Backend Block-Subset Bridge Smoke Test (Phase 3.55)
-------------------------------------------------------

```bash
tests/sbagenxlib/test_seq_backend_block_subset.sh
```

Seq Backend Nested-Block Subset Bridge Smoke Test (Phase 3.56)
--------------------------------------------------------------

```bash
tests/sbagenxlib/test_seq_backend_nested_block_subset.sh
```

Seq Backend Many-Token Subset Bridge Smoke Test (Phase 3.65)
------------------------------------------------------------

```bash
tests/sbagenxlib/test_seq_backend_many_tokens_subset.sh
```

Seq Backend MixFX-Requires-Mix Smoke Test (Phase 3.66)
------------------------------------------------------

```bash
tests/sbagenxlib/test_seq_backend_mixfx_requires_mix.sh
```

Seq Backend Multivoice Dump Smoke Test (Phase 3.67)
---------------------------------------------------

```bash
tests/sbagenxlib/test_seq_backend_multivoice_dump.sh
```

Seq Backend Native Mix Dump Smoke Test (Phase 3.68)
---------------------------------------------------

```bash
tests/sbagenxlib/test_seq_backend_mix_dump.sh
```

Seq Backend Safe-Preamble Bridge Smoke Test (Phase 3.69)
--------------------------------------------------------

```bash
tests/sbagenxlib/test_seq_backend_safe_preamble_subset.sh
```

Seq Backend Real-Example Corpus Smoke Test (Phase 3.70)
-------------------------------------------------------

```bash
tests/sbagenxlib/test_seq_backend_real_examples_subset.sh
```

Seq Backend Option-Wrapper Example Smoke Test (Phase 3.71)
----------------------------------------------------------

```bash
tests/sbagenxlib/test_seq_backend_option_wrapper_examples.sh
```

Seq Backend Safe-Preamble Bridge Smoke Test (Phase 3.72)
--------------------------------------------------------

```bash
tests/sbagenxlib/test_seq_backend_safe_preamble_subset.sh
```

Seq Backend Real-Example Corpus Smoke Test (Phase 3.73)
-------------------------------------------------------

```bash
tests/sbagenxlib/test_seq_backend_real_examples_subset.sh
tests/sbagenxlib/test_sbg_timing_loader_api.c
```

Seq Backend Real-Example Corpus Smoke Test (Phase 3.74)
-------------------------------------------------------

```bash
tests/sbagenxlib/test_seq_backend_real_examples_subset.sh
tests/sbagenxlib/test_sbg_timing_loader_api.c
```

Seq Backend Safe-Preamble Bridge Smoke Test (Phase 3.76)
--------------------------------------------------------

```bash
tests/sbagenxlib/test_seq_backend_safe_preamble_subset.sh
tests/sbagenxlib/test_sbg_timing_loader_api.c
```

Seq Backend Full Example Corpus Smoke Test (Phase 3.77)
-------------------------------------------------------

```bash
tests/sbagenxlib/test_seq_backend_full_example_corpus.sh
```

Shared Library Artifact Smoke Test (Phase 3.78)
-----------------------------------------------

```bash
tests/sbagenxlib/test_dist_shared_library_artifacts.sh
tests/sbagenxlib/test_deb_library_payload.sh
```

Plot Sampling API Smoke Test (Phase 3.79)
-----------------------------------------

```bash
gcc -O2 -I. tests/sbagenxlib/test_plot_sampling_api.c sbagenxlib.c -lm -o /tmp/test_plot_sampling_api
/tmp/test_plot_sampling_api
```

Notes:

- `libseq` reads the Phase 3.3 line format: `<time> <tone-spec> [interp]`.
- `libsbg` reads the Phase 3.4+ timing subset (including named tone-sets,
  `NOW`/relative timeline forms, and block definitions/invocations).
- Optional trailing `loop` token is accepted by both bridge commands.
- Direct `seq-file` inputs routed through sbagenxlib now also accept a safe
  legacy file preamble subset (`-S`, `-E`, `-SE`, `-T`, `-m`) before the first
  native sequence line.
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

Developer docs:

- `docs/SBAGENXLIB_API.md`
- `docs/SBAGENXLIB_QUICKSTART.md`
- `docs/SBAGENXLIB_DOTNET_INTEROP.md`
