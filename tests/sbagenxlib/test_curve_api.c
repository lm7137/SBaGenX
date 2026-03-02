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
  SbxCurveProgram *curve;
  SbxCurveEvalConfig cfg;
  SbxCurveEvalPoint pt0, pt1, pt2;
  SbxCurveInfo info;
  const char *name = 0;
  double value = 0.0;
  size_t count = 0;
  int rc;

  curve = sbx_curve_create();
  if (!curve) fail("sbx_curve_create failed");

  rc = sbx_curve_load_file(curve, "examples/basics/curve-sigmoid-like.sbgf");
  if (rc != SBX_OK) fail(sbx_curve_last_error(curve));
  rc = sbx_curve_get_info(curve, &info);
  if (rc != SBX_OK) fail("sbx_curve_get_info failed for sigmoid-like curve");
  if (!info.has_carrier_expr || info.has_solve)
    fail("sigmoid-like curve info mismatch");
  count = sbx_curve_param_count(curve);
  if (count != 2) fail("sigmoid-like curve parameter count mismatch");
  rc = sbx_curve_get_param(curve, 0, &name, &value);
  if (rc != SBX_OK || !name || value <= 0.0)
    fail("sbx_curve_get_param failed for first parameter");
  rc = sbx_curve_set_param(curve, "l", 0.2);
  if (rc != SBX_OK) fail(sbx_curve_last_error(curve));
  rc = sbx_curve_set_param(curve, "h", 0.0);
  if (rc != SBX_OK) fail(sbx_curve_last_error(curve));

  sbx_default_curve_eval_config(&cfg);
  cfg.beat_start_hz = 10.0;
  cfg.beat_target_hz = 2.5;
  cfg.carrier_start_hz = 205.0;
  cfg.carrier_end_hz = 200.0;
  cfg.carrier_span_sec = 1800.0;
  cfg.beat_span_sec = 1800.0;
  cfg.hold_min = 30.0;
  cfg.total_min = 60.0;
  cfg.wake_min = 3.0;
  rc = sbx_curve_prepare(curve, &cfg);
  if (rc != SBX_OK) fail(sbx_curve_last_error(curve));
  rc = sbx_curve_eval(curve, 0.0, &pt0);
  if (rc != SBX_OK) fail(sbx_curve_last_error(curve));
  rc = sbx_curve_eval(curve, 1800.0, &pt1);
  if (rc != SBX_OK) fail(sbx_curve_last_error(curve));
  if (!(pt0.beat_hz > 9.0 && pt0.beat_hz < 10.1))
    fail("sigmoid-like start beat outside expected range");
  if (!(pt1.beat_hz > 2.3 && pt1.beat_hz < 3.0))
    fail("sigmoid-like end beat outside expected range");
  if (!near(pt0.carrier_hz, 205.0, 1e-9) || !near(pt1.carrier_hz, 202.5, 1e-9))
    fail("sigmoid-like carrier interpolation mismatch");

  rc = sbx_curve_load_file(curve, "examples/basics/curve-piecewise-demo.sbgf");
  if (rc != SBX_OK) fail(sbx_curve_last_error(curve));
  rc = sbx_curve_prepare(curve, &cfg);
  if (rc != SBX_OK) fail(sbx_curve_last_error(curve));
  rc = sbx_curve_get_info(curve, &info);
  if (rc != SBX_OK) fail("sbx_curve_get_info failed for piecewise curve");
  if (info.beat_piece_count != 3)
    fail("piecewise beat piece count mismatch");
  rc = sbx_curve_eval(curve, 8.0 * 60.0, &pt0);
  if (rc != SBX_OK) fail(sbx_curve_last_error(curve));
  rc = sbx_curve_eval(curve, 18.0 * 60.0, &pt1);
  if (rc != SBX_OK) fail(sbx_curve_last_error(curve));
  rc = sbx_curve_eval(curve, 30.0 * 60.0, &pt2);
  if (rc != SBX_OK) fail(sbx_curve_last_error(curve));
  if (!near(pt0.beat_hz, 6.0, 1e-9))
    fail("piecewise curve p1 boundary mismatch");
  if (!near(pt1.beat_hz, 3.0, 1e-9))
    fail("piecewise curve p2 boundary mismatch");
  if (!near(pt2.beat_hz, 2.5, 1e-9))
    fail("piecewise curve final target mismatch");

  rc = sbx_curve_load_file(curve, "examples/basics/curve-expfit-solve-demo.sbgf");
  if (rc != SBX_OK) fail(sbx_curve_last_error(curve));
  rc = sbx_curve_prepare(curve, &cfg);
  if (rc != SBX_OK) fail(sbx_curve_last_error(curve));
  rc = sbx_curve_get_info(curve, &info);
  if (rc != SBX_OK) fail("sbx_curve_get_info failed for solve curve");
  if (!info.has_solve)
    fail("solve curve should report solve support");
  rc = sbx_curve_eval(curve, 0.0, &pt0);
  if (rc != SBX_OK) fail(sbx_curve_last_error(curve));
  rc = sbx_curve_eval(curve, 1800.0, &pt1);
  if (rc != SBX_OK) fail(sbx_curve_last_error(curve));
  if (!near(pt0.beat_hz, 10.0, 1e-6))
    fail("solve curve start boundary mismatch");
  if (!near(pt1.beat_hz, 2.5, 1e-6))
    fail("solve curve end boundary mismatch");

  sbx_curve_destroy(curve);
  puts("PASS: sbagenxlib curve API checks");
  return 0;
}
