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
  SbxCurveFileProgramConfig file_cfg;
  SbxCurveParamOverride ovs[2];
  SbxCurveProgram *curve = 0;
  SbxCurveInfo info;
  SbxCurveEvalPoint pt0, pt1;
  SbxCurveTimelineConfig tl_cfg;
  SbxCurveTimeline tl;
  SbxEngineConfig eng_cfg;
  SbxContext *ctx = 0;
  SbxCurveSourceConfig src_cfg;
  SbxToneSpec tone;
  int rc;

  sbx_default_curve_file_program_config(&file_cfg);
  file_cfg.path = "examples/basics/curve-sigmoid-like.sbgf";
  ovs[0].name = "l";
  ovs[0].value = 0.2;
  ovs[1].name = "h";
  ovs[1].value = 0.0;
  file_cfg.overrides = ovs;
  file_cfg.override_count = 2;
  file_cfg.eval_config.carrier_start_hz = 205.0;
  file_cfg.eval_config.carrier_end_hz = 200.0;
  file_cfg.eval_config.carrier_span_sec = 1800.0;
  file_cfg.eval_config.beat_start_hz = 10.0;
  file_cfg.eval_config.beat_target_hz = 0.3;
  file_cfg.eval_config.beat_span_sec = 1800.0;
  file_cfg.eval_config.hold_min = 0.0;
  file_cfg.eval_config.total_min = 30.0;
  file_cfg.eval_config.wake_min = 0.0;
  file_cfg.eval_config.beat_amp0_pct = 100.0;
  file_cfg.eval_config.mix_amp0_pct = 100.0;

  rc = sbx_prepare_curve_file_program(&file_cfg, &curve);
  if (rc != SBX_OK || !curve) fail("sbx_prepare_curve_file_program failed");
  rc = sbx_curve_get_info(curve, &info);
  if (rc != SBX_OK) fail("sbx_curve_get_info failed");
  if (!info.has_carrier_expr) fail("prepared curve should report carrier expression");

  rc = sbx_curve_eval(curve, 0.0, &pt0);
  if (rc != SBX_OK) fail("sbx_curve_eval start failed");
  rc = sbx_curve_eval(curve, 1800.0, &pt1);
  if (rc != SBX_OK) fail("sbx_curve_eval end failed");
  if (!near(pt0.carrier_hz, 205.0, 1e-6) ||
      !near(pt1.carrier_hz, 200.0, 1e-6))
    fail("prepared curve carrier endpoints mismatch");
  if (!near(pt0.beat_hz, 9.97602, 1e-5) ||
      !near(pt1.beat_hz, 0.323984, 1e-6))
    fail("prepared curve beat endpoint mismatch");

  sbx_default_curve_timeline_config(&tl_cfg);
  sbx_default_tone_spec(&tone);
  tone.mode = SBX_TONE_BINAURAL;
  tone.waveform = SBX_WAVE_SINE;
  tone.carrier_hz = 205.0;
  tone.beat_hz = 10.0;
  tone.amplitude = 1.0;
  tl_cfg.start_tone = tone;
  tl_cfg.sample_span_sec = 1800;
  tl_cfg.main_span_sec = 1800;
  tl_cfg.step_len_sec = 60;
  tl_cfg.slide = 1;
  tl_cfg.fade_sec = 10;
  tl_cfg.mute_program_tone = 0;

  rc = sbx_build_curve_timeline(curve, &tl_cfg, &tl);
  if (rc != SBX_OK) fail("sbx_build_curve_timeline failed");
  if (tl.program_frame_count < 3) fail("curve timeline frame count too small");
  if (tl.mix_frame_count != 0) fail("curve timeline unexpectedly produced mixamp keyframes");
  if (!near(tl.program_frames[0].time_sec, 0.0, 1e-12))
    fail("curve timeline first frame time mismatch");
  if (!near(tl.program_frames[tl.program_frame_count - 2].time_sec, 1800.0, 1e-9))
    fail("curve timeline main-end frame time mismatch");
  if (!near(tl.program_frames[tl.program_frame_count - 1].time_sec, 1810.0, 1e-9))
    fail("curve timeline fade frame time mismatch");
  if (!near(tl.program_frames[tl.program_frame_count - 1].tone.amplitude, 0.0, 1e-12))
    fail("curve timeline fade frame amplitude mismatch");

  sbx_default_engine_config(&eng_cfg);
  ctx = sbx_context_create(&eng_cfg);
  if (!ctx) fail("sbx_context_create failed");
  sbx_default_curve_source_config(&src_cfg);
  src_cfg.mode = SBX_TONE_BINAURAL;
  src_cfg.waveform = SBX_WAVE_SINE;
  src_cfg.amplitude = 1.0;
  src_cfg.duration_sec = 1810.0;
  rc = sbx_context_load_curve_program(ctx, curve, &src_cfg);
  if (rc != SBX_OK) fail("sbx_context_load_curve_program failed");
  curve = 0; /* ownership transferred */
  if (sbx_context_source_mode(ctx) != SBX_SOURCE_CURVE)
    fail("curve helper should feed exact curve runtime source");

  sbx_free_curve_timeline(&tl);
  sbx_context_destroy(ctx);
  if (curve) sbx_curve_destroy(curve);
  return 0;
}
