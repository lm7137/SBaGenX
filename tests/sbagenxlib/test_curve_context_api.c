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
  SbxMixFxSpec fx_cfg[4];
  SbxMixFxSpec sampled_fx[8];
  size_t sampled_fx_count = 0;
  SbxToneSpec tones[3];
  double ts[3];
  double beat_hz[3];
  double mix_pct[3];
  float audio[512];
  double mix_add_l = 0.0, mix_add_r = 0.0;
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
      "mixamp = m0*(1-ramp(m,0,T))\n"
      "mixspin_width = 300 + 60*ramp(m,0,T)\n"
      "mixspin_hz = 3 + 1*ramp(m,0,T)\n"
      "mixspin_amp = 35 + 15*ramp(m,0,T)\n"
      "mixpulse_hz = 6 - 2*ramp(m,0,T)\n"
      "mixpulse_amp = 40 + 10*ramp(m,0,T)\n"
      "mixbeat_hz = 4 + 2*ramp(m,0,T)\n"
      "mixbeat_amp = 45 - 5*ramp(m,0,T)\n"
      "mixam_hz = 2 + 2*ramp(m,0,T)\n",
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

  if (sbx_parse_mix_fx_spec("mixspin:300+3/35", SBX_WAVE_SINE, &fx_cfg[0]) != SBX_OK)
    fail("parse mixspin fx failed");
  if (sbx_parse_mix_fx_spec("mixpulse:6/40", SBX_WAVE_SINE, &fx_cfg[1]) != SBX_OK)
    fail("parse mixpulse fx failed");
  if (sbx_parse_mix_fx_spec("mixbeat:4/45", SBX_WAVE_SINE, &fx_cfg[2]) != SBX_OK)
    fail("parse mixbeat fx failed");
  if (sbx_parse_mix_fx_spec("mixam:beat:m=cos:s=0.1:f=0.3", SBX_WAVE_SINE, &fx_cfg[3]) != SBX_OK)
    fail("parse mixam fx failed");
  rc = sbx_context_set_mix_effects(ctx, fx_cfg, 4);
  if (rc != SBX_OK) fail(sbx_context_last_error(ctx));

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

  rc = sbx_context_sample_mix_effects(ctx, 0.0, sampled_fx, 8, &sampled_fx_count);
  if (rc != SBX_OK) fail("sbx_context_sample_mix_effects t=0 failed");
  if (sampled_fx_count != 4) fail("curve context mix effect count mismatch");
  if (!near(sampled_fx[0].carr, 300.0, 1e-9) ||
      !near(sampled_fx[0].res, 3.0, 1e-9) ||
      !near(sampled_fx[0].amp, 0.35, 1e-9))
    fail("curve mixspin override at t=0 mismatch");
  if (!near(sampled_fx[1].res, 6.0, 1e-9) ||
      !near(sampled_fx[1].amp, 0.40, 1e-9))
    fail("curve mixpulse override at t=0 mismatch");
  if (!near(sampled_fx[2].res, 4.0, 1e-9) ||
      !near(sampled_fx[2].amp, 0.45, 1e-9))
    fail("curve mixbeat override at t=0 mismatch");
  if (!near(sampled_fx[3].res, 2.0, 1e-9) || sampled_fx[3].mixam_bind_program_beat)
    fail("curve mixam override at t=0 mismatch");

  rc = sbx_context_sample_mix_effects(ctx, 60.0, sampled_fx, 8, &sampled_fx_count);
  if (rc != SBX_OK) fail("sbx_context_sample_mix_effects t=60 failed");
  if (!near(sampled_fx[0].carr, 330.0, 1e-9) ||
      !near(sampled_fx[0].res, 3.5, 1e-9) ||
      !near(sampled_fx[0].amp, 0.425, 1e-9))
    fail("curve mixspin override at t=60 mismatch");
  if (!near(sampled_fx[1].res, 5.0, 1e-9) ||
      !near(sampled_fx[1].amp, 0.45, 1e-9))
    fail("curve mixpulse override at t=60 mismatch");
  if (!near(sampled_fx[2].res, 5.0, 1e-9) ||
      !near(sampled_fx[2].amp, 0.425, 1e-9))
    fail("curve mixbeat override at t=60 mismatch");
  if (!near(sampled_fx[3].res, 3.0, 1e-9) || sampled_fx[3].mixam_bind_program_beat)
    fail("curve mixam override at t=60 mismatch");

  rc = sbx_context_sample_mix_effects(ctx, 120.0, sampled_fx, 8, &sampled_fx_count);
  if (rc != SBX_OK) fail("sbx_context_sample_mix_effects t=120 failed");
  if (!near(sampled_fx[0].carr, 360.0, 1e-9) ||
      !near(sampled_fx[0].res, 4.0, 1e-9) ||
      !near(sampled_fx[0].amp, 0.50, 1e-9))
    fail("curve mixspin override at t=120 mismatch");
  if (!near(sampled_fx[1].res, 4.0, 1e-9) ||
      !near(sampled_fx[1].amp, 0.50, 1e-9))
    fail("curve mixpulse override at t=120 mismatch");
  if (!near(sampled_fx[2].res, 6.0, 1e-9) ||
      !near(sampled_fx[2].amp, 0.40, 1e-9))
    fail("curve mixbeat override at t=120 mismatch");
  if (!near(sampled_fx[3].res, 4.0, 1e-9) || sampled_fx[3].mixam_bind_program_beat)
    fail("curve mixam override at t=120 mismatch");

  rc = sbx_context_mix_stream_sample(ctx, 60.0, 1200, -900, 1.0, &mix_add_l, &mix_add_r);
  if (rc != SBX_OK) fail("sbx_context_mix_stream_sample failed");
  if (!isfinite(mix_add_l) || !isfinite(mix_add_r))
    fail("curve mix stream sample produced non-finite output");

  rc = sbx_context_render_f32(ctx, audio, 256);
  if (rc != SBX_OK) fail("sbx_context_render_f32 failed");

  sbx_context_destroy(ctx);
  if (curve) sbx_curve_destroy(curve);

  puts("PASS: curve-backed context API checks");
  return 0;
}
