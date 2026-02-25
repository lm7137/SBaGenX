#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sbagenlib.h"

static void fail(const char *msg) {
  fprintf(stderr, "FAIL: %s\n", msg);
  exit(1);
}

static int has_energy(const float *buf, size_t n) {
  size_t i;
  double e = 0.0;
  for (i = 0; i < n; i++) {
    e += fabs(buf[i]);
  }
  return e > 1e-6;
}

static void assert_parse(const char *spec, SbxToneMode mode, double carrier, double beat, double amp, int abs_beat, int waveform) {
  SbxToneSpec t;
  if (sbx_parse_tone_spec(spec, &t) != SBX_OK) fail("parse failed unexpectedly");
  if (t.mode != mode) fail("parsed mode mismatch");
  if (fabs(t.carrier_hz - carrier) > 1e-9) fail("parsed carrier mismatch");
  if (abs_beat) {
    if (fabs(fabs(t.beat_hz) - fabs(beat)) > 1e-9) fail("parsed beat abs mismatch");
  } else {
    if (fabs(t.beat_hz - beat) > 1e-9) fail("parsed beat mismatch");
  }
  if (fabs(t.amplitude - amp) > 1e-9) fail("parsed amplitude mismatch");
  if (t.waveform != waveform) fail("parsed waveform mismatch");
}

int main(void) {
  SbxEngineConfig cfg;
  SbxContext *ctx;
  float *buf;
  const size_t frames = 4096;

  assert_parse("200+10/20", SBX_TONE_BINAURAL, 200.0, 10.0, 0.20, 0, SBX_WAVE_SINE);
  assert_parse("200-10/20", SBX_TONE_BINAURAL, 200.0, -10.0, 0.20, 0, SBX_WAVE_SINE);
  assert_parse("square:200M10/20", SBX_TONE_MONAURAL, 200.0, 10.0, 0.20, 1, SBX_WAVE_SQUARE);
  assert_parse("200@4/30", SBX_TONE_ISOCHRONIC, 200.0, 4.0, 0.30, 1, SBX_WAVE_SINE);
  assert_parse("triangle:300@8/15", SBX_TONE_ISOCHRONIC, 300.0, 8.0, 0.15, 1, SBX_WAVE_TRIANGLE);
  assert_parse("sawtooth:150/25", SBX_TONE_BINAURAL, 150.0, 0.0, 0.25, 0, SBX_WAVE_SAWTOOTH);

  {
    SbxToneSpec t;
    if (sbx_parse_tone_spec("mixspin:400+6/40", &t) == SBX_OK)
      fail("unsupported spec parsed as valid");
  }

  sbx_default_engine_config(&cfg);
  ctx = sbx_context_create(&cfg);
  if (!ctx) fail("context create failed");

  buf = (float *)calloc(frames * 2, sizeof(float));
  if (!buf) fail("alloc failed");

  if (sbx_context_render_f32(ctx, buf, frames) != SBX_ENOTREADY)
    fail("render should fail with ENOTREADY before load");

  if (sbx_context_load_tone_spec(ctx, "200+10/20") != SBX_OK)
    fail("load tone spec failed");
  memset(buf, 0, frames * 2 * sizeof(float));
  if (sbx_context_render_f32(ctx, buf, frames) != SBX_OK)
    fail("render after load failed");
  if (!has_energy(buf, frames * 2))
    fail("render output appears silent");

  if (sbx_context_load_tone_spec(ctx, "invalid") != SBX_EINVAL)
    fail("invalid tone spec should fail");

  free(buf);
  sbx_context_destroy(ctx);

  printf("PASS: sbagenlib context API checks\n");
  return 0;
}
