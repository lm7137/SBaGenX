#ifndef SBAGENXLIB_H
#define SBAGENXLIB_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SBX_API_VERSION 2
#define SBX_MAX_AUX_TONES 16

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
  SBX_WAVE_SAWTOOTH = 3
} SbxWaveform;

typedef enum {
  SBX_INTERP_LINEAR = 0,
  SBX_INTERP_STEP = 1
} SbxInterpMode;

typedef enum {
  SBX_MIXFX_NONE = 0,
  SBX_MIXFX_SPIN = 1,
  SBX_MIXFX_PULSE = 2,
  SBX_MIXFX_BEAT = 3
} SbxMixFxType;

typedef struct {
  int type;      /* SBX_MIXFX_* */
  int waveform;  /* SBX_WAVE_* */
  double carr;   /* mixspin width in microseconds */
  double res;    /* modulation/spin frequency in Hz */
  double amp;    /* 0..1 effect amount */
} SbxMixFxSpec;

typedef struct {
  double time_sec;
  double amp_pct;
  int interp; /* SBX_INTERP_* */
} SbxMixAmpKeyframe;

typedef struct {
  double sample_rate; /* Hz, e.g. 44100 */
  int channels;       /* currently 2 (stereo) */
} SbxEngineConfig;

typedef struct {
  SbxToneMode mode;
  double carrier_hz;
  double beat_hz;
  double amplitude;  /* 0.0 .. 1.0 */
  int waveform;      /* SBX_WAVE_* */
  double duty_cycle; /* for isochronic mode: 0.0 .. 1.0 (default 0.4) */
} SbxToneSpec;

typedef struct {
  double time_sec; /* keyframe timestamp, >= 0, increasing */
  SbxToneSpec tone;
  int interp; /* interpolation mode to next keyframe: SBX_INTERP_* */
} SbxProgramKeyframe;

typedef struct SbxEngine SbxEngine;
typedef struct SbxContext SbxContext;

const char *sbx_version(void);
int sbx_api_version(void);
const char *sbx_status_string(int status);

void sbx_default_engine_config(SbxEngineConfig *cfg);
void sbx_default_tone_spec(SbxToneSpec *tone);

SbxEngine *sbx_engine_create(const SbxEngineConfig *cfg);
void sbx_engine_destroy(SbxEngine *eng);
void sbx_engine_reset(SbxEngine *eng);

int sbx_engine_set_tone(SbxEngine *eng, const SbxToneSpec *tone);

/* Render interleaved stereo float samples into out[frames * channels]. */
int sbx_engine_render_f32(SbxEngine *eng, float *out, size_t frames);

const char *sbx_engine_last_error(const SbxEngine *eng);

/*
 * Phase 3 context API (entry point for future sequence/program loading).
 * This keeps frontend state out of callers and prepares the transition from
 * monolithic sbagenx.c runtime to a reusable core context.
 */
int sbx_parse_tone_spec(const char *spec, SbxToneSpec *out_tone);
int sbx_parse_tone_spec_ex(const char *spec, int default_waveform, SbxToneSpec *out_tone);
int sbx_format_tone_spec(const SbxToneSpec *tone, char *out, size_t out_sz);

SbxContext *sbx_context_create(const SbxEngineConfig *cfg);
void sbx_context_destroy(SbxContext *ctx);
void sbx_context_reset(SbxContext *ctx);
int sbx_context_set_tone(SbxContext *ctx, const SbxToneSpec *tone);
int sbx_context_set_default_waveform(SbxContext *ctx, int waveform);
int sbx_context_load_tone_spec(SbxContext *ctx, const char *tone_spec);
int sbx_context_load_keyframes(SbxContext *ctx,
                               const SbxProgramKeyframe *frames,
                               size_t frame_count,
                               int loop);
int sbx_context_load_sequence_text(SbxContext *ctx, const char *text, int loop);
int sbx_context_load_sequence_file(SbxContext *ctx, const char *path, int loop);
int sbx_context_load_sbg_timing_text(SbxContext *ctx, const char *text, int loop);
int sbx_context_load_sbg_timing_file(SbxContext *ctx, const char *path, int loop);
int sbx_context_set_aux_tones(SbxContext *ctx, const SbxToneSpec *tones, size_t tone_count);
size_t sbx_context_aux_tone_count(const SbxContext *ctx);
int sbx_context_get_aux_tone(const SbxContext *ctx, size_t index, SbxToneSpec *out);
int sbx_parse_mix_fx_spec(const char *spec, int default_waveform, SbxMixFxSpec *out_fx);
int sbx_context_set_mix_effects(SbxContext *ctx, const SbxMixFxSpec *fxv, size_t fx_count);
size_t sbx_context_mix_effect_count(const SbxContext *ctx);
int sbx_context_get_mix_effect(const SbxContext *ctx, size_t index, SbxMixFxSpec *out_fx);
int sbx_context_apply_mix_effects(SbxContext *ctx,
                                  double mix_l,
                                  double mix_r,
                                  double base_amp,
                                  double *out_add_l,
                                  double *out_add_r);
int sbx_context_set_mix_amp_keyframes(SbxContext *ctx,
                                      const SbxMixAmpKeyframe *kfs,
                                      size_t kf_count,
                                      double default_amp_pct);
double sbx_context_mix_amp_at(SbxContext *ctx, double t_sec);
size_t sbx_context_keyframe_count(const SbxContext *ctx);
int sbx_context_get_keyframe(const SbxContext *ctx, size_t index, SbxProgramKeyframe *out);
int sbx_context_render_f32(SbxContext *ctx, float *out, size_t frames);
double sbx_context_time_sec(const SbxContext *ctx);
const char *sbx_context_last_error(const SbxContext *ctx);

#ifdef __cplusplus
}
#endif

#endif /* SBAGENXLIB_H */
