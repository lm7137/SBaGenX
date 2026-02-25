#ifndef SBAGENLIB_H
#define SBAGENLIB_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SBX_API_VERSION 1

/* Status codes returned by sbagenlib APIs. */
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
  SBX_TONE_ISOCHRONIC = 3
} SbxToneMode;

typedef struct {
  double sample_rate; /* Hz, e.g. 44100 */
  int channels;       /* currently 2 (stereo) */
} SbxEngineConfig;

typedef struct {
  SbxToneMode mode;
  double carrier_hz;
  double beat_hz;
  double amplitude;  /* 0.0 .. 1.0 */
  double duty_cycle; /* for isochronic mode: 0.0 .. 1.0 (default 0.4) */
} SbxToneSpec;

typedef struct {
  double time_sec; /* keyframe timestamp, >= 0, increasing */
  SbxToneSpec tone;
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

SbxContext *sbx_context_create(const SbxEngineConfig *cfg);
void sbx_context_destroy(SbxContext *ctx);
void sbx_context_reset(SbxContext *ctx);
int sbx_context_set_tone(SbxContext *ctx, const SbxToneSpec *tone);
int sbx_context_load_tone_spec(SbxContext *ctx, const char *tone_spec);
int sbx_context_load_keyframes(SbxContext *ctx,
                               const SbxProgramKeyframe *frames,
                               size_t frame_count,
                               int loop);
int sbx_context_load_sequence_text(SbxContext *ctx, const char *text, int loop);
int sbx_context_load_sequence_file(SbxContext *ctx, const char *path, int loop);
int sbx_context_load_sbg_timing_text(SbxContext *ctx, const char *text, int loop);
int sbx_context_load_sbg_timing_file(SbxContext *ctx, const char *path, int loop);
int sbx_context_render_f32(SbxContext *ctx, float *out, size_t frames);
double sbx_context_time_sec(const SbxContext *ctx);
const char *sbx_context_last_error(const SbxContext *ctx);

#ifdef __cplusplus
}
#endif

#endif /* SBAGENLIB_H */
