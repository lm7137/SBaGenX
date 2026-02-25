#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sbagenxlib.h"

static void
fail(const char *msg) {
  fprintf(stderr, "FAIL: %s\n", msg);
  exit(1);
}

static int
zero_crossings_left(const float *buf, size_t frames) {
  size_t i;
  int zc = 0;
  float prev = buf[0];
  for (i = 1; i < frames; i++) {
    float cur = buf[i * 2];
    if ((prev <= 0.0f && cur > 0.0f) || (prev >= 0.0f && cur < 0.0f))
      zc++;
    prev = cur;
  }
  return zc;
}

int
main(void) {
  SbxEngineConfig cfg;
  SbxContext *ctx;
  SbxProgramKeyframe kf[2];
  float *buf;
  const size_t frames_1s = 44100;
  size_t loop_frames;
  double t_expected, t_actual;
  int zc0, zc1;
  size_t step_frames;

  sbx_default_engine_config(&cfg);
  cfg.sample_rate = 44100.0;
  cfg.channels = 2;

  ctx = sbx_context_create(&cfg);
  if (!ctx) fail("context create failed");

  sbx_default_tone_spec(&kf[0].tone);
  kf[0].time_sec = 0.0;
  kf[0].tone.mode = SBX_TONE_BINAURAL;
  kf[0].tone.carrier_hz = 100.0;
  kf[0].tone.beat_hz = 0.0;
  kf[0].tone.amplitude = 0.5;
  kf[0].interp = SBX_INTERP_LINEAR;

  kf[1] = kf[0];
  kf[1].time_sec = 1.0;
  kf[1].tone.carrier_hz = 300.0;

  if (sbx_context_load_keyframes(ctx, kf, 2, 0) != SBX_OK)
    fail("load_keyframes (linear ramp) failed");

  buf = (float *)calloc(frames_1s * 2, sizeof(float));
  if (!buf) fail("alloc failed");
  if (sbx_context_render_f32(ctx, buf, frames_1s) != SBX_OK)
    fail("render failed");

  zc0 = zero_crossings_left(buf, frames_1s / 2);
  zc1 = zero_crossings_left(buf + (frames_1s / 2) * 2, frames_1s / 2);
  if (!(zc1 > (int)(1.3 * zc0)))
    fail("expected significantly higher crossing rate in second half");

  t_actual = sbx_context_time_sec(ctx);
  if (fabs(t_actual - 1.0) > 1e-3)
    fail("context time after 1 second render is out of range");

  /* Step mode should hold first frequency until the keyframe boundary. */
  kf[1] = kf[0];
  kf[1].time_sec = 1.0;
  kf[1].tone.carrier_hz = 300.0;
  kf[0].interp = SBX_INTERP_STEP;
  if (sbx_context_load_keyframes(ctx, kf, 2, 0) != SBX_OK)
    fail("load_keyframes (step mode) failed");
  step_frames = (size_t)(1.2 * cfg.sample_rate);
  free(buf);
  buf = (float *)calloc(step_frames * 2, sizeof(float));
  if (!buf) fail("alloc failed (step mode)");
  if (sbx_context_render_f32(ctx, buf, step_frames) != SBX_OK)
    fail("render failed for step mode");
  zc0 = zero_crossings_left(buf, (size_t)(0.4 * cfg.sample_rate));
  zc1 = zero_crossings_left(buf + ((size_t)(1.0 * cfg.sample_rate)) * 2,
                            (size_t)(0.2 * cfg.sample_rate));
  if (!(zc1 > (int)(1.2 * zc0)))
    fail("step mode should jump strongly at boundary");
  kf[0].interp = SBX_INTERP_LINEAR;

  free(buf);
  buf = (float *)calloc(frames_1s * 2, sizeof(float));
  if (!buf) fail("alloc failed");

  /* Validation checks. */
  kf[1] = kf[0];
  kf[1].time_sec = 0.0;
  if (sbx_context_load_keyframes(ctx, kf, 2, 0) != SBX_EINVAL)
    fail("non-increasing keyframe times should fail");

  kf[1] = kf[0];
  kf[1].time_sec = 1.0;
  kf[1].tone.mode = SBX_TONE_MONAURAL;
  if (sbx_context_load_keyframes(ctx, kf, 2, 0) != SBX_EINVAL)
    fail("mixed keyframe modes should fail");

  /* Looping check. */
  kf[1] = kf[0];
  kf[1].time_sec = 0.1;
  kf[1].tone.carrier_hz = 180.0;
  if (sbx_context_load_keyframes(ctx, kf, 2, 1) != SBX_OK)
    fail("load_keyframes (looping) failed");

  loop_frames = (size_t)(0.35 * cfg.sample_rate);
  memset(buf, 0, loop_frames * 2 * sizeof(float));
  if (sbx_context_render_f32(ctx, buf, loop_frames) != SBX_OK)
    fail("loop render failed");

  t_expected = fmod((double)loop_frames / cfg.sample_rate, 0.1);
  t_actual = sbx_context_time_sec(ctx);
  if (fabs(t_actual - t_expected) > 2e-3)
    fail("looped context time is out of expected range");

  free(buf);
  sbx_context_destroy(ctx);
  printf("PASS: sbagenxlib keyframe API checks\n");
  return 0;
}
