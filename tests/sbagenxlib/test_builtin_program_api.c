#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "sbagenxlib.h"

static void fail(const char *msg) {
  fprintf(stderr, "FAIL: %s\n", msg);
  exit(1);
}

static int near(double a, double b, double eps) {
  return fabs(a - b) <= eps;
}

int main(void) {
  SbxBuiltinDropConfig drop_cfg;
  SbxBuiltinSigmoidConfig sig_cfg;
  SbxBuiltinSlideConfig slide_cfg;
  SbxProgramKeyframe *kfs = NULL;
  size_t kf_count = 0;
  SbxCurveProgram *curve = NULL;
  SbxCurveEvalPoint pt;
  double sig_a = 0.0, sig_b = 0.0;
  int rc;

  sbx_default_builtin_drop_config(&drop_cfg);
  drop_cfg.start_tone.mode = SBX_TONE_BINAURAL;
  drop_cfg.start_tone.carrier_hz = 205.0;
  drop_cfg.start_tone.beat_hz = 10.0;
  drop_cfg.start_tone.amplitude = 1.0;
  drop_cfg.carrier_end_hz = 200.0;
  drop_cfg.beat_target_hz = 2.5;
  drop_cfg.drop_sec = 600;
  drop_cfg.hold_sec = 120;
  drop_cfg.wake_sec = 60;
  drop_cfg.slide = 0;
  drop_cfg.step_len_sec = 200;
  drop_cfg.fade_sec = 2.5;

  rc = sbx_build_drop_keyframes(&drop_cfg, &kfs, &kf_count);
  if (rc != SBX_OK) fail("sbx_build_drop_keyframes failed");
  if (kf_count != 6) fail("drop keyframe count mismatch");
  if (!near(kfs[0].time_sec, 0.0, 1e-12) ||
      !near(kfs[1].time_sec, 200.0, 1e-12) ||
      !near(kfs[2].time_sec, 400.0, 1e-12) ||
      !near(kfs[3].time_sec, 720.0, 1e-12) ||
      !near(kfs[4].time_sec, 780.0, 1e-12) ||
      !near(kfs[5].time_sec, 782.5, 1e-12))
    fail("drop keyframe times mismatch");
  if (!near(kfs[0].tone.beat_hz, 10.0, 1e-9) ||
      !near(kfs[1].tone.beat_hz, 5.0, 1e-9) ||
      !near(kfs[2].tone.beat_hz, 2.5, 1e-9) ||
      !near(kfs[5].tone.amplitude, 0.0, 1e-12))
    fail("drop keyframe values mismatch");
  free(kfs);
  kfs = NULL;

  drop_cfg.slide = 1;
  drop_cfg.hold_sec = 0;
  drop_cfg.wake_sec = 0;
  rc = sbx_build_drop_curve_program(&drop_cfg, &curve);
  if (rc != SBX_OK || !curve) fail("sbx_build_drop_curve_program failed");
  rc = sbx_curve_eval(curve, 300.0, &pt);
  if (rc != SBX_OK) fail("drop curve eval failed");
  if (!near(pt.beat_hz, 5.0, 1e-9)) fail("drop curve midpoint beat mismatch");
  if (!near(pt.carrier_hz, 202.5, 1e-9)) fail("drop curve midpoint carrier mismatch");
  sbx_curve_destroy(curve);
  curve = NULL;

  if (sbx_compute_sigmoid_coefficients(1800, 10.0, 2.5, 0.125, 0.0, &sig_a, &sig_b) != SBX_OK)
    fail("sbx_compute_sigmoid_coefficients failed");
  if (!near(sig_a, -3.93063113095, 1e-6) || !near(sig_b, 6.25, 1e-9))
    fail("sigmoid coefficient mismatch");

  sbx_default_builtin_sigmoid_config(&sig_cfg);
  sig_cfg.start_tone.mode = SBX_TONE_BINAURAL;
  sig_cfg.start_tone.carrier_hz = 205.0;
  sig_cfg.start_tone.beat_hz = 10.0;
  sig_cfg.start_tone.amplitude = 1.0;
  sig_cfg.carrier_end_hz = 200.0;
  sig_cfg.beat_target_hz = 2.5;
  sig_cfg.drop_sec = 1800;
  sig_cfg.slide = 1;
  sig_cfg.step_len_sec = 600;
  sig_cfg.fade_sec = 2.5;
  sig_cfg.sig_l = 0.125;
  sig_cfg.sig_h = 0.0;

  rc = sbx_build_sigmoid_curve_program(&sig_cfg, &curve);
  if (rc != SBX_OK || !curve) fail("sbx_build_sigmoid_curve_program failed");
  rc = sbx_curve_eval(curve, 900.0, &pt);
  if (rc != SBX_OK) fail("sigmoid curve eval failed");
  if (!near(pt.beat_hz, 6.25, 1e-6)) fail("sigmoid midpoint beat mismatch");
  sbx_curve_destroy(curve);
  curve = NULL;

  sig_cfg.slide = 0;
  sig_cfg.wake_sec = 180;
  rc = sbx_build_sigmoid_keyframes(&sig_cfg, &kfs, &kf_count);
  if (rc != SBX_OK) fail("sbx_build_sigmoid_keyframes failed");
  if (kf_count != 6) fail("sigmoid keyframe count mismatch");
  if (!near(kfs[3].time_sec, 1800.0, 1e-12) ||
      !near(kfs[4].time_sec, 1980.0, 1e-12) ||
      !near(kfs[5].time_sec, 1982.5, 1e-12))
    fail("sigmoid keyframe tail mismatch");
  free(kfs);
  kfs = NULL;

  sbx_default_builtin_slide_config(&slide_cfg);
  slide_cfg.start_tone.mode = SBX_TONE_BINAURAL;
  slide_cfg.start_tone.carrier_hz = 200.0;
  slide_cfg.start_tone.beat_hz = -10.0;
  slide_cfg.start_tone.amplitude = 1.0;
  slide_cfg.carrier_end_hz = 5.0;
  slide_cfg.slide_sec = 600;
  slide_cfg.fade_sec = 2.5;
  rc = sbx_build_slide_keyframes(&slide_cfg, &kfs, &kf_count);
  if (rc != SBX_OK) fail("sbx_build_slide_keyframes failed");
  if (kf_count != 3) fail("slide keyframe count mismatch");
  if (!near(kfs[0].tone.beat_hz, -10.0, 1e-9) ||
      !near(kfs[1].time_sec, 600.0, 1e-12) ||
      !near(kfs[2].time_sec, 602.5, 1e-12) ||
      !near(kfs[2].tone.amplitude, 0.0, 1e-12))
    fail("slide keyframe values mismatch");
  free(kfs);

  puts("PASS: built-in program builder API checks");
  return 0;
}
