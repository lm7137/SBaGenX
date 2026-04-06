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

static double stereo_delta_ratio(const float *buf, size_t frames) {
  size_t i;
  double diff = 0.0;
  double total = 0.0;
  for (i = 0; i < frames; i++) {
    double l = fabs((double)buf[i * 2]);
    double r = fabs((double)buf[i * 2 + 1]);
    diff += fabs((double)buf[i * 2] - (double)buf[i * 2 + 1]);
    total += l + r;
  }
  if (total <= 1e-12) return 0.0;
  return diff / total;
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
  assert_parse("spin00:spin:120+1.25/35", SBX_TONE_SPIN_PINK, 120.0, 1.25, 0.35, 0, SBX_WAVE_SPIN_BASE);
  {
    SbxToneSpec t;
    if (sbx_parse_tone_spec("noise00/20", &t) != SBX_OK)
      fail("parse noiseNN tone failed");
    if (t.mode != SBX_TONE_WHITE_NOISE || t.noise_waveform != SBX_NOISE_WAVE_BASE)
      fail("noiseNN tone parse mismatch");
    if (sbx_parse_tone_spec("spin00:noise00:spin:120+1.25/35", &t) != SBX_OK)
      fail("parse spinNN+noiseNN tone failed");
    if (t.mode != SBX_TONE_SPIN_WHITE ||
        t.waveform != SBX_WAVE_SPIN_BASE ||
        t.noise_waveform != SBX_NOISE_WAVE_BASE)
      fail("spinNN+noiseNN tone parse mismatch");
    if (sbx_parse_tone_spec("custom00:noise00:noisepulse:4/20", &t) != SBX_OK)
      fail("parse custom noisepulse tone failed");
    if (t.mode != SBX_TONE_NOISE_PULSE ||
        t.envelope_waveform != SBX_ENV_WAVE_CUSTOM_BASE ||
        t.noise_waveform != SBX_NOISE_WAVE_BASE ||
        fabs(t.beat_hz - 4.0) > 1e-9 ||
        fabs(t.amplitude - 0.20) > 1e-9)
      fail("custom noisepulse tone parse mismatch");
    if (sbx_parse_tone_spec("custom00:triangle:noise00:noisebeat:4/20", &t) != SBX_OK)
      fail("parse custom noisebeat tone failed");
    if (t.mode != SBX_TONE_NOISE_BEAT ||
        t.envelope_waveform != SBX_ENV_WAVE_CUSTOM_BASE ||
        t.noise_waveform != SBX_NOISE_WAVE_BASE ||
        t.waveform != SBX_WAVE_TRIANGLE ||
        fabs(t.beat_hz - 4.0) > 1e-9 ||
        fabs(t.amplitude - 0.20) > 1e-9)
      fail("custom noisebeat tone parse mismatch");
  }
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
    SbxToneSpec in, out;
    char spec[256];
    if (sbx_parse_tone_spec("spin00:spin:120+1.25/35", &in) != SBX_OK)
      fail("parse spinNN tone failed");
    if (sbx_format_tone_spec(&in, spec, sizeof(spec)) != SBX_OK)
      fail("format spinNN tone failed");
    if (strcmp(spec, "spin00:spin:120+1.25/35") != 0)
      fail("format spinNN tone should preserve prefix");
    if (sbx_parse_tone_spec(spec, &out) != SBX_OK)
      fail("parse spinNN tone roundtrip failed");
    if (out.mode != in.mode || out.waveform != in.waveform)
      fail("spinNN tone roundtrip waveform mismatch");
  }
  {
    SbxToneSpec in, out;
    char spec[256];
    if (sbx_parse_tone_spec("noise00/20", &in) != SBX_OK)
      fail("parse noiseNN tone failed");
    if (sbx_format_tone_spec(&in, spec, sizeof(spec)) != SBX_OK)
      fail("format noiseNN tone failed");
    if (strcmp(spec, "noise00/20") != 0)
      fail("format noiseNN tone should preserve prefix");
    if (sbx_parse_tone_spec(spec, &out) != SBX_OK)
      fail("parse noiseNN tone roundtrip failed");
    if (out.mode != in.mode || out.noise_waveform != in.noise_waveform)
      fail("noiseNN tone roundtrip mismatch");
    if (sbx_parse_tone_spec("spin00:noise00:spin:120+1.25/35", &in) != SBX_OK)
      fail("parse spinNN+noiseNN tone failed");
    if (sbx_format_tone_spec(&in, spec, sizeof(spec)) != SBX_OK)
      fail("format spinNN+noiseNN tone failed");
    if (strcmp(spec, "spin00:noise00:spin:120+1.25/35") != 0)
      fail("format spinNN+noiseNN tone should preserve prefixes");
    if (sbx_parse_tone_spec("custom00:noise00:noisepulse:4/20", &in) != SBX_OK)
      fail("parse custom noisepulse tone failed");
    if (sbx_format_tone_spec(&in, spec, sizeof(spec)) != SBX_OK)
      fail("format custom noisepulse tone failed");
    if (strcmp(spec, "custom00:noise00:noisepulse:4/20") != 0)
      fail("format custom noisepulse tone should preserve prefixes");
    if (sbx_parse_tone_spec("custom00:triangle:noise00:noisebeat:4/20", &in) != SBX_OK)
      fail("parse custom noisebeat tone failed");
    if (sbx_format_tone_spec(&in, spec, sizeof(spec)) != SBX_OK)
      fail("format custom noisebeat tone failed");
    if (strcmp(spec, "custom00:noise00:triangle:noisebeat:4/20") != 0)
      fail("format custom noisebeat tone should preserve prefixes");
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
    if (sbx_parse_mix_fx_spec("custom00:mixpulse:1/50", SBX_WAVE_SINE, &fx) != SBX_OK)
      fail("custom mixpulse parse failed");
    if (fx.type != SBX_MIXFX_PULSE ||
        fx.waveform != SBX_WAVE_SINE ||
        fx.envelope_waveform != SBX_ENV_WAVE_CUSTOM_BASE)
      fail("custom mixpulse parse value mismatch");
    if (sbx_parse_mix_fx_spec("custom00:triangle:mixspin:400+6/40", SBX_WAVE_SINE, &fx) != SBX_OK)
      fail("custom mixspin parse failed");
    if (fx.type != SBX_MIXFX_SPIN ||
        fx.waveform != SBX_WAVE_TRIANGLE ||
        fx.envelope_waveform != SBX_ENV_WAVE_CUSTOM_BASE)
      fail("custom mixspin parse value mismatch");
    if (sbx_parse_mix_fx_spec("custom00:mixbeat:3/25", SBX_WAVE_SINE, &fx) != SBX_OK)
      fail("custom mixbeat parse failed");
    if (fx.type != SBX_MIXFX_BEAT ||
        fx.waveform != SBX_WAVE_SINE ||
        fx.envelope_waveform != SBX_ENV_WAVE_CUSTOM_BASE)
      fail("custom mixbeat parse value mismatch");
    if (sbx_parse_mix_fx_spec("custom00:mixam:8", SBX_WAVE_SINE, &fx) != SBX_EINVAL)
      fail("custom envelope should be rejected for mixam");
  }
  {
    SbxMixFxSpec fx;
    if (sbx_parse_mix_fx_spec("mixam:8", SBX_WAVE_SINE, &fx) != SBX_OK)
      fail("mixam default parse failed");
    if (fx.type != SBX_MIXFX_AM || fx.mixam_mode != SBX_MIXAM_MODE_COS ||
        fabs(fx.mixam_start - 0.0) > 1e-9 || fabs(fx.mixam_floor - 0.45) > 1e-9)
      fail("mixam default parse value mismatch");
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
    if (sbx_parse_mix_fx_spec("custom00:triangle:mixspin:400+6/40", SBX_WAVE_SINE, &fx) != SBX_OK)
      fail("custom mix fx parse for format failed");
    if (sbx_format_mix_fx_spec(&fx, spec, sizeof(spec)) != SBX_OK)
      fail("custom mix fx format failed");
    if (strcmp(spec, "custom00:triangle:mixspin:400+6/40") != 0)
      fail("custom mix fx format should preserve both prefixes");
    if (sbx_parse_mix_fx_spec(spec, SBX_WAVE_SINE, &fx_rt) != SBX_OK)
      fail("custom mix fx parse after format failed");
    if (fx_rt.type != fx.type || fx_rt.waveform != fx.waveform ||
        fx_rt.envelope_waveform != fx.envelope_waveform)
      fail("custom mix fx format roundtrip mismatch");
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
  if (sbx_context_source_mode(ctx) != SBX_SOURCE_NONE)
    fail("new context should report source none");
  if (sbx_context_is_looping(ctx) != 0)
    fail("new context should not report looping");
  if (sbx_context_set_default_waveform(ctx, 99) != SBX_EINVAL)
    fail("invalid default waveform should fail");
  if (sbx_context_set_default_waveform(ctx, SBX_WAVE_TRIANGLE) != SBX_OK)
    fail("setting default waveform failed");

  {
    const char *sbg_mix_text =
        "wash: mix/65 mixbeat:3/25 180+0/15\n"
        "00:00 wash\n";
    if (sbx_context_load_sbg_timing_text(ctx, sbg_mix_text, 0) != SBX_OK)
      fail("sbg mix timing load failed");
    if (sbx_context_source_mode(ctx) != SBX_SOURCE_KEYFRAMES)
      fail("sbg timing load should report keyframed source");
    if (sbx_context_is_looping(ctx) != 0)
      fail("non-looping sbg timing load should not report looping");
    if (!sbx_context_has_mix_amp_control(ctx))
      fail("sbg mix timing should expose mix amp control");
    if (!sbx_context_has_mix_effects(ctx))
      fail("sbg mix timing should expose mix effects");
  }
  {
    const char *sbg_tone_text =
        "00:00 180+0/20\n";
    if (sbx_context_load_sbg_timing_text(ctx, sbg_tone_text, 0) != SBX_OK)
      fail("sbg tone-only timing load failed");
    if (sbx_context_has_mix_amp_control(ctx))
      fail("tone-only sbg timing should not expose mix amp control");
    if (sbx_context_has_mix_effects(ctx))
      fail("tone-only sbg timing should not expose mix effects");
  }
  {
    const char *sbg_mix_timed_text =
        "pulse: mix/65 mixbeat:3/25 180+0/15\n"
        "wash: mix/40 mixbeat:2/30 mixspin:400+4/20 200+0/12\n"
        "00:00 pulse\n"
        "00:00:10 wash step\n";
    SbxMixAmpKeyframe mkf;
    SbxTimedMixFxKeyframeInfo fx_info;
    SbxMixFxSpec fx_slot;
    int present = 0;
    if (sbx_context_load_sbg_timing_text(ctx, sbg_mix_timed_text, 0) != SBX_OK)
      fail("timed sbg mix load failed");
    if (sbx_context_mix_amp_keyframe_count(ctx) != 2)
      fail("timed sbg mix should expose two mix amp keyframes");
    if (sbx_context_get_mix_amp_keyframe(ctx, 0, &mkf) != SBX_OK)
      fail("mix amp keyframe 0 retrieval failed");
    if (fabs(mkf.time_sec - 0.0) > 1e-9 || fabs(mkf.amp_pct - 65.0) > 1e-9 || mkf.interp != SBX_INTERP_LINEAR)
      fail("mix amp keyframe 0 mismatch");
    if (sbx_context_get_mix_amp_keyframe(ctx, 1, &mkf) != SBX_OK)
      fail("mix amp keyframe 1 retrieval failed");
    if (fabs(mkf.time_sec - 10.0) > 1e-9 || fabs(mkf.amp_pct - 40.0) > 1e-9 || mkf.interp != SBX_INTERP_STEP)
      fail("mix amp keyframe 1 mismatch");
    if (sbx_context_get_mix_amp_keyframe(ctx, 2, &mkf) != SBX_EINVAL)
      fail("out-of-range mix amp keyframe retrieval should fail");
    if (sbx_context_timed_mix_effect_keyframe_count(ctx) != 2)
      fail("timed sbg mix should expose two timed mix-effect keyframes");
    if (sbx_context_timed_mix_effect_slot_count(ctx) != 2)
      fail("timed sbg mix should expose two timed mix-effect slots");
    if (sbx_context_get_timed_mix_effect_keyframe_info(ctx, 0, &fx_info) != SBX_OK)
      fail("timed mix-effect keyframe info 0 retrieval failed");
    if (fabs(fx_info.time_sec - 0.0) > 1e-9 || fx_info.interp != SBX_INTERP_LINEAR)
      fail("timed mix-effect keyframe info 0 mismatch");
    if (sbx_context_get_timed_mix_effect_keyframe_info(ctx, 1, &fx_info) != SBX_OK)
      fail("timed mix-effect keyframe info 1 retrieval failed");
    if (fabs(fx_info.time_sec - 10.0) > 1e-9 || fx_info.interp != SBX_INTERP_STEP)
      fail("timed mix-effect keyframe info 1 mismatch");
    if (sbx_context_get_timed_mix_effect_keyframe_info(ctx, 2, &fx_info) != SBX_EINVAL)
      fail("out-of-range timed mix-effect keyframe info retrieval should fail");
    if (sbx_context_get_timed_mix_effect_slot(ctx, 0, 0, &fx_slot, &present) != SBX_OK)
      fail("timed mix-effect slot 0,0 retrieval failed");
    if (!present || fx_slot.type != SBX_MIXFX_BEAT || fabs(fx_slot.res - 3.0) > 1e-9)
      fail("timed mix-effect slot 0,0 mismatch");
    if (sbx_context_get_timed_mix_effect_slot(ctx, 0, 1, &fx_slot, &present) != SBX_OK)
      fail("timed mix-effect slot 0,1 retrieval failed");
    if (present || fx_slot.type != SBX_MIXFX_NONE)
      fail("timed mix-effect slot 0,1 should be empty");
    if (sbx_context_get_timed_mix_effect_slot(ctx, 1, 1, &fx_slot, &present) != SBX_OK)
      fail("timed mix-effect slot 1,1 retrieval failed");
    if (!present || fx_slot.type != SBX_MIXFX_SPIN || fabs(fx_slot.carr - 400.0) > 1e-9 || fabs(fx_slot.res - 4.0) > 1e-9)
      fail("timed mix-effect slot 1,1 mismatch");
    if (sbx_context_get_timed_mix_effect_slot(ctx, 1, 2, &fx_slot, &present) != SBX_EINVAL)
      fail("out-of-range timed mix-effect slot retrieval should fail");
  }
  {
    const char *sbg_multivoice_text =
        "duo: 180+0/20 260+0/15\n"
        "00:00 duo\n";
    SbxProgramKeyframe kf_lane;
    if (sbx_context_load_sbg_timing_text(ctx, sbg_multivoice_text, 0) != SBX_OK)
      fail("multivoice sbg timing load failed");
    if (sbx_context_voice_count(ctx) != 2)
      fail("multivoice sbg timing should expose two voice lanes");
    if (sbx_context_get_keyframe_voice(ctx, 0, 1, &kf_lane) != SBX_OK)
      fail("secondary voice retrieval failed");
    if (fabs(kf_lane.tone.carrier_hz - 260.0) > 1e-9 || fabs(kf_lane.tone.amplitude - 0.15) > 1e-9)
      fail("secondary voice values mismatch");
    if (sbx_context_get_keyframe_voice(ctx, 0, 2, &kf_lane) != SBX_EINVAL)
      fail("out-of-range voice retrieval should fail");
  }

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
    const char *sbg_custom_mix_text =
        "custom00: e=2 0 0.2 1 0.2 0\n"
        "pulse: mix/65 custom00:mixpulse:2/40 180+0/15\n"
        "wash: mix/40 custom00:triangle:mixspin:400+4/20 200+0/12\n"
        "beat: mix/55 custom00:mixbeat:3/25 180+0/15\n"
        "NOW pulse ==\n"
        "+00:00:02 wash ==\n"
        "+00:00:04 beat\n";
    SbxTimedMixFxKeyframeInfo fx_info;
    if (sbx_context_load_sbg_timing_text(ctx, sbg_custom_mix_text, 0) != SBX_OK)
      fail("custom mixfx sbg timing load failed");
    if (sbx_context_timed_mix_effect_keyframe_count(ctx) != 3)
      fail("custom mixfx timing should expose 3 timed mix-effect keyframes");
    if (sbx_context_get_timed_mix_effect_keyframe_info(ctx, 0, &fx_info) != SBX_OK)
      fail("custom mixfx first timed keyframe info failed");
    if (fabs(fx_info.time_sec - 0.0) > 1e-9)
      fail("custom mixfx first timed keyframe time mismatch");
  }
  {
    const char *noise_text =
        "noise00: 12 12 11 11 10 10 9 8 7 6 5 4 3 2 1 0 -1 -2 -3 -4 -5 -6 -7 -8 -9 -10 -11 -12 -12 -12 -12 -12\n"
        "base: noise00/20\n"
        "spin00: e=0 0 0 0 100 100 -100 -100\n"
        "wash: spin00:noise00:spin:300+1/35\n"
        "NOW base\n"
        "+00:01 wash\n";
    SbxContext *ctx_noise = sbx_context_create(&cfg);
    SbxContext *ctx_white = sbx_context_create(&cfg);
    float *buf_noise;
    float *buf_white;
    double diff = 0.0;
    size_t i;
    if (!ctx_noise || !ctx_white)
      fail("alloc failed (noiseNN contexts)");
    if (sbx_context_load_sbg_timing_text(ctx_noise, noise_text, 0) != SBX_OK)
      fail("noiseNN timing load failed");
    if (sbx_context_load_sbg_timing_text(ctx_white, "base: white/20\nNOW base\n", 0) != SBX_OK)
      fail("white timing load failed");
    buf_noise = (float *)calloc(frames * 2, sizeof(float));
    buf_white = (float *)calloc(frames * 2, sizeof(float));
    if (!buf_noise || !buf_white)
      fail("alloc failed (noiseNN buffers)");
    if (sbx_context_render_f32(ctx_noise, buf_noise, frames) != SBX_OK)
      fail("noiseNN render failed");
    if (sbx_context_render_f32(ctx_white, buf_white, frames) != SBX_OK)
      fail("white render failed");
    if (!has_energy(buf_noise, frames * 2))
      fail("noiseNN render should produce non-zero energy");
    for (i = 0; i < frames * 2; i++)
      diff += fabs((double)buf_noise[i] - (double)buf_white[i]);
    if (!(diff > 1.0))
      fail("noiseNN render should differ materially from white noise");
    free(buf_noise);
    free(buf_white);
    sbx_context_destroy(ctx_noise);
    sbx_context_destroy(ctx_white);
  }
  {
    if (sbx_context_load_sbg_timing_text(ctx, "00:00 200+2/20\n", 0) != SBX_OK)
      fail("reset context after custom mixfx timing load failed");
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
    SbxMixModSpec mod;
    double base_pct, eff_pct, mul;
    sbx_default_mix_mod_spec(&mod);
    mod.active = 1;
    mod.main_len_sec = 120.0;
    mod.wake_len_sec = 0.0;
    mod.wake_enabled = 0;
    if (sbx_context_set_mix_amp_keyframes(ctx, 0, 0, 97.0) != SBX_OK)
      fail("set default mix amp before mix modulation failed");
    if (sbx_context_set_mix_mod(ctx, &mod) != SBX_OK)
      fail("set mix modulation failed");
    if (!sbx_context_has_mix_mod(ctx))
      fail("mix modulation should report active");
    if (sbx_context_get_mix_mod(ctx, &mod) != SBX_OK)
      fail("get mix modulation failed");
    if (!mod.active || fabs(mod.end_level - 0.7) > 1e-9)
      fail("get mix modulation mismatch");
    base_pct = sbx_context_mix_amp_at(ctx, 10.0);
    mul = sbx_context_mix_mod_mul_at(ctx, 10.0);
    eff_pct = sbx_context_mix_amp_effective_at(ctx, 10.0);
    if (fabs(base_pct - 97.0) > 1e-9)
      fail("base mix amp should remain unchanged by mix modulation");
    if (!(mul < 1.0 && mul > 0.0))
      fail("mix modulation multiplier should attenuate during main phase");
    if (!(eff_pct < base_pct))
      fail("effective mix amp should include mix modulation");
    mul = sbx_context_mix_mod_mul_at(ctx, 120.0);
    if (fabs(mul - 0.7) > 1e-9)
      fail("mix modulation should hold end level after main phase when no wake is present");
    mod.wake_enabled = 1;
    mod.wake_len_sec = 30.0;
    if (sbx_context_set_mix_mod(ctx, &mod) != SBX_OK)
      fail("set wake mix modulation failed");
    mul = sbx_context_mix_mod_mul_at(ctx, 120.0);
    if (fabs(mul - 0.7) > 1e-9)
      fail("wake mix modulation should start at end level");
    mul = sbx_context_mix_mod_mul_at(ctx, 150.0);
    if (fabs(mul - 1.0) > 1e-9)
      fail("wake mix modulation should end at full level");
    if (sbx_context_set_mix_mod(ctx, 0) != SBX_OK)
      fail("clear mix modulation failed");
    if (sbx_context_has_mix_mod(ctx))
      fail("mix modulation should report inactive after clear");
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
    if (sbx_context_source_mode(ctx) != SBX_SOURCE_KEYFRAMES)
      fail("load keyframes should report keyframed source");
    if (sbx_context_is_looping(ctx) != 0)
      fail("non-looping keyframes should not report looping");
    if (fabs(sbx_context_duration_sec(ctx) - 5.0) > 1e-9)
      fail("context duration mismatch");
    if (sbx_context_load_keyframes(ctx, kf, 2, 1) != SBX_OK)
      fail("load looped keyframes for loop state failed");
    if (sbx_context_is_looping(ctx) != 1)
      fail("looped keyframes should report looping");
  }
  {
    SbxToneSpec t;
    if (sbx_parse_tone_spec("200+10/20", &t) != SBX_OK)
      fail("parse tone for static duration failed");
    if (sbx_context_set_tone(ctx, &t) != SBX_OK)
      fail("set tone for static duration failed");
    if (sbx_context_source_mode(ctx) != SBX_SOURCE_STATIC)
      fail("set tone should report static source");
    if (sbx_context_is_looping(ctx) != 0)
      fail("static tone should not report looping");
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
  if (sbx_context_set_time_sec(ctx, 1.5) != SBX_OK)
    fail("set time after static tone load failed");
  if (fabs(sbx_context_time_sec(ctx) - 1.5) > 1e-9)
    fail("set time getter mismatch for static tone");
  if (sbx_context_set_time_sec(ctx, -1.0) != SBX_EINVAL)
    fail("negative set time should fail");
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

  {
    const size_t spin_frames = (size_t)(cfg.sample_rate * 4.0);
    float *spin_sine = (float *)calloc(spin_frames * 2, sizeof(float));
    float *spin_square = (float *)calloc(spin_frames * 2, sizeof(float));
    double ratio_sine, ratio_square;
    if (!spin_sine || !spin_square)
      fail("alloc failed (spin compare)");
    sbx_context_reset(ctx);
    if (sbx_context_load_tone_spec(ctx, "spin:300+0.5/20") != SBX_OK)
      fail("load sine spin tone spec failed");
    if (sbx_context_render_f32(ctx, spin_sine, spin_frames) != SBX_OK)
      fail("render sine spin failed");
    sbx_context_reset(ctx);
    if (sbx_context_load_tone_spec(ctx, "square:spin:300+0.5/20") != SBX_OK)
      fail("load square spin tone spec failed");
    if (sbx_context_render_f32(ctx, spin_square, spin_frames) != SBX_OK)
      fail("render square spin failed");
    ratio_sine = stereo_delta_ratio(spin_sine, spin_frames);
    ratio_square = stereo_delta_ratio(spin_square, spin_frames);
    if (!(ratio_sine > 0.01 && ratio_sine < 0.40))
      fail("default sine spin should not saturate into square-like stereo switching");
    if (!(ratio_square > ratio_sine * 1.7))
      fail("square spin should produce a stronger stereo delta than default spin");
    free(spin_sine);
    free(spin_square);
  }

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

  {
    SbxProgramKeyframe kfs[2];
    SbxToneSpec tone_at_seek;
    memset(kfs, 0, sizeof(kfs));
    sbx_default_tone_spec(&kfs[0].tone);
    sbx_default_tone_spec(&kfs[1].tone);
    kfs[0].time_sec = 0.0;
    kfs[0].tone.carrier_hz = 180.0;
    kfs[0].tone.beat_hz = 10.0;
    kfs[0].tone.amplitude = 0.2;
    kfs[0].interp = SBX_INTERP_LINEAR;
    kfs[1].time_sec = 10.0;
    kfs[1].tone.carrier_hz = 220.0;
    kfs[1].tone.beat_hz = 6.0;
    kfs[1].tone.amplitude = 0.2;
    kfs[1].interp = SBX_INTERP_LINEAR;
    if (sbx_context_load_keyframes(ctx, kfs, 2, 0) != SBX_OK)
      fail("load keyframes for set time failed");
    if (sbx_context_set_time_sec(ctx, 5.25) != SBX_OK)
      fail("set time after keyframe load failed");
    if (fabs(sbx_context_time_sec(ctx) - 5.25) > 1e-9)
      fail("set time getter mismatch for keyframes");
    if (sbx_context_sample_tones(ctx, 5.25, 5.25, 1, NULL, &tone_at_seek) != SBX_OK)
      fail("sample tones after keyframe set time failed");
    if (!(tone_at_seek.carrier_hz > 180.0 && tone_at_seek.carrier_hz < 220.0))
      fail("sampled carrier after keyframe set time is out of expected range");
    if (!(tone_at_seek.beat_hz > 6.0 && tone_at_seek.beat_hz < 10.0))
      fail("sampled beat after keyframe set time is out of expected range");
  }

  free(buf);
  sbx_context_destroy(ctx);

  printf("PASS: sbagenxlib context API checks\n");
  return 0;
}
