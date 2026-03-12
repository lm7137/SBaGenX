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

static double
drop_expected(double drop_sec,
              double beat_start_hz,
              double beat_target_hz,
              int slide,
              int n_step,
              int step_len_sec,
              double t_sec) {
  if (!slide && n_step > 1 && step_len_sec > 0) {
    int idx = (int)(t_sec / (double)step_len_sec);
    if (idx < 0) idx = 0;
    if (idx > n_step - 1) idx = n_step - 1;
    return beat_start_hz * exp(log(beat_target_hz / beat_start_hz) *
                               idx / (double)(n_step - 1));
  }
  return beat_start_hz * exp(log(beat_target_hz / beat_start_hz) * t_sec / drop_sec);
}

static double
sigmoid_expected(double drop_sec,
                 double beat_target_hz,
                 double sig_l,
                 double sig_h,
                 double sig_a,
                 double sig_b,
                 double t_sec) {
  double d_min = drop_sec / 60.0;
  double t_min = t_sec / 60.0;
  if (t_min >= d_min) return beat_target_hz;
  return sig_a * tanh(sig_l * (t_min - d_min / 2.0 - sig_h)) + sig_b;
}

int
main(void) {
  double ts[4];
  double ys[4];
  SbxCurveProgram *curve = NULL;
  SbxCurveEvalConfig cfg;
  int rc;

  rc = sbx_sample_drop_curve(600.0, 10.0, 2.5, 1, 0, 0, 3, ts, ys);
  if (rc != SBX_OK) fail("sbx_sample_drop_curve continuous failed");
  if (!near(ts[0], 0.0, 1e-12) || !near(ts[1], 300.0, 1e-12) || !near(ts[2], 600.0, 1e-12))
    fail("continuous drop sample times mismatch");
  if (!near(ys[0], 10.0, 1e-9) || !near(ys[1], 5.0, 1e-9) || !near(ys[2], 2.5, 1e-9))
    fail("continuous drop sample values mismatch");

  rc = sbx_sample_drop_curve(600.0, 10.0, 2.5, 0, 4, 200, 4, ts, ys);
  if (rc != SBX_OK) fail("sbx_sample_drop_curve stepped failed");
  if (!near(ys[0], drop_expected(600.0, 10.0, 2.5, 0, 4, 200, 0.0), 1e-9) ||
      !near(ys[1], drop_expected(600.0, 10.0, 2.5, 0, 4, 200, 200.0), 1e-9) ||
      !near(ys[2], drop_expected(600.0, 10.0, 2.5, 0, 4, 200, 400.0), 1e-9) ||
      !near(ys[3], drop_expected(600.0, 10.0, 2.5, 0, 4, 200, 600.0), 1e-9))
    fail("stepped drop sample values mismatch");

  rc = sbx_sample_sigmoid_curve(1800.0, 10.0, 2.5,
                                0.125, 0.0, -3.9306, 6.25,
                                3, ts, ys);
  if (rc != SBX_OK) fail("sbx_sample_sigmoid_curve failed");
  if (!near(ys[0], sigmoid_expected(1800.0, 2.5, 0.125, 0.0, -3.9306, 6.25, 0.0), 1e-6) ||
      !near(ys[1], sigmoid_expected(1800.0, 2.5, 0.125, 0.0, -3.9306, 6.25, 900.0), 1e-6) ||
      !near(ys[2], sigmoid_expected(1800.0, 2.5, 0.125, 0.0, -3.9306, 6.25, 1800.0), 1e-6))
    fail("sigmoid sample values mismatch");

  if (sbx_sample_drop_curve(0.0, 10.0, 2.5, 1, 0, 0, 3, ts, ys) != SBX_EINVAL)
    fail("drop invalid args should fail");
  if (sbx_sample_sigmoid_curve(0.0, 10.0, 2.5, 0.125, 0.0, -3.9306, 6.25, 3, ts, ys) != SBX_EINVAL)
    fail("sigmoid invalid args should fail");

  curve = sbx_curve_create();
  if (!curve) fail("sbx_curve_create failed");
  rc = sbx_curve_load_text(curve,
                           "beat = lerp(b0,b1,ramp(m,0,D))\n"
                           "carrier = c0\n",
                           "<curve-plot-test>");
  if (rc != SBX_OK) fail("sbx_curve_load_text failed");
  sbx_default_curve_eval_config(&cfg);
  cfg.carrier_start_hz = 205.0;
  cfg.carrier_end_hz = 205.0;
  cfg.carrier_span_sec = 600.0;
  cfg.beat_start_hz = 10.0;
  cfg.beat_target_hz = 2.5;
  cfg.beat_span_sec = 600.0;
  cfg.total_min = 10.0;
  cfg.beat_amp0_pct = 100.0;
  cfg.mix_amp0_pct = 100.0;
  rc = sbx_curve_prepare(curve, &cfg);
  if (rc != SBX_OK) fail("sbx_curve_prepare failed");
  rc = sbx_curve_sample_program_beat(curve, 0.0, 600.0, 3, ts, ys);
  if (rc != SBX_OK) fail("sbx_curve_sample_program_beat failed");
  if (!near(ys[0], 10.0, 1e-9) || !near(ys[1], 6.25, 1e-9) || !near(ys[2], 2.5, 1e-9))
    fail("curve beat sample values mismatch");
  if (sbx_curve_sample_program_beat(curve, 0.0, 600.0, 0, ts, ys) != SBX_EINVAL)
    fail("curve beat invalid args should fail");
  sbx_curve_destroy(curve);

  puts("PASS: program plot sampling API checks");
  return 0;
}
