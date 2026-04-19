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

static double
stereo_diff_window(const float *buf, size_t i0, size_t i1) {
  size_t i;
  double s = 0.0;
  if (i1 <= i0) return 0.0;
  for (i = i0; i < i1; i++)
    s += fabs((double)buf[i * 2] - (double)buf[i * 2 + 1]);
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
  const char *sbg_now_anchor_wrap_text =
      "base: 120+0/30\n"
      "NOW+00:00:10 base\n"
      "+00:00:02 base\n";
  const char *sbg_day_wrap_text =
      "off: -\n"
      "base: 180+0/25\n"
      "23:59:59 off ->\n"
      "00:00:00 base ->\n"
      "00:00:02 off\n";
  const char *sbg_equal_time_text =
      "a: 180+0/20\n"
      "b: 220+0/20\n"
      "00:00 a ->\n"
      "00:00 b ->\n"
      "00:00:01 a\n";
  const char *sbg_placeholder_voice_text =
      "stack: 500+12/25 0+0/0 0+0/0\n"
      "NOW stack\n";
  const char *sbg_negative_bell_text =
      "hit: bell-4000/30\n"
      "NOW hit\n";
  const char *sbg_timed_bell_text =
      "off: -\n"
      "hit: bell300/100\n"
      "00:00:00 off\n"
      "00:00:03 hit\n"
      "00:00:04 off\n";
  const char *sbg_multivoice_named_text =
      "solo: 180+0/20\n"
      "duo: 180+0/20 260+0/20\n"
      "00:00 solo ==\n"
      "00:00:01 duo\n";
  const char *sbg_custom_wave_text =
      "wave00: 0 1 0 0.25\n"
      "base: wave00:180+2/20\n"
      "NOW base\n";
  const char *sbg_custom_iso_text =
      "custom00: 0 0.25 1 0.25\n"
      "base: custom00:180@4/20\n"
      "NOW base\n";
  const char *sbg_custom_integer_envelope_text =
      "custom00: e=2 0 60 50 70 40 80 20 80 0 100 20 70 50 55 45 60 30 60 35 55 45 55 20 0\n"
      "custom01: e=2 0 50 60 40 45 40 70 40 50 40 55 40 60 30 100 0 85 80 0\n"
      "t1: custom00:196+6/35\n"
      "t2: custom01:180+4/30\n"
      "alloff: -\n"
      "NOW t1\n"
      "+00:01:00 t2 ->\n"
      "+00:02:00 alloff\n";
  const char *sbg_spin_wave_text =
      "spin00: e=0 0 0 0 100 100 -100 -100\n"
      "base: spin00:spin:300+1/35\n"
      "NOW base\n";
  const char *sbg_noise_wave_text =
      "noise00: 12 12 11 11 10 10 9 8 7 6 5 4 3 2 1 0 -1 -2 -3 -4 -5 -6 -7 -8 -9 -10 -11 -12 -12 -12 -12 -12\n"
      "base: noise00/20\n"
      "spin00: e=0 0 0 0 100 100 -100 -100\n"
      "wash: spin00:noise00:spin:300+1/35\n"
      "NOW base\n"
      "+00:00:01 wash\n";
  const char *sbg_noise_pulse_text =
      "custom00: e=0 0 0 1 1 0 0\n"
      "noise00: 12 12 11 11 10 10 9 8 7 6 5 4 3 2 1 0 -1 -2 -3 -4 -5 -6 -7 -8 -9 -10 -11 -12 -12 -12 -12 -12\n"
      "base: custom00:noise00:noisepulse:2/20\n"
      "NOW base\n";
  const char *sbg_noise_beat_text =
      "custom00: e=0 0 0 1 1 0 0\n"
      "noise00: 12 12 11 11 10 10 9 8 7 6 5 4 3 2 1 0 -1 -2 -3 -4 -5 -6 -7 -8 -9 -10 -11 -12 -12 -12 -12 -12\n"
      "base: custom00:triangle:noise00:noisebeat:2/20\n"
      "NOW base\n";
  const char *sbg_mix_tokens_text =
      "00:00 mix/70 mixpulse:2/40 180+0/20 ==\n"
      "00:00:02 -\n";
  const char *sbg_named_mix_text =
      "off: -\n"
      "wash: mix/60 mixbeat:3/25 180+0/15\n"
      "00:00 off ==\n"
      "00:00:01 wash ==\n"
      "00:00:02 off\n";
  const char *sbg_custom_mix_text =
      "custom00: e=2 0 0.2 1 0.2 0\n"
      "spin00: e=0 0 0 100 100 -100 -100\n"
      "pulse: mix/70 custom00:mixpulse:2/40 180+0/20\n"
      "wash: mix/60 custom00:triangle:spin00:mixspin:350+3/25 180+0/15\n"
      "beat: mix/55 custom00:mixbeat:3/25 180+0/15\n"
      "NOW pulse ==\n"
      "+00:00:02 wash ==\n"
      "+00:00:04 beat\n";
  const char *sbg_block_text =
      "off: -\n"
      "base: 180+0/35\n"
      "burst: {\n"
      "  +00:00 off ==\n"
      "  +00:01 base ->\n"
      "  +00:02 off\n"
      "}\n"
      "NOW burst\n"
      "+00:03:00 burst\n";
  const char *sbg_nested_block_text =
      "off: -\n"
      "pulse: {\n"
      "  +00:00 off ==\n"
      "  +00:01 180+0/35 ->\n"
      "}\n"
      "double: {\n"
      "  +00:00 pulse\n"
      "  +00:02 pulse\n"
      "}\n"
      "NOW double\n";
  const char *sbg_multivoice_block_text =
      "off: -\n"
      "duo: 180+0/20 260+0/20\n"
      "burst: {\n"
      "  +00:00 off ==\n"
      "  +00:00:01 duo\n"
      "}\n"
      "NOW burst\n";
  const char *sbg_many_direct_tokens_text =
      "00:00 120+0/12 140+0/12 160+0/12 180+0/12 200+0/12 220+0/12 240+0/12\n";
  const char *sbg_many_block_tokens_text =
      "burst: {\n"
      "  +00:00 120+0/12 140+0/12 160+0/12 180+0/12 200+0/12 220+0/12 240+0/12\n"
      "}\n"
      "NOW burst\n";
  const char *tmp_path = "/tmp/sbx_sbg_timing_test.sbg";
  double t_actual, t_expect;
  double min_mix = 0.0, max_mix = 0.0;
  size_t i;

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

  rc = sbx_context_load_sbg_timing_text(ctx, sbg_now_anchor_wrap_text, 0);
  if (rc != SBX_OK) fail("NOW-anchor wrap sbg timing load failed");
  if (sbx_context_get_keyframe(ctx, 0, &kf) != SBX_OK || fabs(kf.time_sec - 10.0) > 1e-9)
    fail("NOW-anchor wrap keyframe #0 time mismatch");
  if (sbx_context_get_keyframe(ctx, 1, &kf) != SBX_OK || fabs(kf.time_sec - 86402.0) > 1e-9)
    fail("NOW-anchor wrap keyframe #1 time mismatch");

  rc = sbx_context_load_sbg_timing_text(ctx, sbg_day_wrap_text, 0);
  if (rc != SBX_OK) fail("day-wrap sbg timing load failed");
  if (sbx_context_get_keyframe(ctx, 0, &kf) != SBX_OK || fabs(kf.time_sec - 86399.0) > 1e-9)
    fail("day-wrap keyframe #0 time mismatch");
  if (sbx_context_get_keyframe(ctx, 1, &kf) != SBX_OK || fabs(kf.time_sec - 86400.0) > 1e-9)
    fail("day-wrap keyframe #1 time mismatch");
  if (sbx_context_get_keyframe(ctx, 2, &kf) != SBX_OK || fabs(kf.time_sec - 86402.0) > 1e-9)
    fail("day-wrap keyframe #2 time mismatch");

  rc = sbx_context_load_sbg_timing_text(ctx, sbg_equal_time_text, 0);
  if (rc != SBX_OK) fail("equal-time sbg timing load failed");
  if (sbx_context_keyframe_count(ctx) != 3)
    fail("equal-time sbg timing should preserve all keyframes");
  if (sbx_context_get_keyframe(ctx, 0, &kf) != SBX_OK || fabs(kf.time_sec - 0.0) > 1e-9)
    fail("equal-time keyframe #0 time mismatch");
  if (sbx_context_get_keyframe(ctx, 1, &kf) != SBX_OK || fabs(kf.time_sec - 0.0) > 1e-9)
    fail("equal-time keyframe #1 time mismatch");
  if (sbx_context_get_keyframe(ctx, 2, &kf) != SBX_OK || fabs(kf.time_sec - 1.0) > 1e-9)
    fail("equal-time keyframe #2 time mismatch");

  rc = sbx_context_load_sbg_timing_text(ctx, sbg_placeholder_voice_text, 0);
  if (rc != SBX_OK) fail("placeholder-voice sbg timing load failed");
  if (sbx_context_voice_count(ctx) != 3)
    fail("placeholder-voice timing should preserve voice lane count");
  if (sbx_context_get_keyframe_voice(ctx, 0, 1, &kf) != SBX_OK || kf.tone.mode != SBX_TONE_NONE)
    fail("placeholder-voice secondary lane #1 should normalize to silence");
  if (sbx_context_get_keyframe_voice(ctx, 0, 2, &kf) != SBX_OK || kf.tone.mode != SBX_TONE_NONE)
    fail("placeholder-voice secondary lane #2 should normalize to silence");

  rc = sbx_context_load_sbg_timing_text(ctx, sbg_negative_bell_text, 0);
  if (rc != SBX_OK) fail("negative-bell sbg timing load failed");
  if (sbx_context_get_keyframe(ctx, 0, &kf) != SBX_OK)
    fail("negative-bell keyframe retrieval failed");
  if (kf.tone.mode != SBX_TONE_BELL || fabs(kf.tone.carrier_hz - 4000.0) > 1e-6)
    fail("negative bell carrier should normalize to positive magnitude");
  free(buf);
  frames = (size_t)(0.6 * cfg.sample_rate);
  buf = (float *)calloc(frames * 2, sizeof(float));
  if (!buf) fail("alloc failed (negative bell)");
  if (sbx_context_render_f32(ctx, buf, frames) != SBX_OK)
    fail("negative-bell sbg timing render failed");
  if (!(abs_sum_window(buf, (size_t)(0.02 * cfg.sample_rate), (size_t)(0.25 * cfg.sample_rate)) > 1e-3))
    fail("negative-bell sbg timing should produce audible transient energy");

  rc = sbx_context_load_sbg_timing_text(ctx, sbg_timed_bell_text, 0);
  if (rc != SBX_OK) fail("timed-bell sbg timing load failed");
  if (sbx_context_sample_tones(ctx, 2.9, 2.9, 1, NULL, &kf.tone) != SBX_OK)
    fail("timed-bell pre-trigger sample failed");
  if (kf.tone.mode != SBX_TONE_NONE)
    fail("timed bell should stay off before scheduled trigger");
  if (sbx_context_sample_tones(ctx, 3.0, 3.0, 1, NULL, &kf.tone) != SBX_OK)
    fail("timed-bell trigger sample failed");
  if (kf.tone.mode != SBX_TONE_BELL)
    fail("timed bell should activate at scheduled trigger");
  if (sbx_context_sample_tones(ctx, 4.0, 4.0, 1, NULL, &kf.tone) != SBX_OK)
    fail("timed-bell stop sample failed");
  if (kf.tone.mode != SBX_TONE_NONE)
    fail("timed bell should stop at scheduled off keyframe");

  rc = sbx_context_load_sbg_timing_text(ctx, sbg_multivoice_named_text, 0);
  if (rc != SBX_OK) fail("multivoice named sbg timing load failed");
  if (sbx_context_keyframe_count(ctx) != 2)
    fail("multivoice named timing should produce 2 keyframes");
  if (sbx_context_get_keyframe(ctx, 1, &kf) != SBX_OK)
    fail("multivoice named second keyframe retrieval failed");
  if (fabs(kf.tone.carrier_hz - 180.0) > 1e-6)
    fail("multivoice named keyframe should expose primary voice in public keyframe API");
  free(buf);
  frames = (size_t)(1.6 * cfg.sample_rate);
  buf = (float *)calloc(frames * 2, sizeof(float));
  if (!buf) fail("alloc failed (multivoice named)");
  if (sbx_context_render_f32(ctx, buf, frames) != SBX_OK)
    fail("multivoice named sbg timing render failed");
  if (!(abs_sum_window(buf, (size_t)(1.1 * cfg.sample_rate), (size_t)(1.45 * cfg.sample_rate)) >
        abs_sum_window(buf, (size_t)(0.15 * cfg.sample_rate), (size_t)(0.5 * cfg.sample_rate)) * 1.15))
    fail("multivoice named sbg timing should render higher energy after second voice enters");

  rc = sbx_context_load_sbg_timing_text(ctx, sbg_custom_wave_text, 0);
  if (rc != SBX_OK) fail("custom wave sbg timing load failed");
  if (sbx_context_get_keyframe(ctx, 0, &kf) != SBX_OK)
    fail("custom wave keyframe retrieval failed");
  if (kf.tone.envelope_waveform != SBX_ENV_WAVE_LEGACY_BASE)
    fail("custom wave keyframe should preserve wave00 prefix");
  {
    char spec_buf[128];
    if (sbx_format_tone_spec(&kf.tone, spec_buf, sizeof(spec_buf)) != SBX_OK)
      fail("custom wave keyframe format failed");
    if (strncmp(spec_buf, "wave00:", 7) != 0)
      fail("custom wave formatted tone should round-trip wave00 prefix");
  }
  free(buf);
  frames = (size_t)(0.6 * cfg.sample_rate);
  buf = (float *)calloc(frames * 2, sizeof(float));
  if (!buf) fail("alloc failed (custom wave)");
  if (sbx_context_render_f32(ctx, buf, frames) != SBX_OK)
    fail("custom wave render failed");
  if (!(abs_sum_window(buf, (size_t)(0.1 * cfg.sample_rate), (size_t)(0.5 * cfg.sample_rate)) > 1e-3))
    fail("custom wave render should produce non-zero energy");

  rc = sbx_context_load_sbg_timing_text(ctx, sbg_custom_iso_text, 0);
  if (rc != SBX_OK) fail("customNN isochronic timing load failed");
  {
    int edge_mode = -1;
    if (sbx_context_get_envelope_edge_mode(ctx, SBX_ENV_WAVE_CUSTOM_BASE, &edge_mode) != SBX_OK)
      fail("customNN edge-mode query failed");
    if (edge_mode != 1)
      fail("customNN default edge mode should be linear (e=1)");
  }
  if (sbx_context_get_keyframe(ctx, 0, &kf) != SBX_OK)
    fail("customNN isochronic keyframe retrieval failed");
  if (kf.tone.envelope_waveform != SBX_ENV_WAVE_CUSTOM_BASE)
    fail("customNN isochronic keyframe should preserve custom00 prefix");
  {
    char spec_buf[128];
    if (sbx_format_tone_spec(&kf.tone, spec_buf, sizeof(spec_buf)) != SBX_OK)
      fail("customNN isochronic format failed");
    if (strncmp(spec_buf, "custom00:", 9) != 0)
      fail("customNN isochronic tone should round-trip custom00 prefix");
  }
  free(buf);
  frames = (size_t)(0.6 * cfg.sample_rate);
  buf = (float *)calloc(frames * 2, sizeof(float));
  if (!buf) fail("alloc failed (customNN isochronic)");
  if (sbx_context_render_f32(ctx, buf, frames) != SBX_OK)
    fail("customNN isochronic render failed");
  if (!(abs_sum_window(buf, (size_t)(0.15 * cfg.sample_rate), (size_t)(0.35 * cfg.sample_rate)) >
        abs_sum_window(buf, 0, (size_t)(0.05 * cfg.sample_rate)) * 4.0))
    fail("customNN isochronic render should honor zero-based custom envelope");

  rc = sbx_context_load_sbg_timing_text(ctx, sbg_custom_integer_envelope_text, 0);
  if (rc != SBX_OK) fail("integer customNN named timing load failed");
  if (sbx_context_keyframe_count(ctx) != 3)
    fail("integer customNN named timing should produce 3 keyframes");
  if (sbx_context_get_keyframe(ctx, 0, &kf) != SBX_OK)
    fail("integer customNN first keyframe retrieval failed");
  if (kf.tone.envelope_waveform != SBX_ENV_WAVE_CUSTOM_BASE)
    fail("integer customNN first keyframe should preserve custom00 prefix");
  if (fabs(kf.tone.carrier_hz - 196.0) > 1e-6 || fabs(kf.tone.beat_hz - 6.0) > 1e-6)
    fail("integer customNN first keyframe tone mismatch");
  if (sbx_context_get_keyframe(ctx, 1, &kf) != SBX_OK)
    fail("integer customNN second keyframe retrieval failed");
  if (kf.tone.envelope_waveform != SBX_ENV_WAVE_CUSTOM_BASE + 1)
    fail("integer customNN second keyframe should preserve custom01 prefix");
  if (fabs(kf.tone.carrier_hz - 180.0) > 1e-6 || fabs(kf.tone.beat_hz - 4.0) > 1e-6)
    fail("integer customNN second keyframe tone mismatch");
  if (sbx_context_get_keyframe(ctx, 2, &kf) != SBX_OK)
    fail("integer customNN alloff keyframe retrieval failed");
  if (kf.tone.mode != SBX_TONE_NONE)
    fail("integer customNN alloff keyframe should be silent");

  rc = sbx_context_load_sbg_timing_text(ctx, sbg_spin_wave_text, 0);
  if (rc != SBX_OK) fail("spinNN timing load failed");
  if (sbx_context_get_keyframe(ctx, 0, &kf) != SBX_OK)
    fail("spinNN keyframe retrieval failed");
  if (kf.tone.waveform != SBX_WAVE_SPIN_BASE)
    fail("spinNN keyframe should preserve spin00 prefix");
  {
    char spec_buf[128];
    if (sbx_format_tone_spec(&kf.tone, spec_buf, sizeof(spec_buf)) != SBX_OK)
      fail("spinNN tone format failed");
    if (strncmp(spec_buf, "spin00:spin:", 12) != 0)
      fail("spinNN formatted tone should round-trip spin00 prefix");
  }
  free(buf);
  frames = (size_t)(1.1 * cfg.sample_rate);
  buf = (float *)calloc(frames * 2, sizeof(float));
  if (!buf) fail("alloc failed (spinNN)");
  if (sbx_context_render_f32(ctx, buf, frames) != SBX_OK)
    fail("spinNN render failed");
  if (!(stereo_diff_window(buf, (size_t)(0.28 * cfg.sample_rate), (size_t)(0.45 * cfg.sample_rate)) >
        stereo_diff_window(buf, 0, (size_t)(0.18 * cfg.sample_rate)) * 1.5))
    fail("spinNN render should modulate stereo motion strength over the cycle");

  rc = sbx_context_load_sbg_timing_text(ctx, sbg_noise_wave_text, 0);
  if (rc != SBX_OK) fail("noiseNN timing load failed");
  if (sbx_context_get_keyframe(ctx, 0, &kf) != SBX_OK)
    fail("noiseNN keyframe retrieval failed");
  if (kf.tone.noise_waveform != SBX_NOISE_WAVE_BASE)
    fail("noiseNN keyframe should preserve noise00 prefix");
  {
    char spec_buf[128];
    if (sbx_format_tone_spec(&kf.tone, spec_buf, sizeof(spec_buf)) != SBX_OK)
      fail("noiseNN tone format failed");
    if (strcmp(spec_buf, "noise00/20") != 0)
      fail("noiseNN formatted tone should round-trip noise00 prefix");
  }
  if (sbx_context_get_keyframe(ctx, 1, &kf) != SBX_OK)
    fail("noiseNN spin keyframe retrieval failed");
  if (kf.tone.noise_waveform != SBX_NOISE_WAVE_BASE || kf.tone.waveform != SBX_WAVE_SPIN_BASE)
    fail("noiseNN spin keyframe should preserve both prefixes");
  {
    char spec_buf[128];
    if (sbx_format_tone_spec(&kf.tone, spec_buf, sizeof(spec_buf)) != SBX_OK)
      fail("noiseNN spin tone format failed");
    if (strcmp(spec_buf, "spin00:noise00:spin:300+1/35") != 0)
      fail("noiseNN spin formatted tone should round-trip both prefixes");
  }
  free(buf);
  frames = (size_t)(1.1 * cfg.sample_rate);
  buf = (float *)calloc(frames * 2, sizeof(float));
  if (!buf) fail("alloc failed (noiseNN)");
  if (sbx_context_render_f32(ctx, buf, frames) != SBX_OK)
    fail("noiseNN render failed");
  if (!(abs_sum_window(buf, 0, (size_t)(0.5 * cfg.sample_rate)) > 1e-3))
    fail("noiseNN render should produce non-zero energy");

  rc = sbx_context_load_sbg_timing_text(ctx, sbg_noise_pulse_text, 0);
  if (rc != SBX_OK) fail("noisepulse timing load failed");
  if (sbx_context_get_keyframe(ctx, 0, &kf) != SBX_OK)
    fail("noisepulse keyframe retrieval failed");
  if (kf.tone.mode != SBX_TONE_NOISE_PULSE ||
      kf.tone.envelope_waveform != SBX_ENV_WAVE_CUSTOM_BASE ||
      kf.tone.noise_waveform != SBX_NOISE_WAVE_BASE)
    fail("noisepulse keyframe should preserve customNN and noiseNN prefixes");
  {
    char spec_buf[128];
    if (sbx_format_tone_spec(&kf.tone, spec_buf, sizeof(spec_buf)) != SBX_OK)
      fail("noisepulse tone format failed");
    if (strcmp(spec_buf, "custom00:noise00:noisepulse:2/20") != 0)
      fail("noisepulse formatted tone should round-trip both prefixes");
  }
  free(buf);
  frames = (size_t)(0.6 * cfg.sample_rate);
  buf = (float *)calloc(frames * 2, sizeof(float));
  if (!buf) fail("alloc failed (noisepulse)");
  if (sbx_context_render_f32(ctx, buf, frames) != SBX_OK)
    fail("noisepulse render failed");
  if (!(abs_sum_window(buf, (size_t)(0.16 * cfg.sample_rate), (size_t)(0.24 * cfg.sample_rate)) >
        abs_sum_window(buf, 0, (size_t)(0.04 * cfg.sample_rate)) * 2.5))
    fail("noisepulse render should show stronger mid-cycle gated noise energy");

  rc = sbx_context_load_sbg_timing_text(ctx, sbg_noise_beat_text, 0);
  if (rc != SBX_OK) fail("noisebeat timing load failed");
  if (sbx_context_get_keyframe(ctx, 0, &kf) != SBX_OK)
    fail("noisebeat keyframe retrieval failed");
  if (kf.tone.mode != SBX_TONE_NOISE_BEAT ||
      kf.tone.envelope_waveform != SBX_ENV_WAVE_CUSTOM_BASE ||
      kf.tone.noise_waveform != SBX_NOISE_WAVE_BASE ||
      kf.tone.waveform != SBX_WAVE_TRIANGLE)
    fail("noisebeat keyframe should preserve customNN, noiseNN, and waveform prefixes");
  {
    char spec_buf[128];
    if (sbx_format_tone_spec(&kf.tone, spec_buf, sizeof(spec_buf)) != SBX_OK)
      fail("noisebeat tone format failed");
    if (strcmp(spec_buf, "custom00:noise00:triangle:noisebeat:2/20") != 0)
      fail("noisebeat formatted tone should round-trip all prefixes");
  }
  free(buf);
  frames = (size_t)(0.6 * cfg.sample_rate);
  buf = (float *)calloc(frames * 2, sizeof(float));
  if (!buf) fail("alloc failed (noisebeat)");
  if (sbx_context_render_f32(ctx, buf, frames) != SBX_OK)
    fail("noisebeat render failed");
  if (!(stereo_diff_window(buf, 0, (size_t)(0.08 * cfg.sample_rate)) < 1e-6 &&
        stereo_diff_window(buf, (size_t)(0.32 * cfg.sample_rate), (size_t)(0.40 * cfg.sample_rate)) > 10.0))
    fail("noisebeat render should stay mono early and diverge later when customNN opens");

  rc = sbx_context_load_sbg_timing_text(ctx, sbg_mix_tokens_text, 0);
  if (rc != SBX_OK) fail("sbg timing load with direct mix tokens failed");
  if (sbx_context_get_keyframe(ctx, 0, &kf) != SBX_OK)
    fail("mix-token keyframe retrieval failed");
  if (fabs(kf.tone.carrier_hz - 180.0) > 1e-6)
    fail("direct mix-token timing should preserve primary tone");
  if (fabs(sbx_context_mix_amp_at(ctx, 0.5) - 70.0) > 1e-6)
    fail("direct mix-token timing should drive mix/<amp> keyframes");
  min_mix = 1e9;
  max_mix = -1e9;
  for (i = 0; i < (size_t)(0.5 * cfg.sample_rate); i++) {
    double add_l = 0.0, add_r = 0.0;
    double t = 0.75 + (double)i / cfg.sample_rate;
    if (sbx_context_mix_stream_sample(ctx, t, 1600, 1600, 1.0, &add_l, &add_r) != SBX_OK)
      fail("mix stream sample failed for direct mix-token timing");
    if (add_l < min_mix) min_mix = add_l;
    if (add_l > max_mix) max_mix = add_l;
  }
  if (!((max_mix - min_mix) > 5.0))
    fail("direct mix-token timing should modulate mix stream over time");

  rc = sbx_context_load_sbg_timing_text(ctx, sbg_named_mix_text, 0);
  if (rc != SBX_OK) fail("named sbg timing load with mix tokens failed");
  if (fabs(sbx_context_mix_amp_at(ctx, 0.5) - 0.0) > 1e-6)
    fail("named mix timing should be silent before entry");
  if (!(sbx_context_mix_amp_at(ctx, 1.2) > 55.0))
    fail("named mix timing should activate mix amplitude in named set");

  rc = sbx_context_load_sbg_timing_text(ctx, sbg_custom_mix_text, 0);
  if (rc != SBX_OK) fail("custom mixfx sbg timing load failed");
  {
    double min_mix = 1e9, max_mix = -1e9;
    for (i = 0; i < (size_t)(0.5 * cfg.sample_rate); i++) {
      double add_l = 0.0, add_r = 0.0;
      double t = 0.10 + (double)i / cfg.sample_rate;
      if (sbx_context_mix_stream_sample(ctx, t, 1600, 1600, 1.0, &add_l, &add_r) != SBX_OK)
        fail("mix stream sample failed for custom mixpulse timing");
      if (add_l < min_mix) min_mix = add_l;
      if (add_l > max_mix) max_mix = add_l;
    }
    if (!((max_mix - min_mix) > 5.0))
      fail("custom mixpulse timing should modulate mix stream over time");
  }
  {
    double min_l = 1e9, max_l = -1e9;
    double min_r = 1e9, max_r = -1e9;
    for (i = 0; i < (size_t)(0.5 * cfg.sample_rate); i++) {
      double add_l = 0.0, add_r = 0.0;
      double t = 2.10 + (double)i / cfg.sample_rate;
      if (sbx_context_mix_stream_sample(ctx, t, 1600, 0, 1.0, &add_l, &add_r) != SBX_OK)
        fail("mix stream sample failed for custom mixspin timing");
      if (add_l < min_l) min_l = add_l;
      if (add_l > max_l) max_l = add_l;
      if (add_r < min_r) min_r = add_r;
      if (add_r > max_r) max_r = add_r;
    }
    if (!((max_l - min_l) > 2.0 || (max_r - min_r) > 2.0))
      fail("custom mixspin timing should modulate stereo placement over time");
  }
  {
    double min_l = 1e9, max_l = -1e9;
    for (i = 0; i < (size_t)(0.5 * cfg.sample_rate); i++) {
      double add_l = 0.0, add_r = 0.0;
      double t = 4.10 + (double)i / cfg.sample_rate;
      if (sbx_context_mix_stream_sample(ctx, t, 1600, 1600, 1.0, &add_l, &add_r) != SBX_OK)
        fail("mix stream sample failed for custom mixbeat timing");
      if (add_l < min_l) min_l = add_l;
      if (add_l > max_l) max_l = add_l;
    }
    if (!((max_l - min_l) > 2.0))
      fail("custom mixbeat timing should modulate beat effect intensity over time");
  }
  {
    SbxTimedMixFxKeyframeInfo fx_info;
    if (sbx_context_timed_mix_effect_keyframe_count(ctx) != 3)
      fail("custom mixfx timing should expose 3 timed mix-effect keyframes");
    if (sbx_context_get_timed_mix_effect_keyframe_info(ctx, 0, &fx_info) != SBX_OK)
      fail("custom mixfx first timed keyframe info failed");
    if (fabs(fx_info.time_sec - 0.0) > 1e-9)
      fail("custom mixfx first timed keyframe time mismatch");
  }

  rc = sbx_context_load_sbg_timing_text(ctx, "fx: mixpulse:2/40\n00:00 fx\n", 0);
  if (rc != SBX_EINVAL)
    fail("mix effect without mix/<amp> should fail in native sbg timing loader");

  rc = sbx_context_load_sbg_timing_text(ctx, sbg_block_text, 0);
  if (rc != SBX_OK) fail("block-definition sbg timing load failed");
  if (sbx_context_keyframe_count(ctx) != 6)
    fail("block-definition expansion should produce 6 keyframes");
  if (sbx_context_get_keyframe(ctx, 0, &kf) != SBX_OK || fabs(kf.time_sec - 0.0) > 1e-9)
    fail("block keyframe #0 time mismatch");
  if (sbx_context_get_keyframe(ctx, 1, &kf) != SBX_OK || fabs(kf.time_sec - 60.0) > 1e-9)
    fail("block keyframe #1 time mismatch");
  if (sbx_context_get_keyframe(ctx, 2, &kf) != SBX_OK || fabs(kf.time_sec - 120.0) > 1e-9)
    fail("block keyframe #2 time mismatch");
  if (sbx_context_get_keyframe(ctx, 3, &kf) != SBX_OK || fabs(kf.time_sec - 180.0) > 1e-9)
    fail("block keyframe #3 time mismatch");
  if (sbx_context_get_keyframe(ctx, 4, &kf) != SBX_OK || fabs(kf.time_sec - 240.0) > 1e-9)
    fail("block keyframe #4 time mismatch");
  if (sbx_context_get_keyframe(ctx, 5, &kf) != SBX_OK || fabs(kf.time_sec - 300.0) > 1e-9)
    fail("block keyframe #5 time mismatch");

  rc = sbx_context_load_sbg_timing_text(ctx, "b:{\n+00:00 100+0/40\n", 0);
  if (rc != SBX_EINVAL)
    fail("unterminated block definition should fail");
  rc = sbx_context_load_sbg_timing_text(ctx, "b: {\n00:00 100+0/40\n}\nNOW b\n", 0);
  if (rc != SBX_EINVAL)
    fail("block line using absolute time token should fail");
  rc = sbx_context_load_sbg_timing_text(ctx, sbg_nested_block_text, 0);
  if (rc != SBX_OK) fail("nested block-definition sbg timing load failed");
  if (sbx_context_keyframe_count(ctx) != 4)
    fail("nested block-definition expansion should produce 4 keyframes");
  if (sbx_context_get_keyframe(ctx, 0, &kf) != SBX_OK || fabs(kf.time_sec - 0.0) > 1e-9)
    fail("nested block keyframe #0 time mismatch");
  if (sbx_context_get_keyframe(ctx, 1, &kf) != SBX_OK || fabs(kf.time_sec - 60.0) > 1e-9)
    fail("nested block keyframe #1 time mismatch");
  if (sbx_context_get_keyframe(ctx, 2, &kf) != SBX_OK || fabs(kf.time_sec - 120.0) > 1e-9)
    fail("nested block keyframe #2 time mismatch");
  if (sbx_context_get_keyframe(ctx, 3, &kf) != SBX_OK || fabs(kf.time_sec - 180.0) > 1e-9)
    fail("nested block keyframe #3 time mismatch");
  rc = sbx_context_load_sbg_timing_text(ctx, sbg_multivoice_block_text, 0);
  if (rc != SBX_OK) fail("multivoice block-definition sbg timing load failed");
  free(buf);
  frames = (size_t)(1.6 * cfg.sample_rate);
  buf = (float *)calloc(frames * 2, sizeof(float));
  if (!buf) fail("alloc failed (multivoice block)");
  if (sbx_context_render_f32(ctx, buf, frames) != SBX_OK)
    fail("multivoice block-definition render failed");
  if (!(abs_sum_window(buf, (size_t)(0.1 * cfg.sample_rate), (size_t)(0.4 * cfg.sample_rate)) < 1e-4))
    fail("multivoice block should be silent in initial off segment");
  if (!(abs_sum_window(buf, (size_t)(1.1 * cfg.sample_rate), (size_t)(1.45 * cfg.sample_rate)) > 1e-3))
    fail("multivoice block should render energy in active duo segment");

  rc = sbx_context_load_sbg_timing_text(ctx, sbg_many_direct_tokens_text, 0);
  if (rc != SBX_OK)
    fail("direct multivoice timing line should allow more than six tokens");
  if (sbx_context_keyframe_count(ctx) != 1)
    fail("direct multivoice timing line should produce one keyframe");
  free(buf);
  frames = (size_t)(0.3 * cfg.sample_rate);
  buf = (float *)calloc(frames * 2, sizeof(float));
  if (!buf) fail("alloc failed (many direct tokens)");
  if (sbx_context_render_f32(ctx, buf, frames) != SBX_OK)
    fail("direct multivoice timing render failed");
  if (!(abs_sum_window(buf, (size_t)(0.02 * cfg.sample_rate), (size_t)(0.25 * cfg.sample_rate)) > 1e-3))
    fail("direct multivoice timing render should produce non-zero energy");

  rc = sbx_context_load_sbg_timing_text(ctx, sbg_many_block_tokens_text, 0);
  if (rc != SBX_OK)
    fail("block entry should allow more than six tokens");
  if (sbx_context_keyframe_count(ctx) != 1)
    fail("block multivoice expansion should produce one keyframe");
  free(buf);
  frames = (size_t)(0.3 * cfg.sample_rate);
  buf = (float *)calloc(frames * 2, sizeof(float));
  if (!buf) fail("alloc failed (many block tokens)");
  if (sbx_context_render_f32(ctx, buf, frames) != SBX_OK)
    fail("block multivoice timing render failed");
  if (!(abs_sum_window(buf, (size_t)(0.02 * cfg.sample_rate), (size_t)(0.25 * cfg.sample_rate)) > 1e-3))
    fail("block multivoice timing render should produce non-zero energy");

  rc = sbx_context_load_sbg_timing_text(ctx, "self: {\n+00:00 self\n}\nNOW self\n", 0);
  if (rc != SBX_EINVAL)
    fail("self-referential nested block should fail");

  free(buf);
  sbx_context_destroy(ctx);
  printf("PASS: sbagenxlib sbg timing loader checks\n");
  return 0;
}
