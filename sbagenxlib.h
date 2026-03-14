#ifndef SBAGENXLIB_H
#define SBAGENXLIB_H

/*
 * sbagenxlib public C API.
 *
 * The library renders stereo float audio from tone specs or loaded keyframed
 * programs. Device I/O and container encoding remain host responsibilities.
 */

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SBX_API_VERSION 22  /* public API contract revision */
#define SBX_MAX_AUX_TONES 16 /* max auxiliary overlay tones */

/* Status codes returned by sbagenxlib APIs. */
enum {
  SBX_OK = 0,
  SBX_EINVAL = 1,
  SBX_ENOMEM = 2,
  SBX_ENOTREADY = 3
};

typedef enum {
  SBX_TONE_NONE = 0,
  SBX_TONE_BINAURAL = 1,
  SBX_TONE_MONAURAL = 2,
  SBX_TONE_ISOCHRONIC = 3,
  SBX_TONE_WHITE_NOISE = 4,
  SBX_TONE_PINK_NOISE = 5,
  SBX_TONE_BROWN_NOISE = 6,
  SBX_TONE_SPIN_PINK = 7,
  SBX_TONE_SPIN_BROWN = 8,
  SBX_TONE_SPIN_WHITE = 9,
  SBX_TONE_BELL = 10
} SbxToneMode;

typedef enum {
  SBX_WAVE_SINE = 0,
  SBX_WAVE_SQUARE = 1,
  SBX_WAVE_TRIANGLE = 2,
  SBX_WAVE_SAWTOOTH = 3,
  SBX_WAVE_CUSTOM_BASE = 1000 /* deprecated legacy placeholder; use SBX_ENV_WAVE_* for custom envelopes */
} SbxWaveform;

typedef enum {
  SBX_ENV_WAVE_NONE = 0,
  SBX_ENV_WAVE_LEGACY_BASE = 2000, /* legacy waveNN envelope id: + [0..99] */
  SBX_ENV_WAVE_CUSTOM_BASE = 2100  /* literal customNN envelope id: + [0..99] */
} SbxEnvelopeWaveform;

typedef enum {
  SBX_INTERP_LINEAR = 0,
  SBX_INTERP_STEP = 1
} SbxInterpMode;

typedef enum {
  SBX_SOURCE_NONE = 0,
  SBX_SOURCE_STATIC = 1,
  SBX_SOURCE_KEYFRAMES = 2,
  SBX_SOURCE_CURVE = 3
} SbxSourceMode;

typedef enum {
  SBX_MIXFX_NONE = 0,
  SBX_MIXFX_SPIN = 1,
  SBX_MIXFX_PULSE = 2,
  SBX_MIXFX_BEAT = 3,
  SBX_MIXFX_AM = 4
} SbxMixFxType;

typedef enum {
  SBX_MIXAM_MODE_PULSE = 0,
  SBX_MIXAM_MODE_COS = 1
} SbxMixamMode;

typedef struct {
  int type;      /* SBX_MIXFX_* */
  int waveform;  /* SBX_WAVE_* */
  double carr;   /* mixspin width in microseconds */
  double res;    /* modulation/spin frequency in Hz */
  double amp;    /* 0..1 effect amount */
  /* mixam envelope controls (all cycle-relative 0..1 unless noted). */
  int mixam_mode;        /* m: 0 pulse (d/a/r/e active), 1 raised-cosine (d/a/r/e ignored) */
  double mixam_start;    /* s: cycle phase offset */
  double mixam_duty;     /* d: on-window duty */
  double mixam_attack;   /* a: attack share of on-window */
  double mixam_release;  /* r: release share of on-window */
  int mixam_edge_mode;   /* e: 0 hard, 1 linear, 2 smoothstep, 3 smootherstep */
  double mixam_floor;    /* f: minimum gain floor */
  int mixam_bind_program_beat; /* 1 => AM rate follows current program beat */
} SbxMixFxSpec;

typedef struct {
  double time_sec;
  double amp_pct;
  int interp; /* SBX_INTERP_* */
} SbxMixAmpKeyframe;

typedef struct {
  int active;            /* 1 => enabled, 0 => disabled */
  double delta;          /* dip depth parameter */
  double epsilon;        /* Gaussian width parameter */
  double period_sec;     /* half-period spacing in seconds */
  double end_level;      /* terminal linear level at end of main phase, 0..1 */
  double main_len_sec;   /* main phase duration in seconds (drop+hold) */
  double wake_len_sec;   /* wake phase duration in seconds */
  int wake_enabled;      /* 1 => include wake phase ramp */
} SbxMixModSpec;

typedef struct {
  double time_sec;
  int interp; /* SBX_INTERP_* */
} SbxTimedMixFxKeyframeInfo;

typedef enum {
  SBX_EXTRA_INVALID = 0,
  SBX_EXTRA_MIXAMP = 1,
  SBX_EXTRA_TONE = 2,
  SBX_EXTRA_MIXFX = 3
} SbxExtraTokenType;

typedef struct {
  double sample_rate; /* Hz, e.g. 44100 */
  int channels;       /* currently 2 (stereo) */
} SbxEngineConfig;

typedef struct {
  SbxToneMode mode;
  double carrier_hz;
  double beat_hz;
  double amplitude;  /* 0.0 .. 1.0 */
  int waveform;      /* SBX_WAVE_* carrier waveform */
  int envelope_waveform; /* SBX_ENV_WAVE_NONE or SBX_ENV_WAVE_* + [0..99] */
  double duty_cycle; /* for isochronic mode: 0.0 .. 1.0 (default 0.403014) */
  double iso_start;   /* cycle-relative start phase for isochronic mode (default 0.048493) */
  double iso_attack;  /* attack share of isochronic on-window (default 0.5) */
  double iso_release; /* release share of isochronic on-window (default 0.5) */
  int iso_edge_mode;  /* 0 hard, 1 linear, 2 smoothstep, 3 smootherstep */
} SbxToneSpec;

typedef struct {
  double start;   /* cycle-relative start phase (0..1) */
  double duty;    /* cycle-relative on-window width (0..1) */
  double attack;  /* attack share of on-window (0..1) */
  double release; /* release share of on-window (0..1) */
  int edge_mode;  /* 0 hard, 1 linear, 2 smoothstep, 3 smootherstep */
} SbxIsoEnvelopeSpec;

typedef struct {
  double time_sec; /* keyframe timestamp, >= 0, increasing */
  SbxToneSpec tone;
  int interp; /* interpolation mode to next keyframe: SBX_INTERP_* */
} SbxProgramKeyframe;

typedef struct SbxEngine SbxEngine;
typedef struct SbxContext SbxContext;
typedef struct SbxCurveProgram SbxCurveProgram;

typedef struct {
  double carrier_start_hz;
  double carrier_end_hz;
  double carrier_span_sec;
  double beat_start_hz;
  double beat_target_hz;
  double beat_span_sec;
  double hold_min;
  double total_min;
  double wake_min;
  double beat_amp0_pct;
  double mix_amp0_pct;
} SbxCurveEvalConfig;

typedef struct {
  double beat_hz;
  double carrier_hz;
  double beat_amp_pct;
  double mix_amp_pct;
} SbxCurveEvalPoint;

typedef struct {
  size_t parameter_count;
  int has_solve;
  int has_carrier_expr;
  int has_amp_expr;
  int has_mixamp_expr;
  size_t beat_piece_count;
  size_t carrier_piece_count;
  size_t amp_piece_count;
  size_t mixamp_piece_count;
} SbxCurveInfo;

typedef struct {
  SbxToneMode mode;
  int waveform;
  double duty_cycle;
  double iso_start;
  double iso_attack;
  double iso_release;
  int iso_edge_mode;
  double amplitude;
  double duration_sec;
  int loop;
} SbxCurveSourceConfig;

typedef struct {
  unsigned int rng_state; /* caller-owned RNG state for TPDF dithering */
} SbxPcm16DitherState;

typedef enum {
  SBX_PCM_DITHER_NONE = 0,
  SBX_PCM_DITHER_TPDF = 1
} SbxPcmDitherMode;

typedef struct {
  unsigned int rng_state; /* caller-owned RNG state for quantization helpers */
  int dither_mode;        /* SBX_PCM_DITHER_* */
} SbxPcmConvertState;

typedef struct {
  double time_sec;            /* context timeline time at snapshot */
  int source_mode;            /* SBX_SOURCE_* */
  SbxToneSpec primary_tone;   /* evaluated primary tone at time_sec */
  double program_beat_hz;     /* convenience mirror of primary_tone.beat_hz */
  double program_carrier_hz;  /* convenience mirror of primary_tone.carrier_hz */
  double mix_amp_pct;         /* evaluated mix amp profile at time_sec */
  size_t voice_count;         /* active keyframed/static voice lanes */
  size_t aux_tone_count;      /* configured auxiliary overlay tones */
  size_t mix_effect_count;    /* configured static + timed mix-effect slots */
} SbxRuntimeTelemetry;

typedef void (*SbxTelemetryCallback)(const SbxRuntimeTelemetry *telem, void *user);

/* ----- Version and status ----- */

/* Runtime library version string (human-readable). */
const char *sbx_version(void);

/* Public API version integer (for feature gating). */
int sbx_api_version(void);

/* Convert status code (SBX_*) to short text. */
const char *sbx_status_string(int status);

/* ----- Defaults ----- */

/* Fill cfg with library defaults (44.1k stereo). */
void sbx_default_engine_config(SbxEngineConfig *cfg);

/* Fill tone with default binaural-safe values. */
void sbx_default_tone_spec(SbxToneSpec *tone);

/* Fill spec with library-default isochronic envelope values. */
void sbx_default_iso_envelope_spec(SbxIsoEnvelopeSpec *spec);

/* Fill cfg with default `.sbgf` evaluation environment values. */
void sbx_default_curve_eval_config(SbxCurveEvalConfig *cfg);

/* Fill cfg with default curve-backed context source settings. */
void sbx_default_curve_source_config(SbxCurveSourceConfig *cfg);

/* Fill dither state with library-default seed. */
void sbx_default_pcm16_dither_state(SbxPcm16DitherState *state);

/* Fill spec with library-default -A mix modulation parameters. */
void sbx_default_mix_mod_spec(SbxMixModSpec *spec);

/* Set explicit seed for deterministic PCM16 dithering. */
void sbx_seed_pcm16_dither_state(SbxPcm16DitherState *state, unsigned int seed);

/* Fill generic PCM conversion state with default seed + TPDF dither. */
void sbx_default_pcm_convert_state(SbxPcmConvertState *state);

/* Set explicit seed and dither mode for generic PCM conversion helpers. */
void sbx_seed_pcm_convert_state(SbxPcmConvertState *state,
                                unsigned int seed,
                                int dither_mode);

/* ----- Engine API ----- */

/* Create low-level engine instance. Returns NULL on failure. */
SbxEngine *sbx_engine_create(const SbxEngineConfig *cfg);

/* Destroy engine created by sbx_engine_create(). */
void sbx_engine_destroy(SbxEngine *eng);

/* Reset internal phase/state on existing engine. */
void sbx_engine_reset(SbxEngine *eng);

/* Set active tone for engine rendering. */
int sbx_engine_set_tone(SbxEngine *eng, const SbxToneSpec *tone);

/* Render interleaved stereo float samples into out[frames * channels]. */
int sbx_engine_render_f32(SbxEngine *eng, float *out, size_t frames);

/* Last engine-local error text. */
const char *sbx_engine_last_error(const SbxEngine *eng);

/* ----- PCM conversion helpers ----- */

/*
 * Convert normalized float samples (-1..1 nominal) to signed 16-bit PCM.
 * If dither_state is non-NULL, TPDF dither is added before rounding.
 */
int sbx_convert_f32_to_s16(const float *in,
                           short *out,
                           size_t sample_count,
                           SbxPcm16DitherState *dither_state);

/*
 * Convert normalized float samples (-1..1 nominal) to signed PCM using an
 * explicit conversion state. dither_mode controls whether TPDF dither is
 * added before rounding.
 */
int sbx_convert_f32_to_s16_ex(const float *in,
                              int16_t *out,
                              size_t sample_count,
                              SbxPcmConvertState *state);
int sbx_convert_f32_to_s24_32(const float *in,
                              int32_t *out,
                              size_t sample_count,
                              SbxPcmConvertState *state);
int sbx_convert_f32_to_s32(const float *in,
                           int32_t *out,
                           size_t sample_count,
                           SbxPcmConvertState *state);

/*
 * ----- Parser/formatter helpers -----
 */

/* Parse one tone token using sine default waveform. */
int sbx_parse_tone_spec(const char *spec, SbxToneSpec *out_tone);

/* Parse one tone token with explicit default waveform for unprefixed tones. */
int sbx_parse_tone_spec_ex(const char *spec, int default_waveform, SbxToneSpec *out_tone);

/* Parse SBG clock token HH:MM or HH:MM:SS. Returns consumed chars. */
int sbx_parse_sbg_clock_token(const char *tok, size_t *out_consumed, double *out_sec);

/* Format mix effect as canonical token (mixspin/mixpulse/mixbeat/mixam). */
int sbx_format_mix_fx_spec(const SbxMixFxSpec *fx, char *out, size_t out_sz);

/*
 * Parse one extra token as mix amp, tone, or mix effect.
 * out_type is set to SBX_EXTRA_* on success.
 */
int sbx_parse_extra_token(const char *tok,
                          int default_waveform,
                          int *out_type,
                          SbxToneSpec *out_tone,
                          SbxMixFxSpec *out_fx,
                          double *out_mix_amp_pct);

/* Format tone as canonical token string. */
int sbx_format_tone_spec(const SbxToneSpec *tone, char *out, size_t out_sz);

/* ----- Curve program API (.sbgf) ----- */

/* Create/destroy reusable `.sbgf` curve program object. */
SbxCurveProgram *sbx_curve_create(void);
void sbx_curve_destroy(SbxCurveProgram *curve);

/* Reset loaded/compiled state on an existing curve object. */
void sbx_curve_reset(SbxCurveProgram *curve);

/* Load `.sbgf` text/file into curve object. */
int sbx_curve_load_text(SbxCurveProgram *curve, const char *text, const char *source_name);
int sbx_curve_load_file(SbxCurveProgram *curve, const char *path);

/* Override/add one parameter value before preparation. */
int sbx_curve_set_param(SbxCurveProgram *curve, const char *name, double value);

/* Compile/prepare loaded curve expressions for evaluation. */
int sbx_curve_prepare(SbxCurveProgram *curve, const SbxCurveEvalConfig *cfg);

/* Evaluate prepared curve at timeline position t_sec. */
int sbx_curve_eval(SbxCurveProgram *curve, double t_sec, SbxCurveEvalPoint *out_point);

/*
 * Sample effective beat/pulse frequency from a prepared curve program over
 * a caller-specified time range. The curve must already be prepared.
 * out_t_sec is optional (may be NULL).
 */
int sbx_curve_sample_program_beat(SbxCurveProgram *curve,
                                  double t0_sec,
                                  double t1_sec,
                                  size_t sample_count,
                                  double *out_t_sec,
                                  double *out_hz);

/* Curve introspection. */
int sbx_curve_get_info(const SbxCurveProgram *curve, SbxCurveInfo *out_info);
size_t sbx_curve_param_count(const SbxCurveProgram *curve);
int sbx_curve_get_param(const SbxCurveProgram *curve,
                        size_t index,
                        const char **out_name,
                        double *out_value);
const char *sbx_curve_source_name(const SbxCurveProgram *curve);
const char *sbx_curve_last_error(const SbxCurveProgram *curve);

/* ----- Context lifecycle/load/render ----- */

/* Create context (higher-level runtime/load object). */
SbxContext *sbx_context_create(const SbxEngineConfig *cfg);

/* Destroy context created by sbx_context_create(). */
void sbx_context_destroy(SbxContext *ctx);

/* Reset context time and active source state. */
void sbx_context_reset(SbxContext *ctx);

/* Set static tone source on context. */
int sbx_context_set_tone(SbxContext *ctx, const SbxToneSpec *tone);

/* Set default waveform used by unprefixed parsed tones. */
int sbx_context_set_default_waveform(SbxContext *ctx, int waveform);

/* Parse and load single tone token onto context. */
int sbx_context_load_tone_spec(SbxContext *ctx, const char *tone_spec);

/*
 * Load a prepared curve program as an exact runtime source.
 * On success the context takes ownership of `curve`.
 */
int sbx_context_load_curve_program(SbxContext *ctx,
                                   SbxCurveProgram *curve,
                                   const SbxCurveSourceConfig *cfg);

/* Load keyframed program (strictly increasing time_sec). */
int sbx_context_load_keyframes(SbxContext *ctx,
                               const SbxProgramKeyframe *frames,
                               size_t frame_count,
                               int loop);

/* Load minimal keyframe sequence text. */
int sbx_context_load_sequence_text(SbxContext *ctx, const char *text, int loop);

/* Load minimal keyframe sequence file. */
int sbx_context_load_sequence_file(SbxContext *ctx, const char *path, int loop);

/* Load SBG timing subset text (HH:MM[:SS], NOW, blocks, names). */
int sbx_context_load_sbg_timing_text(SbxContext *ctx, const char *text, int loop);

/* Load SBG timing subset file. */
int sbx_context_load_sbg_timing_file(SbxContext *ctx, const char *path, int loop);

/* ----- Runtime overlays: aux tones ----- */

/* Replace auxiliary overlay tone list (max SBX_MAX_AUX_TONES). */
int sbx_context_set_aux_tones(SbxContext *ctx, const SbxToneSpec *tones, size_t tone_count);

/* Get number of currently configured auxiliary tones. */
size_t sbx_context_aux_tone_count(const SbxContext *ctx);

/* Read configured auxiliary tone by index. */
int sbx_context_get_aux_tone(const SbxContext *ctx, size_t index, SbxToneSpec *out);

/* ----- Runtime overlays: mix effects ----- */

/* Parse one mix effect token (mixspin/mixpulse/mixbeat/mixam). */
int sbx_parse_mix_fx_spec(const char *spec, int default_waveform, SbxMixFxSpec *out_fx);

/* Replace context mix-effect chain. */
int sbx_context_set_mix_effects(SbxContext *ctx, const SbxMixFxSpec *fxv, size_t fx_count);

/* Get number of configured mix effects. */
size_t sbx_context_mix_effect_count(const SbxContext *ctx);

/* Read configured mix effect by index. */
int sbx_context_get_mix_effect(const SbxContext *ctx, size_t index, SbxMixFxSpec *out_fx);

/* Apply configured mix effects to one stereo sample pair. */
int sbx_context_apply_mix_effects(SbxContext *ctx,
                                  double mix_l,
                                  double mix_r,
                                  double base_amp,
                                  double *out_add_l,
                                  double *out_add_r);

/*
 * Full mix-stream sample path used by runtime adapters.
 * Accepts int16 mix samples and returns additive stereo contribution.
 * mix_mod_mul is an optional extra host multiplier applied on top of any
 * configured SbxMixModSpec runtime modulation.
 */
int sbx_context_mix_stream_sample(SbxContext *ctx,
                                  double t_sec,
                                  int mix_l_sample,
                                  int mix_r_sample,
                                  double mix_mod_mul,
                                  double *out_add_l,
                                  double *out_add_r);

/* ----- Runtime overlays: mix amp keyframes ----- */

/* Replace mix amplitude keyframe profile. */
int sbx_context_set_mix_amp_keyframes(SbxContext *ctx,
                                      const SbxMixAmpKeyframe *kfs,
                                      size_t kf_count,
                                      double default_amp_pct);

/* Replace mix-modulation runtime profile used by the -A host option. */
int sbx_context_set_mix_mod(SbxContext *ctx, const SbxMixModSpec *spec);

/* Read currently configured mix-modulation profile. */
int sbx_context_get_mix_mod(const SbxContext *ctx, SbxMixModSpec *out);

/* Report whether a mix-modulation runtime profile is active. */
int sbx_context_has_mix_mod(const SbxContext *ctx);

/* One-call runtime extras setup (mix amp + mix effects + aux tones). */
int sbx_context_configure_runtime(SbxContext *ctx,
                                  const SbxMixAmpKeyframe *mix_kfs,
                                  size_t mix_kf_count,
                                  double default_mix_amp_pct,
                                  const SbxMixFxSpec *mix_fx,
                                  size_t mix_fx_count,
                                  const SbxToneSpec *aux_tones,
                                  size_t aux_count);

/* Evaluate mix amplitude percentage at context time t_sec. */
double sbx_context_mix_amp_at(SbxContext *ctx, double t_sec);

/* Evaluate mix-modulation multiplier at context time t_sec. */
double sbx_context_mix_mod_mul_at(SbxContext *ctx, double t_sec);

/* Evaluate runtime-effective mix amplitude percentage at t_sec. */
double sbx_context_mix_amp_effective_at(SbxContext *ctx, double t_sec);

/* Evaluate one mix-modulation spec directly, without a context. */
double sbx_mix_mod_mul_at(const SbxMixModSpec *spec, double t_sec);

/*
 * Sample evaluated mix amplitude percentage over [t0_sec, t1_sec].
 * - sample_count must be >= 1.
 * - out_amp_pct must have sample_count elements.
 * - out_t_sec is optional (may be NULL).
 */
int sbx_context_sample_mix_amp(SbxContext *ctx,
                               double t0_sec,
                               double t1_sec,
                               size_t sample_count,
                               double *out_t_sec,
                               double *out_amp_pct);

/* Number of explicit mix amplitude keyframes currently loaded. */
size_t sbx_context_mix_amp_keyframe_count(const SbxContext *ctx);

/* Read explicit mix amplitude keyframe by index. */
int sbx_context_get_mix_amp_keyframe(const SbxContext *ctx,
                                     size_t index,
                                     SbxMixAmpKeyframe *out);

/* Report whether the loaded/runtime context has explicit mix/<amp> control. */
int sbx_context_has_mix_amp_control(const SbxContext *ctx);

/* Report whether the loaded/runtime context has active mix-effect content. */
int sbx_context_has_mix_effects(const SbxContext *ctx);

/* Number of timed mix-effect keyframes currently loaded. */
size_t sbx_context_timed_mix_effect_keyframe_count(const SbxContext *ctx);

/* Number of timed mix-effect slots in each keyframe. */
size_t sbx_context_timed_mix_effect_slot_count(const SbxContext *ctx);

/* Read timed mix-effect keyframe metadata by index. */
int sbx_context_get_timed_mix_effect_keyframe_info(const SbxContext *ctx,
                                                   size_t index,
                                                   SbxTimedMixFxKeyframeInfo *out);

/*
 * Read one timed mix-effect slot from a loaded keyframe.
 * out_present is set to 1 when the slot was explicitly present in that
 * keyframe, or 0 when the slot is implicitly empty/none.
 */
int sbx_context_get_timed_mix_effect_slot(const SbxContext *ctx,
                                          size_t keyframe_index,
                                          size_t slot_index,
                                          SbxMixFxSpec *out_fx,
                                          int *out_present);

/*
 * Evaluate the effective mix-effect chain at one context time.
 * - Returns static runtime mix effects first, followed by evaluated timed
 *   native `.sbg` mix-effect slots.
 * - If out_fxv is NULL, out_count may be used to query the required size.
 * - Entries with type SBX_MIXFX_NONE represent empty timed slots.
 */
int sbx_context_sample_mix_effects(SbxContext *ctx,
                                   double t_sec,
                                   SbxMixFxSpec *out_fxv,
                                   size_t out_slots,
                                   size_t *out_count);

/*
 * Evaluate the effective tone set at one context time.
 * - Returns keyframed/static voice lanes first, followed by auxiliary tones.
 * - voice lane count is given by sbx_context_voice_count().
 * - If out_tones is NULL, out_count may be used to query the required size.
 * - Does not advance context render time.
 */
int sbx_context_eval_active_tones(SbxContext *ctx,
                                  double t_sec,
                                  SbxToneSpec *out_tones,
                                  size_t out_slots,
                                  size_t *out_count);

/*
 * Runtime telemetry:
 * - Register callback to receive one snapshot per sbx_context_render_f32 call.
 * - Pass NULL callback to disable.
 */
int sbx_context_set_telemetry_callback(SbxContext *ctx,
                                       SbxTelemetryCallback cb,
                                       void *user);

/* Retrieve a runtime telemetry snapshot at the current context time. */
int sbx_context_get_runtime_telemetry(SbxContext *ctx, SbxRuntimeTelemetry *out);

/* ----- Introspection/render ----- */

/* Number of currently loaded keyframes. */
size_t sbx_context_keyframe_count(const SbxContext *ctx);

/* Number of active voice lanes in the loaded source (1 for static tones). */
size_t sbx_context_voice_count(const SbxContext *ctx);

/* Source kind currently loaded into the context (SBX_SOURCE_*). */
int sbx_context_source_mode(const SbxContext *ctx);

/* Whether the current keyframed source is configured to loop. */
int sbx_context_is_looping(const SbxContext *ctx);

/* Read keyframe by index. */
int sbx_context_get_keyframe(const SbxContext *ctx, size_t index, SbxProgramKeyframe *out);

/* Read a specific voice lane from a loaded keyframe. */
int sbx_context_get_keyframe_voice(const SbxContext *ctx,
                                   size_t index,
                                   size_t voice_index,
                                   SbxProgramKeyframe *out);

/* Program duration in seconds (0 for static tone sources). */
double sbx_context_duration_sec(const SbxContext *ctx);

/*
 * Set current render clock time in seconds.
 * This resets internal oscillator/effect phase/state and restarts playback
 * from the requested timeline time.
 */
int sbx_context_set_time_sec(SbxContext *ctx, double t_sec);

/*
 * Sample evaluated tone values over [t0_sec, t1_sec].
 * - sample_count must be >= 1.
 * - out_tones must have sample_count elements.
 * - out_t_sec is optional (may be NULL).
 * - Times are interpreted in context time domain; looped keyframe programs
 *   are wrapped to keyframe duration when sampled.
 */
int sbx_context_sample_tones(SbxContext *ctx,
                             double t0_sec,
                             double t1_sec,
                             size_t sample_count,
                             double *out_t_sec,
                             SbxToneSpec *out_tones);

/*
 * Sample one specific voice lane over [t0_sec, t1_sec].
 * - voice_index 0 is the primary lane.
 * - secondary lanes are available for multivoice native `.sbg` content.
 */
int sbx_context_sample_tones_voice(SbxContext *ctx,
                                   size_t voice_index,
                                   double t0_sec,
                                   double t1_sec,
                                   size_t sample_count,
                                   double *out_t_sec,
                                   SbxToneSpec *out_tones);

/*
 * Sample effective program beat/pulse frequency over [t0_sec, t1_sec].
 * - sample_count must be >= 1.
 * - out_hz must have sample_count elements.
 * - out_t_sec is optional (may be NULL).
 * - Looping keyframed programs are wrapped to program duration when sampled.
 */
int sbx_context_sample_program_beat(SbxContext *ctx,
                                    double t0_sec,
                                    double t1_sec,
                                    size_t sample_count,
                                    double *out_t_sec,
                                    double *out_hz);

/*
 * Sample effective beat/pulse frequency for one specific voice lane.
 * - voice_index 0 is the primary lane.
 * - secondary lanes are available for multivoice native `.sbg` content.
 */
int sbx_context_sample_program_beat_voice(SbxContext *ctx,
                                          size_t voice_index,
                                          double t0_sec,
                                          double t1_sec,
                                          size_t sample_count,
                                          double *out_t_sec,
                                          double *out_hz);

/*
 * Sample one mixam cycle for plotting/inspection.
 * - fx must be an SBX_MIXFX_AM spec with valid mixam fields.
 * - rate_hz controls the cycle duration on the time axis.
 * - at least one of out_envelope/out_gain must be non-NULL.
 * - out_t_sec is optional (may be NULL).
 */
int sbx_sample_mixam_cycle(const SbxMixFxSpec *fx,
                           double rate_hz,
                           size_t sample_count,
                           double *out_t_sec,
                           double *out_envelope,
                           double *out_gain);

/*
 * Sample the built-in exponential drop beat/pulse curve used by `-p drop`.
 * Times are in seconds. out_t_sec is optional (may be NULL).
 */
int sbx_sample_drop_curve(double drop_sec,
                          double beat_start_hz,
                          double beat_target_hz,
                          int slide,
                          int n_step,
                          int step_len_sec,
                          size_t sample_count,
                          double *out_t_sec,
                          double *out_hz);

/*
 * Sample the built-in sigmoid beat/pulse curve used by `-p sigmoid`.
 * Times are in seconds. out_t_sec is optional (may be NULL).
 */
int sbx_sample_sigmoid_curve(double drop_sec,
                             double beat_start_hz,
                             double beat_target_hz,
                             double sig_l,
                             double sig_h,
                             double sig_a,
                             double sig_b,
                             size_t sample_count,
                             double *out_t_sec,
                             double *out_hz);

/*
 * Sample one isochronic cycle for plotting/inspection.
 * - tone must be an isochronic tone with beat_hz > 0.
 * - if env is NULL, the tone's runtime envelope is used:
 *   start=tone->iso_start, duty=tone->duty_cycle, attack=tone->iso_attack,
 *   release=tone->iso_release, edge=tone->iso_edge_mode.
 * - at least one of out_envelope/out_wave must be non-NULL.
 * - out_t_sec is optional (may be NULL).
 */
int sbx_sample_isochronic_cycle(const SbxToneSpec *tone,
                                const SbxIsoEnvelopeSpec *env,
                                size_t sample_count,
                                double *out_t_sec,
                                double *out_envelope,
                                double *out_wave);

/*
 * Context-aware variant used when the tone references a context-owned custom
 * envelope (`waveNN` or `customNN`).
 */
int sbx_context_sample_isochronic_cycle(const SbxContext *ctx,
                                        const SbxToneSpec *tone,
                                        const SbxIsoEnvelopeSpec *env,
                                        size_t sample_count,
                                        double *out_t_sec,
                                        double *out_envelope,
                                        double *out_wave);

/* Query stored edge/smoothing mode for a context-owned waveNN/customNN envelope. */
int sbx_context_get_envelope_edge_mode(const SbxContext *ctx,
                                       int envelope_waveform,
                                       int *out_edge_mode);

/* Render interleaved stereo float frames from context source. */
int sbx_context_render_f32(SbxContext *ctx, float *out, size_t frames);

/* Current render clock time in seconds. */
double sbx_context_time_sec(const SbxContext *ctx);

/* Last context-local error text. */
const char *sbx_context_last_error(const SbxContext *ctx);

#ifdef __cplusplus
}
#endif

#endif /* SBAGENXLIB_H */
