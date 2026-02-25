#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "sbagenlib_dsp.h"

static double legacy_clamp(double v, double lo, double hi) {
  if (v < lo) return lo;
  if (v > hi) return hi;
  return v;
}

static double legacy_smoothstep01(double x) {
  if (x <= 0.0) return 0.0;
  if (x >= 1.0) return 1.0;
  return x * x * (3.0 - 2.0 * x);
}

static double legacy_smootherstep01(double x) {
  if (x <= 0.0) return 0.0;
  if (x >= 1.0) return 1.0;
  return x * x * x * (x * (x * 6.0 - 15.0) + 10.0);
}

static double legacy_iso_edge_shape(double x, int mode) {
  if (x <= 0.0) return 0.0;
  if (x >= 1.0) return 1.0;
  switch (mode) {
    case 0: return x > 0.0 ? 1.0 : 0.0;
    case 1: return x;
    case 3: return legacy_smootherstep01(x);
    case 2:
    default: return legacy_smoothstep01(x);
  }
}

static double legacy_iso_mod_factor_custom(double phase,
                                           double start, double duty,
                                           double attack, double release,
                                           int edge_mode) {
  double end = start + duty;
  double u = -1.0;

  phase -= floor(phase);
  if (phase < 0.0) phase += 1.0;

  if (duty >= 1.0)
    return 1.0;

  if (end <= 1.0) {
    if (phase >= start && phase < end)
      u = (phase - start) / duty;
  } else {
    if (phase >= start)
      u = (phase - start) / duty;
    else if (phase < (end - 1.0))
      u = (phase + (1.0 - start)) / duty;
  }

  if (u <= 0.0 || u >= 1.0)
    return 0.0;

  if (attack > 0.0 && u < attack)
    return legacy_iso_edge_shape(u / attack, edge_mode);
  if (u <= (1.0 - release))
    return 1.0;
  if (release > 0.0)
    return legacy_iso_edge_shape((1.0 - u) / release, edge_mode);
  return 0.0;
}

static int nearly_equal(double a, double b, double eps) {
  double d = fabs(a - b);
  if (d <= eps) return 1;
  if (fabs(a) > 1.0 || fabs(b) > 1.0) {
    double m = fabs(a) > fabs(b) ? fabs(a) : fabs(b);
    return d <= eps * m;
  }
  return 0;
}

static void fail(const char *name, double a, double b, double x) {
  fprintf(stderr, "FAIL %s at x=%.12g: got %.17g expected %.17g\n", name, x, a, b);
  exit(1);
}

int main(void) {
  int i, mode, p;
  const double eps = 1e-12;

  for (i = -1000; i <= 2000; i++) {
    double x = i / 1000.0;
    double got = sbx_dsp_clamp(x, -0.25, 0.75);
    double exp = legacy_clamp(x, -0.25, 0.75);
    if (!nearly_equal(got, exp, eps)) fail("clamp", got, exp, x);
  }

  for (i = -1000; i <= 2000; i++) {
    double x = i / 1000.0;
    double got = sbx_dsp_smoothstep01(x);
    double exp = legacy_smoothstep01(x);
    if (!nearly_equal(got, exp, eps)) fail("smoothstep01", got, exp, x);
  }

  for (i = -1000; i <= 2000; i++) {
    double x = i / 1000.0;
    double got = sbx_dsp_smootherstep01(x);
    double exp = legacy_smootherstep01(x);
    if (!nearly_equal(got, exp, eps)) fail("smootherstep01", got, exp, x);
  }

  for (mode = 0; mode <= 3; mode++) {
    for (i = -1000; i <= 2000; i++) {
      double x = i / 1000.0;
      double got = sbx_dsp_iso_edge_shape(x, mode);
      double exp = legacy_iso_edge_shape(x, mode);
      if (!nearly_equal(got, exp, eps)) fail("iso_edge_shape", got, exp, x);
    }
  }

  {
    const double params[][5] = {
      {0.0485, 0.4030, 0.50, 0.50, 2.0},
      {0.10,   0.30,   0.20, 0.20, 3.0},
      {0.70,   0.60,   0.15, 0.25, 2.0},
      {0.00,   1.00,   0.50, 0.50, 2.0},
      {0.20,   0.40,   0.00, 0.00, 1.0}
    };
    const int nparams = (int)(sizeof(params) / sizeof(params[0]));

    for (p = 0; p < nparams; p++) {
      double s = params[p][0];
      double d = params[p][1];
      double a = params[p][2];
      double r = params[p][3];
      int e = (int)params[p][4];

      for (i = -2000; i <= 4000; i++) {
        double phase = i / 2000.0;
        double got = sbx_dsp_iso_mod_factor_custom(phase, s, d, a, r, e);
        double exp = legacy_iso_mod_factor_custom(phase, s, d, a, r, e);
        if (!nearly_equal(got, exp, eps)) fail("iso_mod_factor_custom", got, exp, phase);
      }
    }
  }

  printf("PASS: sbagenlib DSP parity checks\n");
  return 0;
}
