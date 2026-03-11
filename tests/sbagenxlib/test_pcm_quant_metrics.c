#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "sbagenxlib.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define N 4096
#define BIN 127

static void
fail(const char *msg) {
  fprintf(stderr, "FAIL: %s\n", msg);
  exit(1);
}

static void
gen_low_level_sine(float *buf) {
  size_t i;
  const double amp = 1.2 / 32767.0;
  for (i = 0; i < N; i++) {
    double ph = (2.0 * M_PI * (double)BIN * (double)i) / (double)N;
    buf[i] = (float)(amp * sin(ph));
  }
}

static void
reconstruct_s16(const int16_t *in, double *out) {
  size_t i;
  for (i = 0; i < N; i++)
    out[i] = (double)in[i] / 32767.0;
}

static void
compute_error(const float *src, const double *recon, double *err) {
  size_t i;
  for (i = 0; i < N; i++)
    err[i] = recon[i] - (double)src[i];
}

static double
vec_rms(const double *v) {
  size_t i;
  double acc = 0.0;
  for (i = 0; i < N; i++) acc += v[i] * v[i];
  return sqrt(acc / (double)N);
}

static double
vec_mean(const double *v) {
  size_t i;
  double acc = 0.0;
  for (i = 0; i < N; i++) acc += v[i];
  return acc / (double)N;
}

static double
normalized_corr(const float *src, const double *err) {
  size_t i;
  double sxy = 0.0, sx2 = 0.0, sy2 = 0.0;
  for (i = 0; i < N; i++) {
    double x = (double)src[i];
    double y = err[i];
    sxy += x * y;
    sx2 += x * x;
    sy2 += y * y;
  }
  if (sx2 <= 0.0 || sy2 <= 0.0) return 0.0;
  return sxy / sqrt(sx2 * sy2);
}

static double
max_bin_to_avg_ratio(const double *err) {
  int k;
  double max_mag = 0.0;
  double sum_mag = 0.0;
  int bins = 0;
  for (k = 1; k < N / 2; k++) {
    int n;
    double re = 0.0, im = 0.0;
    for (n = 0; n < N; n++) {
      double ang = (2.0 * M_PI * (double)k * (double)n) / (double)N;
      re += err[n] * cos(ang);
      im -= err[n] * sin(ang);
    }
    {
      double mag = hypot(re, im);
      if (mag > max_mag) max_mag = mag;
      sum_mag += mag;
      bins++;
    }
  }
  if (bins == 0 || sum_mag <= 0.0) return 0.0;
  return max_mag / (sum_mag / (double)bins);
}

int
main(void) {
  float src[N];
  int16_t nodither_q[N], dither_q[N];
  double nodither_recon[N], dither_recon[N];
  double nodither_err[N], dither_err[N];
  SbxPcmConvertState nodither_state;
  SbxPcmConvertState dither_state;
  double nodither_corr, dither_corr;
  double nodither_ratio, dither_ratio;
  double nodither_rms, dither_rms;
  double dither_mean;

  gen_low_level_sine(src);

  sbx_seed_pcm_convert_state(&nodither_state, 1u, SBX_PCM_DITHER_NONE);
  sbx_seed_pcm_convert_state(&dither_state, 1u, SBX_PCM_DITHER_TPDF);

  if (sbx_convert_f32_to_s16_ex(src, nodither_q, N, &nodither_state) != SBX_OK)
    fail("nodither quantization failed");
  if (sbx_convert_f32_to_s16_ex(src, dither_q, N, &dither_state) != SBX_OK)
    fail("dither quantization failed");

  reconstruct_s16(nodither_q, nodither_recon);
  reconstruct_s16(dither_q, dither_recon);
  compute_error(src, nodither_recon, nodither_err);
  compute_error(src, dither_recon, dither_err);

  nodither_corr = fabs(normalized_corr(src, nodither_err));
  dither_corr = fabs(normalized_corr(src, dither_err));
  nodither_ratio = max_bin_to_avg_ratio(nodither_err);
  dither_ratio = max_bin_to_avg_ratio(dither_err);
  nodither_rms = vec_rms(nodither_err);
  dither_rms = vec_rms(dither_err);
  dither_mean = fabs(vec_mean(dither_err));

  if (!(dither_corr < nodither_corr * 0.2))
    fail("dither should substantially reduce signal-correlated quantization error");
  if (!(dither_ratio < nodither_ratio * 0.2))
    fail("dither should flatten the error spectrum");
  if (!(dither_rms > nodither_rms))
    fail("TPDF dither should raise total noise power relative to undithered quantization");
  if (!(dither_mean < 1.0 / 32767.0))
    fail("dithered error mean should remain small");

  printf("PASS: quant metrics nodither_corr=%g dither_corr=%g nodither_ratio=%g dither_ratio=%g nodither_rms=%g dither_rms=%g\n",
         nodither_corr, dither_corr, nodither_ratio, dither_ratio,
         nodither_rms, dither_rms);
  return 0;
}
