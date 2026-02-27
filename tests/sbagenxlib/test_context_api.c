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
  assert_parse("-", SBX_TONE_NONE, 200.0, 10.0, 0.0, 0, SBX_WAVE_SINE);
  assert_parse("spin:80+1/40", SBX_TONE_SPIN_PINK, 80.0, 1.0, 0.40, 0, SBX_WAVE_SINE);
  assert_parse("triangle:bspin:120-1.5/35", SBX_TONE_SPIN_BROWN, 120.0, -1.5, 0.35, 0, SBX_WAVE_TRIANGLE);
  assert_parse("square:wspin:60+0.75/25", SBX_TONE_SPIN_WHITE, 60.0, 0.75, 0.25, 0, SBX_WAVE_SQUARE);
  {
    double sec = 0.0;
    size_t used = 0;
    if (sbx_parse_sbg_clock_token("00:12:34", &used, &sec) != SBX_OK)
      fail("parse sbg clock token hh:mm:ss failed");
    if (used != 8 || fabs(sec - 754.0) > 1e-9)
      fail("parse sbg clock token hh:mm:ss mismatch");
    if (sbx_parse_sbg_clock_token("04:30", &used, &sec) != SBX_OK)
      fail("parse sbg clock token hh:mm failed");
    if (used != 5 || fabs(sec - 16200.0) > 1e-9)
      fail("parse sbg clock token hh:mm mismatch");
    if (sbx_parse_sbg_clock_token("25:00", &used, &sec) != SBX_EINVAL)
      fail("parse sbg clock token range validation failed");
  }
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
    SbxToneSpec in, out;
    char spec[256];
    if (sbx_parse_tone_spec("-", &in) != SBX_OK)
      fail("parse off tone failed");
    if (sbx_format_tone_spec(&in, spec, sizeof(spec)) != SBX_OK)
      fail("format off tone failed");
    if (strcmp(spec, "-") != 0)
      fail("format off tone string mismatch");
    if (sbx_parse_tone_spec(spec, &out) != SBX_OK || out.mode != SBX_TONE_NONE)
      fail("parse off tone roundtrip failed");
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
    if (sbx_parse_mix_fx_spec("triangle:mixpulse:1/50", SBX_WAVE_SINE, &fx) != SBX_OK)
      fail("mix fx parse failed");
    if (fx.type != SBX_MIXFX_PULSE || fx.waveform != SBX_WAVE_TRIANGLE)
      fail("mix fx parse value mismatch");
  }
  {
    SbxMixFxSpec fx;
    if (sbx_parse_mix_fx_spec("mixam:8:s=0:d=0.5:a=0.1:r=0.1:e=3:f=0.25",
                              SBX_WAVE_SINE, &fx) != SBX_OK)
      fail("mixam parse failed");
    if (fx.type != SBX_MIXFX_AM)
      fail("mixam parse type mismatch");
    if (fabs(fx.res - 8.0) > 1e-9 || fabs(fx.mixam_floor - 0.25) > 1e-9)
      fail("mixam parse value mismatch");
    if (sbx_parse_mix_fx_spec("mixam:beat:s=0:d=0.5:a=0.1:r=0.1:e=3:f=0.25",
                              SBX_WAVE_SINE, &fx) != SBX_OK)
      fail("mixam beat-bind parse failed");
    if (!fx.mixam_bind_program_beat)
      fail("mixam beat-bind parse flag mismatch");
    if (sbx_parse_mix_fx_spec("mixpulse:beat:s=0:d=0.5:a=0.1:r=0.1:e=3:f=0.25",
                              SBX_WAVE_SINE, &fx) != SBX_OK)
      fail("mixpulse:beat compatibility parse failed");
    if (fx.type != SBX_MIXFX_AM || !fx.mixam_bind_program_beat)
      fail("mixpulse:beat compatibility parse mismatch");
    if (sbx_parse_mix_fx_spec("mixam:8:m=cos:s=0:f=0.4",
                              SBX_WAVE_SINE, &fx) != SBX_OK)
      fail("mixam cos parse failed");
    if (fx.mixam_mode != SBX_MIXAM_MODE_COS || fabs(fx.mixam_floor - 0.4) > 1e-9)
      fail("mixam cos parse value mismatch");
    if (sbx_parse_mix_fx_spec("mixam:6:a=0.8:r=0.5", SBX_WAVE_SINE, &fx) != SBX_EINVAL)
      fail("mixam a+r constraint should fail");
  }
  {
    int typ = SBX_EXTRA_INVALID;
    SbxToneSpec tone;
    SbxMixFxSpec fx;
    SbxMixFxSpec fx_rt;
    char spec[256];
    double mixpct = 0.0;
    if (sbx_parse_extra_token("mix/87", SBX_WAVE_SINE, &typ, &tone, &fx, &mixpct) != SBX_OK)
      fail("parse extra mix token failed");
    if (typ != SBX_EXTRA_MIXAMP || fabs(mixpct - 87.0) > 1e-9)
      fail("parse extra mix token mismatch");
    if (sbx_parse_extra_token("sawtooth:mixbeat:2/30", SBX_WAVE_SINE, &typ, &tone, &fx, &mixpct) != SBX_OK)
      fail("parse extra mixfx token failed");
    if (typ != SBX_EXTRA_MIXFX || fx.type != SBX_MIXFX_BEAT || fx.waveform != SBX_WAVE_SAWTOOTH)
      fail("parse extra mixfx token mismatch");
    if (sbx_parse_extra_token("mixam:6:d=0.5:a=0.1:r=0.1:e=3:f=0.2",
                              SBX_WAVE_SINE, &typ, &tone, &fx, &mixpct) != SBX_OK)
      fail("parse extra mixam token failed");
    if (typ != SBX_EXTRA_MIXFX || fx.type != SBX_MIXFX_AM)
      fail("parse extra mixam token mismatch");
    if (sbx_parse_extra_token("mixam:beat:d=0.5:a=0.1:r=0.1:e=3:f=0.2",
                              SBX_WAVE_SINE, &typ, &tone, &fx, &mixpct) != SBX_OK)
      fail("parse extra beat-bound mixam token failed");
    if (typ != SBX_EXTRA_MIXFX || fx.type != SBX_MIXFX_AM || !fx.mixam_bind_program_beat)
      fail("parse extra beat-bound mixam token mismatch");
    if (sbx_parse_extra_token("mixpulse:beat:d=0.5:a=0.1:r=0.1:e=3:f=0.2",
                              SBX_WAVE_SINE, &typ, &tone, &fx, &mixpct) != SBX_OK)
      fail("parse extra beat-bound mixpulse compatibility token failed");
    if (typ != SBX_EXTRA_MIXFX || fx.type != SBX_MIXFX_AM || !fx.mixam_bind_program_beat)
      fail("parse extra beat-bound mixpulse compatibility token mismatch");
    if (sbx_parse_extra_token("mixam:beat:m=cos:s=0.1:f=0.35",
                              SBX_WAVE_SINE, &typ, &tone, &fx, &mixpct) != SBX_OK)
      fail("parse extra beat-bound mixam cos token failed");
    if (typ != SBX_EXTRA_MIXFX || fx.type != SBX_MIXFX_AM ||
        !fx.mixam_bind_program_beat || fx.mixam_mode != SBX_MIXAM_MODE_COS)
      fail("parse extra beat-bound mixam cos token mismatch");
    if (sbx_parse_extra_token("triangle:200+8/20", SBX_WAVE_SINE, &typ, &tone, &fx, &mixpct) != SBX_OK)
      fail("parse extra tone token failed");
    if (typ != SBX_EXTRA_TONE || tone.mode != SBX_TONE_BINAURAL || tone.waveform != SBX_WAVE_TRIANGLE)
      fail("parse extra tone token mismatch");
    if (sbx_parse_mix_fx_spec("sawtooth:mixspin:400+6/40", SBX_WAVE_SINE, &fx) != SBX_OK)
      fail("mix fx parse for format failed");
    if (sbx_format_mix_fx_spec(&fx, spec, sizeof(spec)) != SBX_OK)
      fail("mix fx format failed");
    if (sbx_parse_mix_fx_spec(spec, SBX_WAVE_SINE, &fx_rt) != SBX_OK)
      fail("mix fx parse after format failed");
    if (fx_rt.type != fx.type || fx_rt.waveform != fx.waveform)
      fail("mix fx format roundtrip type/wave mismatch");
    if (fabs(fx_rt.carr - fx.carr) > 1e-9 || fabs(fx_rt.res - fx.res) > 1e-9 || fabs(fx_rt.amp - fx.amp) > 1e-9)
      fail("mix fx format roundtrip value mismatch");
    if (sbx_parse_mix_fx_spec("mixam:7:d=0.6:a=0.2:r=0.2:e=2:f=0.1", SBX_WAVE_SINE, &fx) != SBX_OK)
      fail("mixam parse for format failed");
    if (sbx_format_mix_fx_spec(&fx, spec, sizeof(spec)) != SBX_OK)
      fail("mixam format failed");
    if (sbx_parse_mix_fx_spec(spec, SBX_WAVE_SINE, &fx_rt) != SBX_OK)
      fail("mixam parse after format failed");
    if (fx_rt.type != SBX_MIXFX_AM || fabs(fx_rt.res - 7.0) > 1e-9)
      fail("mixam format roundtrip type mismatch");
    if (fabs(fx_rt.mixam_duty - 0.6) > 1e-9 || fabs(fx_rt.mixam_floor - 0.1) > 1e-9)
      fail("mixam format roundtrip value mismatch");
    if (sbx_parse_mix_fx_spec("mixam:beat:d=0.6:a=0.2:r=0.2:e=2:f=0.1", SBX_WAVE_SINE, &fx) != SBX_OK)
      fail("mixam beat-bind parse for format failed");
    if (sbx_format_mix_fx_spec(&fx, spec, sizeof(spec)) != SBX_OK)
      fail("mixam beat-bind format failed");
    if (sbx_parse_mix_fx_spec(spec, SBX_WAVE_SINE, &fx_rt) != SBX_OK)
      fail("mixam beat-bind parse after format failed");
    if (!fx_rt.mixam_bind_program_beat)
      fail("mixam beat-bind format roundtrip flag mismatch");
    if (sbx_parse_mix_fx_spec("mixam:7:m=cos:s=0.2:f=0.3", SBX_WAVE_SINE, &fx) != SBX_OK)
      fail("mixam cos parse for format failed");
    if (sbx_format_mix_fx_spec(&fx, spec, sizeof(spec)) != SBX_OK)
      fail("mixam cos format failed");
    if (sbx_parse_mix_fx_spec(spec, SBX_WAVE_SINE, &fx_rt) != SBX_OK)
      fail("mixam cos parse after format failed");
    if (fx_rt.mixam_mode != SBX_MIXAM_MODE_COS || fabs(fx_rt.mixam_floor - 0.3) > 1e-9)
      fail("mixam cos format roundtrip mismatch");
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
  {
    SbxMixAmpKeyframe mkf[2];
    double v;
    mkf[0].time_sec = 0.0;
    mkf[0].amp_pct = 100.0;
    mkf[0].interp = SBX_INTERP_LINEAR;
    mkf[1].time_sec = 10.0;
    mkf[1].amp_pct = 50.0;
    mkf[1].interp = SBX_INTERP_LINEAR;
    if (sbx_context_set_mix_amp_keyframes(ctx, mkf, 2, 80.0) != SBX_OK)
      fail("set mix amp keyframes failed");
    v = sbx_context_mix_amp_at(ctx, 5.0);
    if (!(v > 70.0 && v < 80.0))
      fail("mix amp interpolation mismatch");
    if (sbx_context_set_mix_amp_keyframes(ctx, 0, 0, 77.0) != SBX_OK)
      fail("clear mix amp keyframes failed");
    v = sbx_context_mix_amp_at(ctx, 5.0);
    if (fabs(v - 77.0) > 1e-9)
      fail("mix amp default mismatch");
  }
  {
    SbxProgramKeyframe kf[2];
    SbxToneSpec t;
    if (sbx_parse_tone_spec("200+10/20", &t) != SBX_OK)
      fail("parse tone for duration failed");
    kf[0].time_sec = 0.0;
    kf[0].tone = t;
    kf[0].interp = SBX_INTERP_LINEAR;
    kf[1].time_sec = 5.0;
    kf[1].tone = t;
    kf[1].interp = SBX_INTERP_STEP;
    if (sbx_context_load_keyframes(ctx, kf, 2, 0) != SBX_OK)
      fail("load keyframes for duration failed");
    if (fabs(sbx_context_duration_sec(ctx) - 5.0) > 1e-9)
      fail("context duration mismatch");
  }
  {
    SbxToneSpec t;
    if (sbx_parse_tone_spec("200+10/20", &t) != SBX_OK)
      fail("parse tone for static duration failed");
    if (sbx_context_set_tone(ctx, &t) != SBX_OK)
      fail("set tone for static duration failed");
    if (fabs(sbx_context_duration_sec(ctx)) > 1e-9)
      fail("static tone duration should be zero");
  }
  {
    SbxMixAmpKeyframe mkf[2];
    SbxMixFxSpec fx;
    SbxToneSpec aux;
    double add_l = 0.0, add_r = 0.0;
    int rc;
    mkf[0].time_sec = 0.0;
    mkf[0].amp_pct = 100.0;
    mkf[0].interp = SBX_INTERP_LINEAR;
    mkf[1].time_sec = 10.0;
    mkf[1].amp_pct = 50.0;
    mkf[1].interp = SBX_INTERP_LINEAR;
    if (sbx_parse_mix_fx_spec("mixpulse:2/40", SBX_WAVE_SINE, &fx) != SBX_OK)
      fail("parse mix fx for runtime config failed");
    if (sbx_parse_tone_spec("250/10", &aux) != SBX_OK)
      fail("parse aux for runtime config failed");
    rc = sbx_context_configure_runtime(ctx,
                                       mkf, 2, 80.0,
                                       &fx, 1,
                                       &aux, 1);
    if (rc != SBX_OK)
      fail("configure runtime failed");
    if (sbx_context_mix_stream_sample(ctx, 5.0, 1600, -1200, 1.0, &add_l, &add_r) != SBX_OK)
      fail("mix stream sample failed");
    if (!(fabs(add_l) > 1e-9 || fabs(add_r) > 1e-9))
      fail("mix stream sample produced zero contribution");
    if (sbx_context_configure_runtime(ctx, 0, 0, 100.0, 0, 0, 0, 0) != SBX_OK)
      fail("clear runtime config failed");
  }
  {
    SbxMixFxSpec fx;
    double add_l = 0.0, add_r = 0.0;
    double max_abs_l = 0.0;
    double min_abs_l = 1e9;
    int i;
    if (sbx_parse_mix_fx_spec("mixam:2:d=0.5:a=0:r=0:e=0:f=0", SBX_WAVE_SINE, &fx) != SBX_OK)
      fail("parse mixam for runtime sample failed");
    if (sbx_context_configure_runtime(ctx, 0, 0, 100.0, &fx, 1, 0, 0) != SBX_OK)
      fail("configure runtime with mixam failed");
    for (i = 0; i < 50000; i++) {
      if (sbx_context_mix_stream_sample(ctx, (double)i / cfg.sample_rate,
                                        1600, -1200, 1.0, &add_l, &add_r) != SBX_OK)
        fail("mix stream sample with mixam failed");
      {
        double al = fabs(add_l);
        if (al > max_abs_l) max_abs_l = al;
        if (al < min_abs_l) min_abs_l = al;
      }
      if (fabs(add_l) > 100.0001 || fabs(add_r) > 75.0001)
        fail("mixam should not amplify above dry stream");
    }
    if (!(max_abs_l > 90.0))
      fail("mixam should preserve near-full level during ON phase");
    if (!(min_abs_l < 5.0))
      fail("mixam should gate near zero during OFF phase");
    if (sbx_context_configure_runtime(ctx, 0, 0, 100.0, 0, 0, 0, 0) != SBX_OK)
      fail("clear runtime config after mixam failed");
  }
  {
    SbxProgramKeyframe kf[2];
    SbxToneSpec t0, t1;
    SbxMixFxSpec fx;
    int i;
    int edges_early = 0, edges_late = 0;
    int prev_on = 0;
    if (sbx_parse_tone_spec("200+2/20", &t0) != SBX_OK)
      fail("parse start tone for beat-bound mixam failed");
    if (sbx_parse_tone_spec("200+8/20", &t1) != SBX_OK)
      fail("parse end tone for beat-bound mixam failed");
    kf[0].time_sec = 0.0;
    kf[0].tone = t0;
    kf[0].interp = SBX_INTERP_LINEAR;
    kf[1].time_sec = 10.0;
    kf[1].tone = t1;
    kf[1].interp = SBX_INTERP_LINEAR;
    if (sbx_context_load_keyframes(ctx, kf, 2, 0) != SBX_OK)
      fail("load keyframes for beat-bound mixam failed");
    if (sbx_parse_mix_fx_spec("mixam:beat:d=0.5:a=0:r=0:e=0:f=0", SBX_WAVE_SINE, &fx) != SBX_OK)
      fail("parse beat-bound mixam for runtime sample failed");

    if (sbx_context_configure_runtime(ctx, 0, 0, 100.0, &fx, 1, 0, 0) != SBX_OK)
      fail("configure runtime with beat-bound mixam (early window) failed");
    prev_on = 0;
    for (i = 0; i < (int)cfg.sample_rate; i++) {
      double add_l = 0.0, add_r = 0.0;
      int on;
      if (sbx_context_mix_stream_sample(ctx, (double)i / cfg.sample_rate,
                                        1600, -1200, 1.0, &add_l, &add_r) != SBX_OK)
        fail("mix stream sample with beat-bound mixam (early window) failed");
      on = (fabs(add_l) > 50.0) ? 1 : 0;
      if (on && !prev_on) edges_early++;
      prev_on = on;
    }

    if (sbx_context_configure_runtime(ctx, 0, 0, 100.0, &fx, 1, 0, 0) != SBX_OK)
      fail("configure runtime with beat-bound mixam (late window) failed");
    prev_on = 0;
    for (i = 0; i < (int)cfg.sample_rate; i++) {
      double t = 9.0 + (double)i / cfg.sample_rate;
      double add_l = 0.0, add_r = 0.0;
      int on;
      if (sbx_context_mix_stream_sample(ctx, t, 1600, -1200, 1.0, &add_l, &add_r) != SBX_OK)
        fail("mix stream sample with beat-bound mixam (late window) failed");
      on = (fabs(add_l) > 50.0) ? 1 : 0;
      if (on && !prev_on) edges_late++;
      prev_on = on;
    }

    if (!(edges_late > edges_early + 2))
      fail("beat-bound mixam should track rising program beat frequency");
    if (sbx_context_configure_runtime(ctx, 0, 0, 100.0, 0, 0, 0, 0) != SBX_OK)
      fail("clear runtime config after beat-bound mixam failed");
  }
  {
    SbxMixFxSpec fx;
    double add_l = 0.0, add_r = 0.0;
    double max_abs_l = 0.0;
    double min_abs_l = 1e9;
    int i;
    if (sbx_parse_mix_fx_spec("mixam:2:m=cos:s=0:f=0.4", SBX_WAVE_SINE, &fx) != SBX_OK)
      fail("parse mixam cos for runtime sample failed");
    if (sbx_context_configure_runtime(ctx, 0, 0, 100.0, &fx, 1, 0, 0) != SBX_OK)
      fail("configure runtime with mixam cos failed");
    for (i = 0; i < 50000; i++) {
      if (sbx_context_mix_stream_sample(ctx, (double)i / cfg.sample_rate,
                                        1600, -1200, 1.0, &add_l, &add_r) != SBX_OK)
        fail("mix stream sample with mixam cos failed");
      {
        double al = fabs(add_l);
        if (al > max_abs_l) max_abs_l = al;
        if (al < min_abs_l) min_abs_l = al;
      }
      if (fabs(add_l) > 100.0001 || fabs(add_r) > 75.0001)
        fail("mixam cos should not amplify above dry stream");
    }
    if (!(max_abs_l > 95.0))
      fail("mixam cos should reach near full level");
    if (!(min_abs_l > 35.0 && min_abs_l < 45.0))
      fail("mixam cos floor behavior mismatch");
    if (sbx_context_configure_runtime(ctx, 0, 0, 100.0, 0, 0, 0, 0) != SBX_OK)
      fail("clear runtime config after mixam cos failed");
  }

  buf = (float *)calloc(frames * 2, sizeof(float));
  if (!buf) fail("alloc failed");

  {
    SbxContext *ctx0 = sbx_context_create(&cfg);
    if (!ctx0) fail("context create failed (ctx0)");
    if (sbx_context_render_f32(ctx0, buf, frames) != SBX_ENOTREADY)
      fail("render should fail with ENOTREADY before load");
    sbx_context_destroy(ctx0);
  }

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
