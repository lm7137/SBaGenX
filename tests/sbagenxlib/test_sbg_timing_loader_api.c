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
zero_crossings_left(const float *buf, size_t frames) {
  size_t i;
  int zc = 0;
  float prev = buf[0];
  for (i = 1; i < frames; i++) {
    float cur = buf[i * 2];
    if ((prev <= 0.0f && cur > 0.0f) || (prev >= 0.0f && cur < 0.0f))
      zc++;
    prev = cur;
  }
  return zc;
}

static double
abs_sum_window(const float *buf, size_t i0, size_t i1) {
  size_t i;
  double s = 0.0;
  if (i1 <= i0) return 0.0;
  for (i = i0; i < i1; i++) {
    s += fabs((double)buf[i * 2]);
    s += fabs((double)buf[i * 2 + 1]);
  }
  return s;
}

static void
write_text_file(const char *path, const char *text) {
  FILE *fp = fopen(path, "wb");
  size_t n;
  if (!fp) fail("failed to create temp sbg file");
  n = fwrite(text, 1, strlen(text), fp);
  fclose(fp);
  if (n != strlen(text)) fail("failed to write temp sbg file");
}

int
main(void) {
  SbxEngineConfig cfg;
  SbxContext *ctx;
  SbxProgramKeyframe kf;
  float *buf;
  size_t frames;
  int zc_lo, zc_hi;
  int rc;
  const char *sbg_text =
      "# minimal sbg timing subset\n"
      "00:00 100+0/50 step\n"
      "00:00:06 240+0/50\n";
  const char *sbg_named_text =
      "# named tone-set + timeline references\n"
      "off: -\n"
      "base: 180+0/35\n"
      "00:00 off ==\n"
      "00:00:01 base ->\n"
      "00:00:02 off\n";
  const char *sbg_relative_text =
      "base: 120+0/30\n"
      "NOW base\n"
      "+00:00:02 base\n";
  const char *tmp_path = "/tmp/sbx_sbg_timing_test.sbg";
  double t_actual, t_expect;

  sbx_default_engine_config(&cfg);
  cfg.sample_rate = 44100.0;
  ctx = sbx_context_create(&cfg);
  if (!ctx) fail("context create failed");
  if (sbx_context_set_default_waveform(ctx, SBX_WAVE_TRIANGLE) != SBX_OK)
    fail("set_default_waveform failed");

  rc = sbx_context_load_sbg_timing_text(ctx, sbg_text, 0);
  if (rc != SBX_OK) fail("load_sbg_timing_text failed");
  if (sbx_context_get_keyframe(ctx, 0, &kf) != SBX_OK)
    fail("keyframe retrieval failed");
  if (kf.tone.waveform != SBX_WAVE_TRIANGLE)
    fail("default waveform should apply to unprefixed sbg timing tones");

  frames = (size_t)(6.6 * cfg.sample_rate);
  buf = (float *)calloc(frames * 2, sizeof(float));
  if (!buf) fail("alloc failed");
  if (sbx_context_render_f32(ctx, buf, frames) != SBX_OK)
    fail("render failed");

  zc_lo = zero_crossings_left(buf, (size_t)(0.4 * cfg.sample_rate));
  zc_hi = zero_crossings_left(buf + ((size_t)(6.1 * cfg.sample_rate)) * 2,
                              (size_t)(0.4 * cfg.sample_rate));
  if (!(zc_hi > (int)(2.0 * zc_lo)))
    fail("expected higher crossing rate near end of sbg timing segment");

  t_actual = sbx_context_time_sec(ctx);
  if (fabs(t_actual - 6.6) > 5e-3)
    fail("time tracking after sbg timing render is out of range");

  rc = sbx_context_load_sbg_timing_text(ctx, "00:00\n", 0);
  if (rc != SBX_EINVAL)
    fail("invalid sbg timing line should fail");

  rc = sbx_context_load_sbg_timing_text(ctx, "0s 100+0/40\n", 0);
  if (rc != SBX_EINVAL)
    fail("non-HH:MM token should fail for sbg timing loader");
  rc = sbx_context_load_sbg_timing_text(ctx, "+00:00:02 100+0/40\n", 0);
  if (rc != SBX_EINVAL)
    fail("relative token without previous absolute/NOW should fail");

  rc = sbx_context_load_sbg_timing_text(ctx, "00:00 100+0/40 smooth\n", 0);
  if (rc != SBX_EINVAL)
    fail("unknown interpolation token should fail");

  write_text_file(tmp_path, sbg_text);
  rc = sbx_context_load_sbg_timing_file(ctx, tmp_path, 0);
  remove(tmp_path);
  if (rc != SBX_OK) fail("load_sbg_timing_file failed");

  rc = sbx_context_load_sbg_timing_text(ctx, "00:00 100+0/40\n00:00:05 220+0/40\n", 1);
  if (rc != SBX_OK) fail("looped sbg timing load failed");
  frames = (size_t)(12.4 * cfg.sample_rate);
  free(buf);
  buf = (float *)calloc(frames * 2, sizeof(float));
  if (!buf) fail("alloc failed (loop)");
  if (sbx_context_render_f32(ctx, buf, frames) != SBX_OK)
    fail("looped sbg timing render failed");
  t_expect = fmod((double)frames / cfg.sample_rate, 5.0);
  t_actual = sbx_context_time_sec(ctx);
  if (fabs(t_actual - t_expect) > 4e-3)
    fail("looped sbg timing time tracking out of range");

  rc = sbx_context_load_sbg_timing_text(ctx, sbg_named_text, 0);
  if (rc != SBX_OK) fail("named sbg timing load failed");
  free(buf);
  frames = (size_t)(2.3 * cfg.sample_rate);
  buf = (float *)calloc(frames * 2, sizeof(float));
  if (!buf) fail("alloc failed (named)");
  if (sbx_context_render_f32(ctx, buf, frames) != SBX_OK)
    fail("named sbg timing render failed");
  if (!(abs_sum_window(buf, (size_t)(0.1 * cfg.sample_rate), (size_t)(0.4 * cfg.sample_rate)) < 1e-4))
    fail("named sbg timing should be silent in initial off segment");
  if (!(abs_sum_window(buf, (size_t)(1.2 * cfg.sample_rate), (size_t)(1.6 * cfg.sample_rate)) > 1e-3))
    fail("named sbg timing should have energy in base segment");
  if (!(abs_sum_window(buf, (size_t)(2.05 * cfg.sample_rate), (size_t)(2.25 * cfg.sample_rate)) < 1e-4))
    fail("named sbg timing should be silent after final off switch");

  rc = sbx_context_load_sbg_timing_text(ctx, sbg_relative_text, 0);
  if (rc != SBX_OK) fail("relative/NOW sbg timing load failed");
  if (sbx_context_get_keyframe(ctx, 0, &kf) != SBX_OK || fabs(kf.time_sec - 0.0) > 1e-9)
    fail("NOW-based keyframe time mismatch");
  if (sbx_context_get_keyframe(ctx, 1, &kf) != SBX_OK || fabs(kf.time_sec - 2.0) > 1e-9)
    fail("relative +HH:MM:SS keyframe time mismatch");

  free(buf);
  sbx_context_destroy(ctx);
  printf("PASS: sbagenxlib sbg timing loader checks\n");
  return 0;
}
