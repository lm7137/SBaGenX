SBaGenX Library API Reference
=============================

This document is the developer-facing reference for `sbagenxlib` as shipped in
`sbagenxlib.h`.

Scope
-----

- Core engine creation and float rendering.
- Dedicated `.sbgf` curve program loading, solving, and evaluation.
- Context API for tone loading, keyframes, sequence loading, and runtime extras.
- Parser/formatter helpers used by front-ends.

Out of scope
------------

- Audio device I/O (ALSA/WASAPI/CoreAudio). The library renders samples; host
  applications own device playback.
- PNG image rendering. `sbagenxlib` exposes sampling APIs; frontends own
  plotting/visualization implementation.
- Package manager policy details. Build scripts now emit shared libraries and
  `pkg-config` metadata, but distro packaging policy remains outside the core
  API contract.

Versioning and Compatibility
----------------------------

- Runtime version string: `sbx_version()`
- API version integer: `sbx_api_version()`
- Public API version macro: `SBX_API_VERSION`

For bindings/front-ends, gate features by `sbx_api_version()` rather than
parsing version strings.

Distribution Artifacts
----------------------

Current build scripts ship these developer-facing artifacts:

- Linux:
  - `dist/libsbagenx.so.<version>` plus SONAME symlinks
  - `dist/libsbagenx-linux64.a` / `dist/libsbagenx-linux32.a`
- Windows:
  - `dist/sbagenxlib-win64.dll` / `dist/sbagenxlib-win32.dll`
  - `dist/libsbagenx-win64.dll.a` / `dist/libsbagenx-win32.dll.a`
- macOS:
  - `dist/libsbagenx.<version>.dylib` plus compatibility symlinks
- Common:
  - `dist/include/sbagenxlib.h`
  - `dist/include/sbagenlib.h` (compatibility alias)
  - `dist/pkgconfig/sbagenxlib.pc`
  - `dist/pkgconfig/sbagenxlib-uninstalled.pc`

Use `sbagenxlib-uninstalled.pc` when linking directly against the unpacked
`dist/` tree. Use `sbagenxlib.pc` after installation into `/usr` or another
prefix.

On Debian packaging, those are installed under:

- `/usr/include`
- `/usr/lib/<multiarch>`
- `/usr/lib/<multiarch>/pkgconfig`

Core Concepts
-------------

- `SbxEngine`: low-level stateless-ish renderer configured with one active tone.
- `SbxCurveProgram`: library-owned `.sbgf` curve object that can be loaded,
  parameterized, prepared, and sampled without going through the CLI.
- `SbxContext`: higher-level runtime object with load state (tone, keyframes,
  sequence data, aux tones, mix effects, mix amplitude profile, clock).
- `SbxToneSpec`: canonical tone description across binaural/monaural/isochronic/
  noise/bell/spin modes.
- `SbxPcm16DitherState`: caller-owned RNG state for TPDF dither when converting
  normalized float render output to signed 16-bit PCM.
- `SbxPcmConvertState`: generic PCM conversion state with explicit dither mode.
- `SbxProgramKeyframe`: timestamp + tone + interpolation mode to next keyframe.
- `SbxMixAmpKeyframe`: explicit `mix/<amp>` timeline point.
- `SbxTimedMixFxKeyframeInfo`: timestamp/interp metadata for one native timed
  mix-effect keyframe loaded from `.sbg`.

Status and Errors
-----------------

Most APIs return:

- `SBX_OK`
- `SBX_EINVAL`
- `SBX_ENOMEM`
- `SBX_ENOTREADY`

Helpers:

- `sbx_status_string(status)` maps codes to short text.
- `sbx_engine_last_error(eng)` and `sbx_context_last_error(ctx)` return
  object-local error text.

Threading Model
---------------

- No implicit internal threading.
- Treat each `SbxEngine`/`SbxContext` instance as single-thread-owned.
- Concurrent use is safe across different objects, not the same object.
- Callers handle external synchronization and realtime threading concerns.

Memory Ownership
----------------

- `sbx_engine_create` / `sbx_context_create` allocate; caller destroys with
  `sbx_engine_destroy` / `sbx_context_destroy`.
- Input arrays passed to setters/loaders are copied; caller retains ownership.
- Output pointers (`*_last_error`) are owned by the object and valid until the
  next mutating call or destroy.

Determinism Notes
-----------------

- For fixed inputs/configuration, output is intended to be stable.
- Cross-platform floating-point and toolchain differences may create tiny
  sample-level differences.
- No bit-identical guarantee across all platforms/build flags.

PCM Quality Notes
-----------------

`sbagenxlib` now exposes explicit float-to-PCM conversion helpers rather than
leaving 16-bit quantization entirely to host code. The default high-quality
path is TPDF-dithered 16-bit conversion.

The bundled regression tests quantify the expected tradeoff:

- total quantization noise rises slightly when dithering is enabled
- signal-correlated quantization error drops substantially
- the error spectrum becomes much flatter and less tonal

On the current low-level sine fixture in
`tests/sbagenxlib/test_pcm_quant_metrics.c`, TPDF dithering reduces absolute
error correlation with the source signal from about `0.126` to about `0.0135`,
and reduces the dominant-bin to average-bin error-spectrum ratio from about
`132x` to about `2.9x`.

That is the behavior we want: less distortion-like error, at the cost of a
slightly higher but more benign noise floor.

API Groups
----------

1) Introspection and defaults

- `sbx_version(void)`
- `sbx_api_version(void)`
- `sbx_status_string(int status)`
- `sbx_default_engine_config(SbxEngineConfig *cfg)`
- `sbx_default_tone_spec(SbxToneSpec *tone)`
- `sbx_default_pcm16_dither_state(SbxPcm16DitherState *state)`
- `sbx_seed_pcm16_dither_state(SbxPcm16DitherState *state, unsigned int seed)`
- `sbx_default_pcm_convert_state(SbxPcmConvertState *state)`
- `sbx_seed_pcm_convert_state(SbxPcmConvertState *state, unsigned int seed, int dither_mode)`

2) Engine lifecycle and render

- `sbx_engine_create(const SbxEngineConfig *cfg)`
- `sbx_engine_destroy(SbxEngine *eng)`
- `sbx_engine_reset(SbxEngine *eng)`
- `sbx_engine_set_tone(SbxEngine *eng, const SbxToneSpec *tone)`
- `sbx_engine_render_f32(SbxEngine *eng, float *out, size_t frames)`
- `sbx_engine_last_error(const SbxEngine *eng)`

`sbx_engine_render_f32` writes interleaved stereo floats to `out`.

3) PCM conversion helpers

- `sbx_convert_f32_to_s16(const float *in, short *out, size_t sample_count, SbxPcm16DitherState *dither_state)`
- `sbx_convert_f32_to_s16_ex(const float *in, int16_t *out, size_t sample_count, SbxPcmConvertState *state)`
- `sbx_convert_f32_to_s24_32(const float *in, int32_t *out, size_t sample_count, SbxPcmConvertState *state)`
- `sbx_convert_f32_to_s32(const float *in, int32_t *out, size_t sample_count, SbxPcmConvertState *state)`

This converts normalized float samples (`-1..1` nominal) to signed 16-bit PCM.
If `dither_state` is non-NULL, the helper adds TPDF dither before rounding.
Pass `NULL` to disable dithering.

The `_ex` and wider-width helpers use `SbxPcmConvertState`, which makes the
dither policy explicit:

- `SBX_PCM_DITHER_NONE`
- `SBX_PCM_DITHER_TPDF`

`sbx_convert_f32_to_s24_32` returns signed 24-bit PCM in a 32-bit container.
`sbx_convert_f32_to_s32` returns full signed 32-bit PCM. Both are useful for
host-side exporters and GUI tooling even though the render source remains
32-bit float.

4) Curve program API

- `sbx_default_curve_eval_config(SbxCurveEvalConfig *cfg)`
- `sbx_default_curve_source_config(SbxCurveSourceConfig *cfg)`
- `sbx_curve_create(void)`
- `sbx_curve_destroy(SbxCurveProgram *curve)`
- `sbx_curve_reset(SbxCurveProgram *curve)`
- `sbx_curve_load_text(SbxCurveProgram *curve, const char *text, const char *source_name)`
- `sbx_curve_load_file(SbxCurveProgram *curve, const char *path)`
- `sbx_curve_set_param(SbxCurveProgram *curve, const char *name, double value)`
- `sbx_curve_prepare(SbxCurveProgram *curve, const SbxCurveEvalConfig *cfg)`
- `sbx_curve_eval(SbxCurveProgram *curve, double t_sec, SbxCurveEvalPoint *out_point)`
- `sbx_curve_get_info(const SbxCurveProgram *curve, SbxCurveInfo *out_info)`
- `sbx_curve_param_count(const SbxCurveProgram *curve)`
- `sbx_curve_get_param(const SbxCurveProgram *curve, size_t index, const char **out_name, double *out_value)`
- `sbx_curve_source_name(const SbxCurveProgram *curve)`
- `sbx_curve_last_error(const SbxCurveProgram *curve)`

This is the library-owned `.sbgf` surface used by both the CLI `-p curve`
path and future IDE/GUI tooling.

Typical flow:

1. `sbx_curve_create`
2. `sbx_curve_load_file` or `sbx_curve_load_text`
3. optional `sbx_curve_set_param`
4. fill `SbxCurveEvalConfig` with `sbx_default_curve_eval_config`
5. `sbx_curve_prepare`
6. `sbx_curve_eval` over the desired time range

`sbx_curve_get_info`, `sbx_curve_param_count`, and `sbx_curve_get_param`
exist so hosts can build inspectors/parameter UIs without reparsing `.sbgf`
syntax themselves.

In addition to `beat`, `carrier`, `amp`, and `mixamp`, `.sbgf` can also
define runtime mix-effect parameter targets:

- `mixspin_width`, `mixspin_hz`, `mixspin_amp`
- `mixpulse_hz`, `mixpulse_amp`
- `mixbeat_hz`, `mixbeat_amp`
- `mixam_hz`

These only take effect when a matching runtime mix effect is present on the
context. They override numeric parameters, not the effect type itself.

If a host wants to render a prepared curve directly through a context rather
than sampling it manually, use:

- `sbx_context_load_curve_program(SbxContext *ctx, SbxCurveProgram *curve, const SbxCurveSourceConfig *cfg)`

That installs the prepared `.sbgf` as a runtime source owned by the context.
The curve is then evaluated at sample time through the normal context render
path, and the context reports `SBX_SOURCE_CURVE` from
`sbx_context_source_mode()`.

5) Parser/formatter helpers

- `sbx_parse_tone_spec(const char *spec, SbxToneSpec *out_tone)`
- `sbx_parse_tone_spec_ex(const char *spec, int default_waveform, SbxToneSpec *out_tone)`
- `sbx_parse_sbg_clock_token(const char *tok, size_t *out_consumed, double *out_sec)`
- `sbx_parse_mix_fx_spec(const char *spec, int default_waveform, SbxMixFxSpec *out_fx)`
- `sbx_format_mix_fx_spec(const SbxMixFxSpec *fx, char *out, size_t out_sz)`
- `sbx_parse_extra_token(...)`
- `sbx_format_tone_spec(const SbxToneSpec *tone, char *out, size_t out_sz)`

These are designed so front-ends can share parser semantics with CLI code.

6) Context lifecycle and basic load/render

- `sbx_context_create(const SbxEngineConfig *cfg)`
- `sbx_context_destroy(SbxContext *ctx)`
- `sbx_context_reset(SbxContext *ctx)`
- `sbx_context_set_tone(SbxContext *ctx, const SbxToneSpec *tone)`
- `sbx_context_set_default_waveform(SbxContext *ctx, int waveform)`
- `sbx_context_load_tone_spec(SbxContext *ctx, const char *tone_spec)`
- `sbx_context_load_curve_program(SbxContext *ctx, SbxCurveProgram *curve, const SbxCurveSourceConfig *cfg)`
- `sbx_context_render_f32(SbxContext *ctx, float *out, size_t frames)`
- `sbx_context_set_time_sec(SbxContext *ctx, double t_sec)`
- `sbx_context_time_sec(const SbxContext *ctx)`
- `sbx_context_last_error(const SbxContext *ctx)`

`sbx_context_set_time_sec` is the transport/scrubbing entry point for hosts.
It resets internal oscillator/effect phase/state and restarts playback from the
requested timeline time. That gives deterministic behavior for GUI scrubbing
and preview playback.

7) Keyframes and sequence loading

- `sbx_context_load_keyframes(SbxContext *ctx, const SbxProgramKeyframe *frames, size_t frame_count, int loop)`
- `sbx_context_load_sequence_text(SbxContext *ctx, const char *text, int loop)`
- `sbx_context_load_sequence_file(SbxContext *ctx, const char *path, int loop)`
- `sbx_context_load_sbg_timing_text(SbxContext *ctx, const char *text, int loop)`
- `sbx_context_load_sbg_timing_file(SbxContext *ctx, const char *path, int loop)`
- `sbx_context_keyframe_count(const SbxContext *ctx)`
- `sbx_context_voice_count(const SbxContext *ctx)`
- `sbx_context_source_mode(const SbxContext *ctx)`
- `sbx_context_is_looping(const SbxContext *ctx)`
- `sbx_context_get_keyframe(const SbxContext *ctx, size_t index, SbxProgramKeyframe *out)`
- `sbx_context_get_keyframe_voice(const SbxContext *ctx, size_t index, size_t voice_index, SbxProgramKeyframe *out)`
- `sbx_context_duration_sec(const SbxContext *ctx)`
- `sbx_context_set_time_sec(SbxContext *ctx, double t_sec)`
- `sbx_context_sample_tones(SbxContext *ctx, double t0_sec, double t1_sec, size_t sample_count, double *out_t_sec, SbxToneSpec *out_tones)`
- `sbx_context_sample_tones_voice(SbxContext *ctx, size_t voice_index, double t0_sec, double t1_sec, size_t sample_count, double *out_t_sec, SbxToneSpec *out_tones)`
- `sbx_context_sample_program_beat(SbxContext *ctx, double t0_sec, double t1_sec, size_t sample_count, double *out_t_sec, double *out_hz)`
- `sbx_context_sample_program_beat_voice(SbxContext *ctx, size_t voice_index, double t0_sec, double t1_sec, size_t sample_count, double *out_t_sec, double *out_hz)`

`sbx_context_get_keyframe` continues to expose the primary voice lane for
stability. Use `sbx_context_voice_count` plus `sbx_context_get_keyframe_voice`
to inspect secondary voice lanes loaded from native multivoice `.sbg` content.
Use `sbx_context_sample_tones_voice` and `sbx_context_sample_program_beat_voice`
when a frontend needs plot/preview data for those secondary lanes rather than
just keyframe inspection.

`sbx_context_voice_count` returns `1` for loaded single-voice/static contexts,
so frontends can treat voice-lane enumeration uniformly instead of special-
casing static tones as “zero lanes”.

`sbx_context_source_mode` returns one of:

- `SBX_SOURCE_NONE`
- `SBX_SOURCE_STATIC`
- `SBX_SOURCE_CURVE`
- `SBX_SOURCE_KEYFRAMES`
- `SBX_SOURCE_KEYFRAMES`

Use `sbx_context_is_looping` to decide whether keyframed transport/plotting UI
should treat the program timeline as wrapping.

7) Runtime extras (aux tones, mix effects, mix amp profile)

- `sbx_context_set_aux_tones(...)`
- `sbx_context_aux_tone_count(...)`
- `sbx_context_get_aux_tone(...)`
- `sbx_context_set_mix_effects(...)`
- `sbx_context_mix_effect_count(...)`
- `sbx_context_get_mix_effect(...)`
- `sbx_context_apply_mix_effects(...)`
- `sbx_context_set_mix_amp_keyframes(...)`
- `sbx_context_mix_amp_at(...)`
- `sbx_context_sample_mix_amp(...)`
- `sbx_context_mix_amp_keyframe_count(...)`
- `sbx_context_get_mix_amp_keyframe(...)`
- `sbx_context_has_mix_amp_control(...)`
- `sbx_context_has_mix_effects(...)`
- `sbx_context_timed_mix_effect_keyframe_count(...)`
- `sbx_context_timed_mix_effect_slot_count(...)`
- `sbx_context_get_timed_mix_effect_keyframe_info(...)`
- `sbx_context_get_timed_mix_effect_slot(...)`
- `sbx_context_sample_mix_effects(...)`
- `sbx_context_eval_active_tones(...)`
- `sbx_context_set_telemetry_callback(...)`
- `sbx_context_get_runtime_telemetry(...)`
- `sbx_context_mix_stream_sample(...)`
- `sbx_context_configure_runtime(...)`

`sbx_context_configure_runtime` is the one-call setup path for mix keyframes,
mix effects, and auxiliary tones.

`sbx_context_has_mix_amp_control` and `sbx_context_has_mix_effects` are useful
when a frontend has loaded native `.sbg`/`libsbg` content and needs to know
whether the loaded context depends on an external mix stream.

For deeper native `.sbg` inspection, use:

- `sbx_context_mix_amp_keyframe_count(...)`
- `sbx_context_get_mix_amp_keyframe(...)`
- `sbx_context_timed_mix_effect_keyframe_count(...)`
- `sbx_context_timed_mix_effect_slot_count(...)`
- `sbx_context_get_timed_mix_effect_keyframe_info(...)`
- `sbx_context_get_timed_mix_effect_slot(...)`

Those let a frontend inspect the exact `mix/<amp>` timeline and the timed
mix-effect slots loaded from native `.sbg` content, without reverse-engineering
them from render output.

`sbx_context_sample_mix_amp(...)` is the range-sampling companion to
`sbx_context_mix_amp_at(...)`. Use it when a frontend wants a full amplitude
curve for plotting or inspection.

`sbx_context_sample_mix_effects(...)` evaluates the effective mix-effect chain
at one timeline time. It returns:

- static runtime mix effects first,
- then evaluated timed native `.sbg` mix-effect slots.

For curve-backed contexts, any matching `.sbgf` mix-effect targets are already
applied to those returned specs at the requested time.

Entries with `type == SBX_MIXFX_NONE` represent empty timed slots.

8) Plot/data sampling support

- `sbx_context_sample_tones(...)`
- `sbx_context_sample_tones_voice(...)`
- `sbx_context_sample_program_beat(...)`
- `sbx_context_sample_program_beat_voice(...)`
- `sbx_context_eval_active_tones(...)`
- `sbx_sample_mixam_cycle(...)`
- `sbx_sample_isochronic_cycle(...)`
- `sbx_default_iso_envelope_spec(...)`

`sbx_context_sample_tones` evaluates the currently loaded source over a caller
provided time range (`[t0_sec, t1_sec]`) into `sample_count` tone samples.

- Works for static tone and keyframed sources.
- For looped keyframes, evaluation wraps to program duration.
- Does not advance context render clock (`sbx_context_time_sec`).
- Returns optional sample-time output via `out_t_sec` when non-NULL.

`sbx_context_sample_program_beat` samples the effective program beat/pulse
frequency over a time range using the same keyframe evaluation logic as the
runtime adapter. This is the preferred frontend API for beat-vs-time graphs.

`sbx_context_sample_tones_voice` and `sbx_context_sample_program_beat_voice`
do the same work for one specific voice lane. Voice lane `0` is the primary
lane; higher indices address secondary lanes loaded from native multivoice
`.sbg` content.

`sbx_context_eval_active_tones` is the point-time snapshot helper for the same
surface. It returns the effective voice lanes first, then any configured
auxiliary tones. Use it when a frontend needs a “current sounding tones” view
for inspectors, live labels, or non-buffered previews.

`sbx_context_set_telemetry_callback` registers an optional callback that emits
one `SbxRuntimeTelemetry` snapshot per `sbx_context_render_f32` call. Use
`sbx_context_get_runtime_telemetry` for pull-style snapshots at the current
context time when callbacks are not desired.

`sbx_context_sample_mix_amp` samples the effective `mix/<amp>` profile over a
time range without advancing render time.

`sbx_sample_mixam_cycle` samples one cycle of a `mixam` envelope plus the
derived gain curve (`f + (1-f) * envelope`) for plotting/inspection.

`sbx_sample_isochronic_cycle` samples one isochronic cycle into envelope and
waveform arrays. If no explicit `SbxIsoEnvelopeSpec` is provided, the helper
uses the library runtime defaults (`start=0`, `duty=tone->duty_cycle`,
`attack=0.15`, `release=0.15`, `edge=2`).

Minimal Lifecycle
-----------------

1. Fill `SbxEngineConfig` with `sbx_default_engine_config`.
2. Create context with `sbx_context_create`.
3. Load one of:
   - tone spec,
   - keyframes,
   - sequence text/file,
   - SBG timing text/file.
4. (Optional) configure runtime extras.
5. Render blocks with `sbx_context_render_f32`.
6. Destroy context.

Tone/Token Surface (Current)
----------------------------

Supported tone forms include:

- binaural: `<carrier>+<beat>/<amp>` and `<carrier>-<beat>/<amp>`
- monaural: `<carrier>M<beat>/<amp>`
- isochronic: `<carrier>@<pulse>/<amp>`
- bell: `bell<carrier>/<amp>`
- noise: `white/<amp>`, `pink/<amp>`, `brown/<amp>`
- spin noise: `spin:<width-us><spin-hz>/<amp>`, `bspin:...`, `wspin:...`
- off: `-`
- waveform prefixes for tonal forms: `sine:`, `square:`, `triangle:`, `sawtooth:`

`libsbg` subset loader currently supports HH:MM[:SS], NOW/relative forms,
named tone-sets, block definitions, and nested block invocation.

Current native `.sbg` notes:

- direct timeline entries and block entries are not artificially capped at six
  whitespace tokens; they scale to the library's native multivoice/mix-slot
  limits,
- native-loaded `.sbg` content may include explicit mix control/effects, and
  frontends can detect that through
  `sbx_context_has_mix_amp_control()` / `sbx_context_has_mix_effects()`,
- native-loaded `.sbg` mix timelines can also be inspected directly through
  `sbx_context_mix_amp_keyframe_count()` /
  `sbx_context_get_mix_amp_keyframe()` and
  `sbx_context_timed_mix_effect_keyframe_count()` /
  `sbx_context_timed_mix_effect_slot_count()` /
  `sbx_context_get_timed_mix_effect_keyframe_info()` /
  `sbx_context_get_timed_mix_effect_slot()`,
- native-loaded multivoice `.sbg` content can be inspected lane-by-lane through
  `sbx_context_voice_count()` / `sbx_context_get_keyframe_voice()`.

See Also
--------

- `docs/SBAGENXLIB.md` (migration roadmap and parity notes)
- `docs/SBAGENXLIB_QUICKSTART.md` (fastest path)
- `docs/SBAGENXLIB_DOTNET_INTEROP.md` (P/Invoke guidance)
