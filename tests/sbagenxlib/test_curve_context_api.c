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
  SbxEngineConfig eng_cfg;
  SbxCurveEvalConfig curve_cfg;
  SbxCurveSourceConfig src_cfg;
  SbxContext *ctx;
  SbxCurveProgram *curve;
  SbxToneSpec tones[3];
  double ts[3];
  double beat_hz[3];
  double mix_pct[3];
  float audio[512];
  int rc;

  sbx_default_engine_config(&eng_cfg);
  ctx = sbx_context_create(&eng_cfg);
  if (!ctx) fail("sbx_context_create failed");

  curve = sbx_curve_create();
  if (!curve) fail("sbx_curve_create failed");
  rc = sbx_curve_load_text(
      curve,
      "beat = b0*exp(ln(b1/b0)*(m/D))\n"
      "carrier = c0 + (c1-c0)*ramp(m,0,T)\n"
      "mixamp = m0*(1-ramp(m,0,T))\n",
      "curve-context-test.sbgf");
  if (rc != SBX_OK) fail(sbx_curve_last_error(curve));

  sbx_default_curve_eval_config(&curve_cfg);
  curve_cfg.carrier_start_hz = 210.0;
  curve_cfg.carrier_end_hz = 200.0;
  curve_cfg.carrier_span_sec = 120.0;
  curve_cfg.beat_start_hz = 10.0;
  curve_cfg.beat_target_hz = 2.5;
  curve_cfg.beat_span_sec = 120.0;
  curve_cfg.hold_min = 0.0;
  curve_cfg.total_min = 2.0;
  curve_cfg.wake_min = 0.0;
  curve_cfg.beat_amp0_pct = 80.0;
  curve_cfg.mix_amp0_pct = 97.0;

  rc = sbx_curve_prepare(curve, &curve_cfg);
  if (rc != SBX_OK) fail(sbx_curve_last_error(curve));

  sbx_default_curve_source_config(&src_cfg);
  src_cfg.mode = SBX_TONE_BINAURAL;
  src_cfg.waveform = SBX_WAVE_SINE;
  src_cfg.amplitude = 1.0;
  src_cfg.duration_sec = 130.0;
  src_cfg.loop = 0;

  rc = sbx_context_load_curve_program(ctx, curve, &src_cfg);
  if (rc != SBX_OK) fail(sbx_context_last_error(ctx));
  curve = NULL; /* ownership transferred */

  if (sbx_context_source_mode(ctx) != SBX_SOURCE_CURVE)
    fail("curve context source mode mismatch");
  if (!near(sbx_context_duration_sec(ctx), 130.0, 1e-9))
    fail("curve context duration mismatch");
  if (sbx_context_is_looping(ctx))
    fail("curve context should not be looping");

  rc = sbx_context_sample_program_beat(ctx, 0.0, 120.0, 3, ts, beat_hz);
  if (rc != SBX_OK) fail("sbx_context_sample_program_beat failed");
  if (!near(ts[0], 0.0, 1e-12) || !near(ts[1], 60.0, 1e-12) || !near(ts[2], 120.0, 1e-12))
    fail("curve program beat sample times mismatch");
  if (!near(beat_hz[0], 10.0, 1e-9) || !near(beat_hz[1], 5.0, 1e-9) || !near(beat_hz[2], 2.5, 1e-9))
    fail("curve program beat sample values mismatch");

  rc = sbx_context_sample_tones(ctx, 0.0, 120.0, 3, ts, tones);
  if (rc != SBX_OK) fail("sbx_context_sample_tones failed");
  if (!near(tones[0].carrier_hz, 210.0, 1e-9) ||
      !near(tones[1].carrier_hz, 205.0, 1e-9) ||
      !near(tones[2].carrier_hz, 200.0, 1e-9))
    fail("curve context carrier sample mismatch");
  if (!near(tones[0].amplitude, 0.8, 1e-9) ||
      !near(tones[1].amplitude, 0.8, 1e-9) ||
      !near(tones[2].amplitude, 0.8, 1e-9))
    fail("curve context amplitude sample mismatch");

  rc = sbx_context_sample_mix_amp(ctx, 0.0, 120.0, 3, ts, mix_pct);
  if (rc != SBX_OK) fail("sbx_context_sample_mix_amp failed");
  if (!near(mix_pct[0], 97.0, 1e-9) ||
      !near(mix_pct[1], 48.5, 1e-9) ||
      !near(mix_pct[2], 0.0, 1e-9))
    fail("curve context mix amp sample mismatch");
  if (!sbx_context_has_mix_amp_control(ctx))
    fail("curve context should report mix amp control");

  rc = sbx_context_render_f32(ctx, audio, 256);
  if (rc != SBX_OK) fail("sbx_context_render_f32 failed");

  sbx_context_destroy(ctx);
  if (curve) sbx_curve_destroy(curve);

  puts("PASS: curve-backed context API checks");
  return 0;
}
