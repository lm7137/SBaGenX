#ifndef SBAGENXLIB_H
#define SBAGENXLIB_H

/*
 * sbagenxlib public C API.
 *
 * The library renders stereo float audio from tone specs or loaded keyframed
 * programs. Device I/O and container encoding remain host responsibilities.
 */

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SBX_API_VERSION 8   /* public API contract revision */
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
  SBX_WAVE_CUSTOM_BASE = 1000 /* context-owned waveNN table: SBX_WAVE_CUSTOM_BASE + [0..99] */
} SbxWaveform;

typedef enum {
  SBX_INTERP_LINEAR = 0,
  SBX_INTERP_STEP = 1
} SbxInterpMode;

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
  int waveform;      /* SBX_WAVE_* or SBX_WAVE_CUSTOM_BASE + [0..99] for waveNN */
  double duty_cycle; /* for isochronic mode: 0.0 .. 1.0 (default 0.4) */
} SbxToneSpec;

typedef struct {
  double time_sec; /* keyframe timestamp, >= 0, increasing */
  SbxToneSpec tone;
  int interp; /* interpolation mode to next keyframe: SBX_INTERP_* */
} SbxProgramKeyframe;

typedef struct SbxEngine SbxEngine;
typedef struct SbxContext SbxContext;

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

/* Report whether the loaded/runtime context has explicit mix/<amp> control. */
int sbx_context_has_mix_amp_control(const SbxContext *ctx);

/* Report whether the loaded/runtime context has active mix-effect content. */
int sbx_context_has_mix_effects(const SbxContext *ctx);

/* ----- Introspection/render ----- */

/* Number of currently loaded keyframes. */
size_t sbx_context_keyframe_count(const SbxContext *ctx);

/* Number of active voice lanes in loaded keyframe content. */
size_t sbx_context_voice_count(const SbxContext *ctx);

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
