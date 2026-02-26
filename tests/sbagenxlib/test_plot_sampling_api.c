#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "sbagenxlib.h"

static void
fail(const char *msg) {
  fprintf(stderr, "FAIL: %s\n", msg);
  exit(1);
}

static int
near(double a, double b, double eps) {
  return fabs(a - b) <= eps;
}

int
main(void) {
  SbxEngineConfig cfg;
  SbxContext *ctx;
  SbxProgramKeyframe kf[2];
  SbxToneSpec samples[8];
  double ts[8];
  int rc;

  sbx_default_engine_config(&cfg);
  cfg.sample_rate = 44100.0;
  cfg.channels = 2;
  ctx = sbx_context_create(&cfg);
  if (!ctx) fail("context create failed");

  sbx_default_tone_spec(&kf[0].tone);
  sbx_default_tone_spec(&kf[1].tone);
  kf[0].time_sec = 0.0;
  kf[0].tone.mode = SBX_TONE_BINAURAL;
  kf[0].tone.carrier_hz = 200.0;
  kf[0].tone.beat_hz = 10.0;
  kf[0].tone.amplitude = 0.4;
  kf[0].interp = SBX_INTERP_LINEAR;
  kf[1] = kf[0];
  kf[1].time_sec = 10.0;
  kf[1].tone.beat_hz = 0.0;

  rc = sbx_context_load_keyframes(ctx, kf, 2, 0);
  if (rc != SBX_OK) fail("load_keyframes linear failed");
  rc = sbx_context_sample_tones(ctx, 0.0, 10.0, 6, ts, samples);
  if (rc != SBX_OK) fail("sample_tones linear failed");
  if (!near(samples[0].beat_hz, 10.0, 1e-9)) fail("linear sample[0] beat mismatch");
  if (!near(samples[1].beat_hz, 8.0, 1e-9)) fail("linear sample[1] beat mismatch");
  if (!near(samples[2].beat_hz, 6.0, 1e-9)) fail("linear sample[2] beat mismatch");
  if (!near(samples[3].beat_hz, 4.0, 1e-9)) fail("linear sample[3] beat mismatch");
  if (!near(samples[4].beat_hz, 2.0, 1e-9)) fail("linear sample[4] beat mismatch");
  if (!near(samples[5].beat_hz, 0.0, 1e-9)) fail("linear sample[5] beat mismatch");
  if (!near(ts[0], 0.0, 1e-12) || !near(ts[5], 10.0, 1e-12))
    fail("sample_tones output times mismatch");
  if (!near(sbx_context_time_sec(ctx), 0.0, 1e-12))
    fail("sample_tones must not advance context render time");

  kf[0].interp = SBX_INTERP_STEP;
  rc = sbx_context_load_keyframes(ctx, kf, 2, 0);
  if (rc != SBX_OK) fail("load_keyframes step failed");
  rc = sbx_context_sample_tones(ctx, 0.0, 10.0, 3, 0, samples);
  if (rc != SBX_OK) fail("sample_tones step failed");
  if (!near(samples[1].beat_hz, 10.0, 1e-9))
    fail("step interpolation should hold previous beat at midpoint");

  kf[0].interp = SBX_INTERP_LINEAR;
  rc = sbx_context_load_keyframes(ctx, kf, 2, 1);
  if (rc != SBX_OK) fail("load_keyframes loop failed");
  rc = sbx_context_sample_tones(ctx, 8.0, 12.0, 3, ts, samples);
  if (rc != SBX_OK) fail("sample_tones loop failed");
  if (!near(samples[0].beat_hz, 2.0, 1e-9)) fail("loop sample at 8s mismatch");
  if (!near(samples[1].beat_hz, 10.0, 1e-9)) fail("loop sample at 10s mismatch");
  if (!near(samples[2].beat_hz, 8.0, 1e-9)) fail("loop sample at 12s mismatch");

  rc = sbx_context_load_tone_spec(ctx, "200+6/40");
  if (rc != SBX_OK) fail("load_tone_spec failed");
  rc = sbx_context_sample_tones(ctx, 0.0, 1.0, 4, ts, samples);
  if (rc != SBX_OK) fail("sample_tones static failed");
  if (samples[0].mode != SBX_TONE_BINAURAL) fail("static mode mismatch");
  if (!near(samples[0].carrier_hz, 200.0, 1e-9)) fail("static carrier mismatch");
  if (!near(samples[0].beat_hz, 6.0, 1e-9)) fail("static beat mismatch");
  if (!near(samples[0].amplitude, 0.4, 1e-9)) fail("static amplitude mismatch");
  if (!near(samples[1].beat_hz, samples[0].beat_hz, 1e-12) ||
      !near(samples[3].beat_hz, samples[0].beat_hz, 1e-12))
    fail("static samples should be constant");

  rc = sbx_context_sample_tones(ctx, 0.0, 1.0, 0, ts, samples);
  if (rc != SBX_EINVAL) fail("sample_count=0 should fail");

  sbx_context_destroy(ctx);
  printf("PASS: sbagenxlib plot sampling API checks\n");
  return 0;
}
