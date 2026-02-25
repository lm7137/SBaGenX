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

static void
write_text_file(const char *path, const char *text) {
  FILE *fp = fopen(path, "wb");
  size_t n;
  if (!fp) fail("failed to create temp sequence file");
  n = fwrite(text, 1, strlen(text), fp);
  fclose(fp);
  if (n != strlen(text)) fail("failed to write temp sequence file");
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
  const char *seq_text =
      "# keyframed test\n"
      "0s 100+0/50\n"
      "0.1s 300+0/50  // faster by end\n";
  const char *seq_units =
      "; minutes-supported time token\n"
      "0m 120+0/30\n"
      "0.002m 240+0/30\n";
  const char *seq_step =
      "0s 100+0/40 step\n"
      "0.1s 220+0/40\n";
  const char *tmp_path = "/tmp/sbx_seq_loader_test.sbxseq";
  double t_actual, t_expect;

  sbx_default_engine_config(&cfg);
  cfg.sample_rate = 44100.0;
  ctx = sbx_context_create(&cfg);
  if (!ctx) fail("context create failed");
  if (sbx_context_set_default_waveform(ctx, SBX_WAVE_SQUARE) != SBX_OK)
    fail("set_default_waveform failed");

  rc = sbx_context_load_sequence_text(ctx, seq_text, 0);
  if (rc != SBX_OK) fail("load_sequence_text failed");
  if (sbx_context_get_keyframe(ctx, 0, &kf) != SBX_OK)
    fail("keyframe retrieval failed");
  if (kf.tone.waveform != SBX_WAVE_SQUARE)
    fail("default waveform should apply to unprefixed sequence tones");

  frames = (size_t)(0.1 * cfg.sample_rate);
  buf = (float *)calloc(frames * 2, sizeof(float));
  if (!buf) fail("alloc failed");
  if (sbx_context_render_f32(ctx, buf, frames) != SBX_OK)
    fail("render from loaded sequence text failed");

  zc_lo = zero_crossings_left(buf, frames / 3);
  zc_hi = zero_crossings_left(buf + (frames * 2 / 3) * 2, frames / 3);
  if (!(zc_hi > (int)(1.5 * zc_lo)))
    fail("expected higher crossing rate near end of keyframed segment");

  t_actual = sbx_context_time_sec(ctx);
  if (fabs(t_actual - 0.1) > 2e-3)
    fail("time tracking after sequence render is out of range");

  rc = sbx_context_load_sequence_text(ctx, seq_units, 0);
  if (rc != SBX_OK) fail("load_sequence_text with minute suffix failed");

  rc = sbx_context_load_sequence_text(ctx, seq_step, 0);
  if (rc != SBX_OK) fail("load_sequence_text with step interpolation failed");

  rc = sbx_context_load_sequence_text(ctx, "0s\n", 0);
  if (rc != SBX_EINVAL)
    fail("invalid sequence line should fail");

  rc = sbx_context_load_sequence_text(ctx, "0s 100+0/40 smooth\n", 0);
  if (rc != SBX_EINVAL)
    fail("unknown interpolation token should fail");

  write_text_file(tmp_path, seq_text);
  rc = sbx_context_load_sequence_file(ctx, tmp_path, 0);
  remove(tmp_path);
  if (rc != SBX_OK) fail("load_sequence_file failed");

  rc = sbx_context_load_sequence_text(ctx, "0s 100+0/40\n0.1s 200+0/40\n", 1);
  if (rc != SBX_OK) fail("looped sequence load failed");
  frames = (size_t)(0.35 * cfg.sample_rate);
  free(buf);
  buf = (float *)calloc(frames * 2, sizeof(float));
  if (!buf) fail("alloc failed (loop)");
  if (sbx_context_render_f32(ctx, buf, frames) != SBX_OK)
    fail("looped render failed");
  t_expect = fmod((double)frames / cfg.sample_rate, 0.1);
  t_actual = sbx_context_time_sec(ctx);
  if (fabs(t_actual - t_expect) > 3e-3)
    fail("looped time tracking out of range");

  free(buf);
  sbx_context_destroy(ctx);
  printf("PASS: sbagenxlib sequence loader API checks\n");
  return 0;
}
