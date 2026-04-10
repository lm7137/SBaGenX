#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sbagenxlib.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static void fail(const char *msg) {
  fprintf(stderr, "FAIL: %s\n", msg);
  exit(1);
}

static double naive_wave_sample(int waveform, double phase_unit) {
  phase_unit = fmod(phase_unit, 1.0);
  if (phase_unit < 0.0) phase_unit += 1.0;
  switch (waveform) {
    case SBX_WAVE_SQUARE:
      return (phase_unit < 0.5) ? 1.0 : -1.0;
    case SBX_WAVE_TRIANGLE:
      if (phase_unit < 0.25) return phase_unit * 4.0;
      if (phase_unit < 0.75) return 2.0 - phase_unit * 4.0;
      return phase_unit * 4.0 - 4.0;
    case SBX_WAVE_SAWTOOTH:
      return -1.0 + 2.0 * phase_unit;
    default:
      return sin(phase_unit * 2.0 * M_PI);
  }
}

static double alias_energy_ratio(const float *buf, int frames, int waveform, int fundamental_bin) {
  int k, i;
  int bins = frames / 2;
  double total = 0.0;
  double harmonic = 0.0;
  for (k = 1; k < bins; ++k) {
    double re = 0.0;
    double im = 0.0;
    for (i = 0; i < frames; ++i) {
      double ang = -2.0 * M_PI * (double)k * (double)i / (double)frames;
      double v = (double)buf[i * 2];
      re += v * cos(ang);
      im += v * sin(ang);
    }
    {
      double mag2 = re * re + im * im;
      int ord = (fundamental_bin > 0 && (k % fundamental_bin) == 0) ? (k / fundamental_bin) : 0;
      int keep = 0;
      if (ord >= 1) {
        if (waveform == SBX_WAVE_SAWTOOTH) keep = 1;
        else if ((ord & 1) == 1) keep = 1;
      }
      total += mag2;
      if (keep) harmonic += mag2;
    }
  }
  if (total <= 0.0) return 0.0;
  return (total - harmonic) / total;
}

static void render_runtime_wave(int waveform,
                                double sample_rate,
                                double carrier_hz,
                                float *buf,
                                int frames) {
  SbxEngineConfig cfg;
  SbxContext *ctx;
  char spec[128];
  const char *prefix = "";
  sbx_default_engine_config(&cfg);
  cfg.sample_rate = sample_rate;
  ctx = sbx_context_create(&cfg);
  if (!ctx)
    fail("context create failed");
  if (waveform == SBX_WAVE_SQUARE) prefix = "square:";
  else if (waveform == SBX_WAVE_TRIANGLE) prefix = "triangle:";
  else if (waveform == SBX_WAVE_SAWTOOTH) prefix = "sawtooth:";
  snprintf(spec, sizeof(spec), "%s%.15g/100", prefix, carrier_hz);
  if (sbx_context_load_tone_spec(ctx, spec) != SBX_OK)
    fail("runtime tone load failed");
  memset(buf, 0, sizeof(float) * (size_t)frames * 2);
  if (sbx_context_render_f32(ctx, buf, (size_t)frames) != SBX_OK)
    fail("runtime render failed");
  sbx_context_destroy(ctx);
}

static void render_naive_wave(int waveform,
                              double sample_rate,
                              double carrier_hz,
                              float *buf,
                              int frames) {
  int i;
  double phase_unit = 0.0;
  double inc = carrier_hz / sample_rate;
  memset(buf, 0, sizeof(float) * (size_t)frames * 2);
  for (i = 0; i < frames; ++i) {
    float v = (float)naive_wave_sample(waveform, phase_unit);
    buf[i * 2] = v;
    buf[i * 2 + 1] = v;
    phase_unit += inc;
    phase_unit -= floor(phase_unit);
  }
}

static void assert_wave_alias_improves(int waveform,
                                       const char *name,
                                       double max_ratio) {
  enum { FRAMES = 2048, FUNDAMENTAL_BIN = 175 };
  const double sample_rate = 44100.0;
  const double carrier_hz = sample_rate * (double)FUNDAMENTAL_BIN / (double)FRAMES;
  float *runtime = (float *)calloc((size_t)FRAMES * 2, sizeof(float));
  float *naive = (float *)calloc((size_t)FRAMES * 2, sizeof(float));
  double runtime_alias;
  double naive_alias;
  double ratio;
  if (!runtime || !naive)
    fail("alloc failed");
  render_runtime_wave(waveform, sample_rate, carrier_hz, runtime, FRAMES);
  render_naive_wave(waveform, sample_rate, carrier_hz, naive, FRAMES);
  runtime_alias = alias_energy_ratio(runtime, FRAMES, waveform, FUNDAMENTAL_BIN);
  naive_alias = alias_energy_ratio(naive, FRAMES, waveform, FUNDAMENTAL_BIN);
  if (!(naive_alias > 0.0))
    fail("naive alias baseline missing");
  ratio = runtime_alias / naive_alias;
  if (!(ratio < max_ratio)) {
    fprintf(stderr,
            "FAIL: %s alias ratio too high runtime=%0.6f naive=%0.6f ratio=%0.3f threshold=%0.3f\n",
            name, runtime_alias, naive_alias, ratio, max_ratio);
    exit(1);
  }
  free(runtime);
  free(naive);
}

int main(void) {
  assert_wave_alias_improves(SBX_WAVE_SQUARE, "square", 0.05);
  assert_wave_alias_improves(SBX_WAVE_TRIANGLE, "triangle", 0.25);
  assert_wave_alias_improves(SBX_WAVE_SAWTOOTH, "sawtooth", 0.10);
  printf("PASS: sbagenxlib runtime oscillator alias checks\n");
  return 0;
}
