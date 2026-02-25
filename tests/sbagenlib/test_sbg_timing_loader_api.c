#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sbagenlib.h"

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
  float *buf;
  size_t frames;
  int zc_lo, zc_hi;
  int rc;
  const char *sbg_text =
      "# minimal sbg timing subset\n"
      "00:00 100+0/50\n"
      "00:00:06 240+0/50\n";
  const char *tmp_path = "/tmp/sbx_sbg_timing_test.sbg";
  double t_actual, t_expect;

  sbx_default_engine_config(&cfg);
  cfg.sample_rate = 44100.0;
  ctx = sbx_context_create(&cfg);
  if (!ctx) fail("context create failed");

  rc = sbx_context_load_sbg_timing_text(ctx, sbg_text, 0);
  if (rc != SBX_OK) fail("load_sbg_timing_text failed");

  frames = (size_t)(6.0 * cfg.sample_rate);
  buf = (float *)calloc(frames * 2, sizeof(float));
  if (!buf) fail("alloc failed");
  if (sbx_context_render_f32(ctx, buf, frames) != SBX_OK)
    fail("render failed");

  zc_lo = zero_crossings_left(buf, frames / 4);
  zc_hi = zero_crossings_left(buf + (frames * 3 / 4) * 2, frames / 4);
  if (!(zc_hi > (int)(1.5 * zc_lo)))
    fail("expected higher crossing rate near end of sbg timing segment");

  t_actual = sbx_context_time_sec(ctx);
  if (fabs(t_actual - 6.0) > 5e-3)
    fail("time tracking after sbg timing render is out of range");

  rc = sbx_context_load_sbg_timing_text(ctx, "00:00\n", 0);
  if (rc != SBX_EINVAL)
    fail("invalid sbg timing line should fail");

  rc = sbx_context_load_sbg_timing_text(ctx, "0s 100+0/40\n", 0);
  if (rc != SBX_EINVAL)
    fail("non-HH:MM token should fail for sbg timing loader");

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

  free(buf);
  sbx_context_destroy(ctx);
  printf("PASS: sbagenlib sbg timing loader checks\n");
  return 0;
}
