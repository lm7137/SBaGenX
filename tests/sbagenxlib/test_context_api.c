#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sbagenxlib.h"

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

static double total_abs(const float *buf, size_t n) {
  size_t i;
  double e = 0.0;
  for (i = 0; i < n; i++) e += fabs((double)buf[i]);
  return e;
}

static int has_stereo_difference(const float *buf, size_t frames) {
  size_t i;
  double d = 0.0;
  for (i = 0; i < frames; i++) {
    d += fabs((double)buf[i * 2] - (double)buf[i * 2 + 1]);
  }
  return d > 1e-6;
}

static double rms_left_window(const float *buf, size_t i0, size_t i1) {
  size_t i;
  double e = 0.0;
  size_t n = 0;
  if (i1 <= i0) return 0.0;
  for (i = i0; i < i1; i++) {
    double v = (double)buf[i * 2];
    e += v * v;
    n++;
  }
  if (!n) return 0.0;
  return sqrt(e / (double)n);
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
  assert_parse("square:bell300/25", SBX_TONE_BELL, 300.0, 0.0, 0.25, 0, SBX_WAVE_SQUARE);
  assert_parse("spin:80+1/40", SBX_TONE_SPIN_PINK, 80.0, 1.0, 0.40, 0, SBX_WAVE_SINE);
  assert_parse("triangle:bspin:120-1.5/35", SBX_TONE_SPIN_BROWN, 120.0, -1.5, 0.35, 0, SBX_WAVE_TRIANGLE);
  assert_parse("square:wspin:60+0.75/25", SBX_TONE_SPIN_WHITE, 60.0, 0.75, 0.25, 0, SBX_WAVE_SQUARE);
  {
    SbxToneSpec in, out;
    char spec[256];
    if (sbx_parse_tone_spec("triangle:bell300/25", &in) != SBX_OK)
      fail("parse before format failed");
    if (sbx_format_tone_spec(&in, spec, sizeof(spec)) != SBX_OK)
      fail("format tone spec failed");
    if (sbx_parse_tone_spec(spec, &out) != SBX_OK)
      fail("parse after format failed");
    if (out.mode != in.mode) fail("format roundtrip mode mismatch");
    if (fabs(out.carrier_hz - in.carrier_hz) > 1e-9) fail("format roundtrip carrier mismatch");
    if (fabs(out.amplitude - in.amplitude) > 1e-9) fail("format roundtrip amplitude mismatch");
  }
  {
    SbxToneSpec t;
    if (sbx_parse_tone_spec_ex("200+8/15", SBX_WAVE_SAWTOOTH, &t) != SBX_OK)
      fail("parse_ex failed");
    if (t.waveform != SBX_WAVE_SAWTOOTH)
      fail("parse_ex default waveform was not applied");
  }

  {
    SbxToneSpec t;
    if (sbx_parse_tone_spec("mixspin:400+6/40", &t) == SBX_OK)
      fail("unsupported spec parsed as valid");
  }
  {
    SbxMixFxSpec fx;
    double add_l = 0.0, add_r = 0.0;
    if (sbx_parse_mix_fx_spec("triangle:mixpulse:1/50", SBX_WAVE_SINE, &fx) != SBX_OK)
      fail("mix fx parse failed");
    if (fx.type != SBX_MIXFX_PULSE || fx.waveform != SBX_WAVE_TRIANGLE)
      fail("mix fx parse value mismatch");
  }

  sbx_default_engine_config(&cfg);
  ctx = sbx_context_create(&cfg);
  if (!ctx) fail("context create failed");
  if (sbx_context_set_default_waveform(ctx, 99) != SBX_EINVAL)
    fail("invalid default waveform should fail");
  if (sbx_context_set_default_waveform(ctx, SBX_WAVE_TRIANGLE) != SBX_OK)
    fail("setting default waveform failed");

  {
    SbxMixFxSpec fx;
    SbxMixFxSpec fx_out;
    double add_l = 0.0, add_r = 0.0;
    if (sbx_parse_mix_fx_spec("triangle:mixpulse:1/50", SBX_WAVE_SINE, &fx) != SBX_OK)
      fail("mix fx parse failed");
    if (sbx_context_set_mix_effects(ctx, &fx, 1) != SBX_OK)
      fail("set mix effects failed");
    if (sbx_context_mix_effect_count(ctx) != 1)
      fail("mix effect count mismatch");
    if (sbx_context_get_mix_effect(ctx, 0, &fx_out) != SBX_OK)
      fail("get mix effect failed");
    if (fx_out.type != SBX_MIXFX_PULSE || fx_out.waveform != SBX_WAVE_TRIANGLE)
      fail("get mix effect mismatch");
    if (sbx_context_apply_mix_effects(ctx, 100.0, 80.0, 0.5, &add_l, &add_r) != SBX_OK)
      fail("apply mix effects failed");
    if (!(fabs(add_l) > 1e-9 || fabs(add_r) > 1e-9))
      fail("mix effects produced zero contribution");
    if (sbx_context_set_mix_effects(ctx, 0, 0) != SBX_OK)
      fail("clear mix effects failed");
  }

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
  {
    SbxToneSpec aux;
    double e_plain, e_aux;
    memset(&aux, 0, sizeof(aux));
    if (sbx_parse_tone_spec("250/10", &aux) != SBX_OK)
      fail("parse aux tone failed");
    e_plain = total_abs(buf, frames * 2);
    if (sbx_context_set_aux_tones(ctx, &aux, 1) != SBX_OK)
      fail("set aux tones failed");
    if (sbx_context_aux_tone_count(ctx) != 1)
      fail("aux tone count mismatch");
    memset(&aux, 0, sizeof(aux));
    if (sbx_context_get_aux_tone(ctx, 0, &aux) != SBX_OK)
      fail("get aux tone failed");
    if (aux.mode != SBX_TONE_BINAURAL)
      fail("aux tone mode mismatch");
    sbx_context_reset(ctx);
    memset(buf, 0, frames * 2 * sizeof(float));
    if (sbx_context_render_f32(ctx, buf, frames) != SBX_OK)
      fail("render after aux set failed");
    e_aux = total_abs(buf, frames * 2);
    if (!(e_aux > e_plain * 1.1))
      fail("aux tones should increase output energy");
    if (sbx_context_set_aux_tones(ctx, 0, 0) != SBX_OK)
      fail("clearing aux tones failed");
    if (sbx_context_aux_tone_count(ctx) != 0)
      fail("aux tone clear count mismatch");
  }

  if (sbx_context_load_tone_spec(ctx, "spin:90+1/30") != SBX_OK)
    fail("load spin tone spec failed");
  memset(buf, 0, frames * 2 * sizeof(float));
  if (sbx_context_render_f32(ctx, buf, frames) != SBX_OK)
    fail("render after spin load failed");
  if (!has_energy(buf, frames * 2))
    fail("spin render output appears silent");
  if (!has_stereo_difference(buf, frames))
    fail("spin render should produce L/R differences");

  if (sbx_context_load_tone_spec(ctx, "bell320/40") != SBX_OK)
    fail("load bell tone spec failed");
  {
    const size_t bell_frames = (size_t)(cfg.sample_rate * 2.0);
    float *bell_buf = (float *)calloc(bell_frames * 2, sizeof(float));
    if (!bell_buf) fail("alloc failed (bell)");
    if (sbx_context_render_f32(ctx, bell_buf, bell_frames) != SBX_OK)
      fail("render after bell load failed");
    if (!has_energy(bell_buf, bell_frames * 2))
      fail("bell render output appears silent");
    {
      double e0 = rms_left_window(bell_buf, 0, bell_frames / 8);
      double e1 = rms_left_window(bell_buf, bell_frames * 3 / 4, bell_frames);
      if (!(e0 > e1 * 3.0))
        fail("bell should decay over time");
    }
    free(bell_buf);
  }

  if (sbx_context_load_tone_spec(ctx, "invalid") != SBX_EINVAL)
    fail("invalid tone spec should fail");

  free(buf);
  sbx_context_destroy(ctx);

  printf("PASS: sbagenxlib context API checks\n");
  return 0;
}
