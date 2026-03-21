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
near(double a, double b, double eps) {
  return fabs(a - b) <= eps;
}

int
main(void) {
  SbxRuntimeContextConfig rt_cfg;
  SbxImmediateParseConfig parse_cfg;
  SbxImmediateSpec imm;
  SbxMixModSpec mix_mod;
  SbxContext *ctx = 0;
  SbxProgramKeyframe kfs[2];
  SbxToneSpec tone;
  SbxCurveProgram *curve = 0;
  SbxCurveEvalConfig curve_eval;
  SbxCurveSourceConfig curve_src;
  double total_sec = 0.0;
  char err[256];
  int rc;

  sbx_default_runtime_context_config(&rt_cfg);
  if (!near(rt_cfg.engine.sample_rate, 44100.0, 1e-9) ||
      rt_cfg.engine.channels != 2 ||
      !near(rt_cfg.default_mix_amp_pct, 100.0, 1e-9))
    fail("default runtime context config mismatch");

  err[0] = 0;
  rc = sbx_validate_runtime_mix_fx_requirements(0, 1, 1, "-i", "tones", err, sizeof(err));
  if (rc != SBX_EINVAL || strstr(err, "mix input stream") == 0)
    fail("expected missing mix input validation failure");
  err[0] = 0;
  rc = sbx_validate_runtime_mix_fx_requirements(1, 0, 1, "-p drop", "extra tone-specs", err, sizeof(err));
  if (rc != SBX_EINVAL || strstr(err, "mix/<amp> in extra tone-specs") == 0)
    fail("expected missing mix/<amp> validation failure");
  err[0] = 0;
  rc = sbx_validate_runtime_mix_fx_requirements(1, 1, 1, "-i", "tones", err, sizeof(err));
  if (rc != SBX_OK)
    fail("runtime mix-fx validation should pass");

  sbx_default_immediate_parse_config(&parse_cfg);
  {
    const char *tokens[] = {"mix/87", "triangle:200+8/20", "200@4/30"};
    err[0] = 0;
    rc = sbx_parse_immediate_tokens(tokens, 3, &parse_cfg, &imm, err, sizeof(err));
    if (rc != SBX_OK)
      fail(err[0] ? err : "sbx_parse_immediate_tokens failed");
  }

  sbx_default_mix_mod_spec(&mix_mod);
  mix_mod.active = 1;
  mix_mod.delta = 0.3;
  mix_mod.epsilon = 0.3;
  mix_mod.period_sec = 10.0;
  mix_mod.end_level = 0.7;
  mix_mod.main_len_sec = 60.0;
  mix_mod.wake_len_sec = 0.0;
  mix_mod.wake_enabled = 0;

  sbx_default_runtime_context_config(&rt_cfg);
  rt_cfg.mix_mod = &mix_mod;
  rc = sbx_runtime_context_create_from_immediate(&imm, &rt_cfg, &ctx);
  if (rc != SBX_OK || !ctx)
    fail("sbx_runtime_context_create_from_immediate failed");
  if (sbx_context_source_mode(ctx) != SBX_SOURCE_STATIC)
    fail("immediate runtime source mode mismatch");
  if (sbx_context_aux_tone_count(ctx) != 1)
    fail("immediate runtime aux tone count mismatch");
  if (!near(sbx_context_mix_amp_at(ctx, 0.0), 87.0, 1e-9))
    fail("immediate runtime mix amp mismatch");
  if (!sbx_context_has_mix_mod(ctx))
    fail("immediate runtime mix mod missing");
  if (!(sbx_context_mix_amp_effective_at(ctx, 30.0) < 87.0))
    fail("immediate runtime effective mix amp should be modulated");
  sbx_context_destroy(ctx);
  ctx = 0;

  sbx_default_tone_spec(&tone);
  tone.mode = SBX_TONE_BINAURAL;
  tone.carrier_hz = 205.0;
  tone.beat_hz = 8.0;
  tone.amplitude = 0.2;
  kfs[0].time_sec = 0.0;
  kfs[0].tone = tone;
  kfs[0].interp = SBX_INTERP_LINEAR;
  tone.carrier_hz = 200.0;
  tone.beat_hz = 4.0;
  kfs[1].time_sec = 5.0;
  kfs[1].tone = tone;
  kfs[1].interp = SBX_INTERP_LINEAR;

  sbx_default_runtime_context_config(&rt_cfg);
  rt_cfg.default_mix_amp_pct = 76.0;
  rc = sbx_runtime_context_create_from_keyframes(kfs, 2, 0, &rt_cfg, &total_sec, &ctx);
  if (rc != SBX_OK || !ctx) {
    if (ctx && sbx_context_last_error(ctx))
      fail(sbx_context_last_error(ctx));
    fail("sbx_runtime_context_create_from_keyframes failed");
  }
  if (sbx_context_source_mode(ctx) != SBX_SOURCE_KEYFRAMES)
    fail("keyframed runtime source mode mismatch");
  if (!near(total_sec, 5.0, 1e-9))
    fail("keyframed runtime total_sec mismatch");
  if (!near(sbx_context_mix_amp_at(ctx, 0.0), 76.0, 1e-9))
    fail("keyframed runtime default mix amp mismatch");
  sbx_context_destroy(ctx);
  ctx = 0;

  curve = sbx_curve_create();
  if (!curve)
    fail("sbx_curve_create failed");
  rc = sbx_curve_load_text(curve,
                           "beat = b0\n"
                           "carrier = c0\n",
                           "runtime-context-test.sbgf");
  if (rc != SBX_OK)
    fail(sbx_curve_last_error(curve));
  sbx_default_curve_eval_config(&curve_eval);
  curve_eval.carrier_start_hz = 210.0;
  curve_eval.carrier_end_hz = 200.0;
  curve_eval.carrier_span_sec = 60.0;
  curve_eval.beat_start_hz = 6.0;
  curve_eval.beat_target_hz = 3.0;
  curve_eval.beat_span_sec = 60.0;
  curve_eval.hold_min = 0.0;
  curve_eval.total_min = 1.0;
  curve_eval.wake_min = 0.0;
  curve_eval.beat_amp0_pct = 100.0;
  curve_eval.mix_amp0_pct = 100.0;
  rc = sbx_curve_prepare(curve, &curve_eval);
  if (rc != SBX_OK)
    fail(sbx_curve_last_error(curve));

  sbx_default_curve_source_config(&curve_src);
  curve_src.mode = SBX_TONE_ISOCHRONIC;
  curve_src.waveform = SBX_WAVE_SINE;
  curve_src.duty_cycle = 0.35;
  curve_src.iso_start = 0.1;
  curve_src.iso_attack = 0.2;
  curve_src.iso_release = 0.6;
  curve_src.iso_edge_mode = 2;
  curve_src.amplitude = 1.0;
  curve_src.duration_sec = 12.0;

  sbx_default_runtime_context_config(&rt_cfg);
  rc = sbx_runtime_context_create_from_curve_program(curve, &curve_src, &rt_cfg, &total_sec, &ctx);
  if (rc != SBX_OK || !ctx)
    fail("sbx_runtime_context_create_from_curve_program failed");
  curve = 0; /* ownership transferred */
  if (sbx_context_source_mode(ctx) != SBX_SOURCE_CURVE)
    fail("curve runtime source mode mismatch");
  if (!near(total_sec, 12.0, 1e-9))
    fail("curve runtime total_sec mismatch");
  if (!near(sbx_context_duration_sec(ctx), 12.0, 1e-9))
    fail("curve runtime duration mismatch");
  sbx_context_destroy(ctx);

  if (curve)
    sbx_curve_destroy(curve);

  puts("PASS: sbagenxlib runtime context helpers activate native sources");
  return 0;
}
