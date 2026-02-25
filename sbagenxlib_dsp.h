#ifndef SBAGENXLIB_DSP_H
#define SBAGENXLIB_DSP_H

#include <math.h>

static inline double
sbx_dsp_clamp(double v, double lo, double hi) {
  if (v < lo) return lo;
  if (v > hi) return hi;
  return v;
}

static inline double
sbx_dsp_wrap_cycle(double phase, double cycle) {
  if (cycle <= 0.0) return phase;
  while (phase >= cycle) phase -= cycle;
  while (phase < 0.0) phase += cycle;
  return phase;
}

static inline double
sbx_dsp_wrap_unit(double phase) {
  return sbx_dsp_wrap_cycle(phase, 1.0);
}

static inline double
sbx_dsp_smoothstep01(double x) {
  if (x <= 0.0) return 0.0;
  if (x >= 1.0) return 1.0;
  return x * x * (3.0 - 2.0 * x);
}

static inline double
sbx_dsp_smootherstep01(double x) {
  if (x <= 0.0) return 0.0;
  if (x >= 1.0) return 1.0;
  return x * x * x * (x * (x * 6.0 - 15.0) + 10.0);
}

/*
 * Edge shaping modes for isochronic custom envelope.
 * 0 hard, 1 linear, 2 smoothstep, 3 smootherstep.
 */
static inline double
sbx_dsp_iso_edge_shape(double x, int mode) {
  if (x <= 0.0) return 0.0;
  if (x >= 1.0) return 1.0;
  switch (mode) {
    case 0: return x > 0.0 ? 1.0 : 0.0;
    case 1: return x;
    case 3: return sbx_dsp_smootherstep01(x);
    case 2:
    default: return sbx_dsp_smoothstep01(x);
  }
}

/*
 * Shared custom isochronic envelope over one unit phase cycle.
 * phase/start/duty/attack/release are in cycle-relative units (0..1).
 */
static inline double
sbx_dsp_iso_mod_factor_custom(double phase,
                              double start, double duty,
                              double attack, double release,
                              int edge_mode) {
  double end = start + duty;
  double u = -1.0;

  phase = sbx_dsp_wrap_unit(phase);

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
    return sbx_dsp_iso_edge_shape(u / attack, edge_mode);
  if (u <= (1.0 - release))
    return 1.0;
  if (release > 0.0)
    return sbx_dsp_iso_edge_shape((1.0 - u) / release, edge_mode);
  return 0.0;
}

#endif /* SBAGENXLIB_DSP_H */
