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
near(double a, double b, double eps) {
  return fabs(a - b) <= eps;
}

static double
stereo_window_abs(const float *buf, size_t frame0, size_t frame1) {
  size_t i;
  double sum = 0.0;
  for (i = frame0; i < frame1; i++) {
    sum += fabs((double)buf[i * 2]);
    sum += fabs((double)buf[i * 2 + 1]);
  }
  return sum;
}

int
main(void) {
  SbxEngineConfig cfg;
  SbxEngine *eng;
  SbxContext *ctx;
  SbxProgramKeyframe kf[2];
  SbxToneSpec samples[8];
  SbxToneSpec iso_tone;
  SbxIsoEnvelopeSpec iso_env;
  SbxMixFxSpec mixam;
  SbxDualPanelPlotDesc dual_desc;
  SbxMixFxSpec fxv[4];
  SbxMixFxSpec runtime_fx[2];
  SbxMixFxSpec timed_fx[2];
  SbxMixAmpKeyframe mix_kf[2];
  SbxToneSpec active[6];
  SbxToneSpec aux[2];
  double ts[8];
  double hz[8];
  double mix_pct[8];
  double env[8];
  double gain[8];
  double wave[8];
  float iso_audio[2000];
  double max_env = 0.0;
  size_t i;
  size_t fx_count = 0;
  int rc;

  if (sbx_api_version() != SBX_API_VERSION)
    fail("api version macro/runtime mismatch");

  sbx_default_engine_config(&cfg);
  cfg.sample_rate = 44100.0;
  cfg.channels = 2;
  ctx = sbx_context_create(&cfg);
  if (!ctx) fail("context create failed");

  sbx_default_tone_spec(&kf[0].tone);
  sbx_default_tone_spec(&kf[1].tone);
  kf[0].time_sec = 0.0;
  kf[0].tone.mode = SBX_TONE_BINAURAL;
  kf[0].tone.carrier_hz = 200.0;
  kf[0].tone.beat_hz = 10.0;
  kf[0].tone.amplitude = 0.4;
  kf[0].interp = SBX_INTERP_LINEAR;
  kf[1] = kf[0];
  kf[1].time_sec = 10.0;
  kf[1].tone.beat_hz = 0.0;

  rc = sbx_context_load_keyframes(ctx, kf, 2, 0);
  if (rc != SBX_OK) fail("load_keyframes linear failed");
  rc = sbx_context_sample_tones(ctx, 0.0, 10.0, 6, ts, samples);
  if (rc != SBX_OK) fail("sample_tones linear failed");
  rc = sbx_context_sample_program_beat(ctx, 0.0, 10.0, 6, ts, hz);
  if (rc != SBX_OK) fail("sample_program_beat linear failed");
  if (!near(samples[0].beat_hz, 10.0, 1e-9)) fail("linear sample[0] beat mismatch");
  if (!near(samples[1].beat_hz, 8.0, 1e-9)) fail("linear sample[1] beat mismatch");
  if (!near(samples[2].beat_hz, 6.0, 1e-9)) fail("linear sample[2] beat mismatch");
  if (!near(samples[3].beat_hz, 4.0, 1e-9)) fail("linear sample[3] beat mismatch");
  if (!near(samples[4].beat_hz, 2.0, 1e-9)) fail("linear sample[4] beat mismatch");
  if (!near(samples[5].beat_hz, 0.0, 1e-9)) fail("linear sample[5] beat mismatch");
  if (!near(hz[0], 10.0, 1e-9) || !near(hz[5], 0.0, 1e-9))
    fail("program beat sample range mismatch");
  if (!near(ts[0], 0.0, 1e-12) || !near(ts[5], 10.0, 1e-12))
    fail("sample_tones output times mismatch");
  if (!near(sbx_context_time_sec(ctx), 0.0, 1e-12))
    fail("sample_tones must not advance context render time");

  kf[0].interp = SBX_INTERP_STEP;
  rc = sbx_context_load_keyframes(ctx, kf, 2, 0);
  if (rc != SBX_OK) fail("load_keyframes step failed");
  rc = sbx_context_sample_tones(ctx, 0.0, 10.0, 3, 0, samples);
  if (rc != SBX_OK) fail("sample_tones step failed");
  if (!near(samples[1].beat_hz, 10.0, 1e-9))
    fail("step interpolation should hold previous beat at midpoint");

  kf[0].interp = SBX_INTERP_LINEAR;
  rc = sbx_context_load_keyframes(ctx, kf, 2, 1);
  if (rc != SBX_OK) fail("load_keyframes loop failed");
  rc = sbx_context_sample_tones(ctx, 8.0, 12.0, 3, ts, samples);
  if (rc != SBX_OK) fail("sample_tones loop failed");
  if (!near(samples[0].beat_hz, 2.0, 1e-9)) fail("loop sample at 8s mismatch");
  if (!near(samples[1].beat_hz, 10.0, 1e-9)) fail("loop sample at 10s mismatch");
  if (!near(samples[2].beat_hz, 8.0, 1e-9)) fail("loop sample at 12s mismatch");

  rc = sbx_context_load_tone_spec(ctx, "200+6/40");
  if (rc != SBX_OK) fail("load_tone_spec failed");
  if (sbx_context_voice_count(ctx) != 1)
    fail("static tone context should expose one primary voice lane");
  rc = sbx_context_sample_tones(ctx, 0.0, 1.0, 4, ts, samples);
  if (rc != SBX_OK) fail("sample_tones static failed");
  rc = sbx_context_sample_program_beat(ctx, 0.0, 1.0, 4, ts, hz);
  if (rc != SBX_OK) fail("sample_program_beat static failed");
  if (samples[0].mode != SBX_TONE_BINAURAL) fail("static mode mismatch");
  if (!near(samples[0].carrier_hz, 200.0, 1e-9)) fail("static carrier mismatch");
  if (!near(samples[0].beat_hz, 6.0, 1e-9)) fail("static beat mismatch");
  if (!near(samples[0].amplitude, 0.4, 1e-9)) fail("static amplitude mismatch");
  if (!near(hz[0], 6.0, 1e-9) || !near(hz[3], 6.0, 1e-9))
    fail("static program beat samples should be constant");
  if (!near(samples[1].beat_hz, samples[0].beat_hz, 1e-12) ||
      !near(samples[3].beat_hz, samples[0].beat_hz, 1e-12))
    fail("static samples should be constant");
  rc = sbx_context_sample_tones_voice(ctx, 1, 0.0, 1.0, 4, ts, samples);
  if (rc != SBX_EINVAL) fail("static tone secondary voice sampling should fail");
  rc = sbx_context_sample_program_beat_voice(ctx, 1, 0.0, 1.0, 4, ts, hz);
  if (rc != SBX_EINVAL) fail("static beat secondary voice sampling should fail");

  sbx_default_tone_spec(&iso_tone);
  iso_tone.mode = SBX_TONE_ISOCHRONIC;
  iso_tone.carrier_hz = 200.0;
  iso_tone.beat_hz = 2.0;
  iso_tone.amplitude = 0.6;
  iso_tone.duty_cycle = 0.35;
  iso_tone.iso_start = 0.2;
  iso_tone.iso_attack = 0.25;
  iso_tone.iso_release = 0.5;
  iso_tone.iso_edge_mode = 3;
  rc = sbx_context_set_tone(ctx, &iso_tone);
  if (rc != SBX_OK) fail("set_tone isochronic failed");
  rc = sbx_context_sample_tones(ctx, 0.0, 0.5, 2, ts, samples);
  if (rc != SBX_OK) fail("sample_tones isochronic failed");
  if (!near(samples[0].duty_cycle, 0.35, 1e-9) ||
      !near(samples[0].iso_start, 0.2, 1e-9) ||
      !near(samples[0].iso_attack, 0.25, 1e-9) ||
      !near(samples[0].iso_release, 0.5, 1e-9) ||
      samples[0].iso_edge_mode != 3)
    fail("static isochronic envelope sampling mismatch");

  rc = sbx_context_sample_tones(ctx, 0.0, 1.0, 0, ts, samples);
  if (rc != SBX_EINVAL) fail("sample_count=0 should fail");
  rc = sbx_context_sample_program_beat(ctx, 0.0, 1.0, 0, ts, hz);
  if (rc != SBX_EINVAL) fail("sample_program_beat sample_count=0 should fail");
  rc = sbx_context_eval_active_tones(ctx, 0.5, NULL, 0, &fx_count);
  if (rc != SBX_OK || fx_count != 1)
    fail("eval_active_tones count query failed for static source");
  rc = sbx_context_eval_active_tones(ctx, 0.5, active, 6, &fx_count);
  if (rc != SBX_OK || fx_count != 1 || !near(active[0].carrier_hz, 200.0, 1e-9))
    fail("eval_active_tones failed for static source");

  mix_kf[0].time_sec = 0.0;
  mix_kf[0].amp_pct = 100.0;
  mix_kf[0].interp = SBX_INTERP_LINEAR;
  mix_kf[1].time_sec = 10.0;
  mix_kf[1].amp_pct = 60.0;
  mix_kf[1].interp = SBX_INTERP_LINEAR;
  rc = sbx_context_set_mix_amp_keyframes(ctx, mix_kf, 2, 100.0);
  if (rc != SBX_OK) fail("set_mix_amp_keyframes failed");
  rc = sbx_context_sample_mix_amp(ctx, 0.0, 10.0, 6, ts, mix_pct);
  if (rc != SBX_OK) fail("sample_mix_amp failed");
  if (!near(mix_pct[0], 100.0, 1e-9) || !near(mix_pct[5], 60.0, 1e-9))
    fail("sample_mix_amp endpoints mismatch");
  if (!near(mix_pct[3], 76.0, 1e-9))
    fail("sample_mix_amp midpoint mismatch");
  rc = sbx_context_sample_mix_amp(ctx, 0.0, 1.0, 0, ts, mix_pct);
  if (rc != SBX_EINVAL) fail("sample_mix_amp sample_count=0 should fail");

  if (sbx_parse_mix_fx_spec("mixbeat:6/0.3", SBX_WAVE_SINE, &runtime_fx[0]) != SBX_OK)
    fail("parse runtime mixbeat failed");
  if (sbx_parse_mix_fx_spec("mixam:beat:m=cos:s=0:f=0.45", SBX_WAVE_SINE, &runtime_fx[1]) != SBX_OK)
    fail("parse runtime mixam failed");
  rc = sbx_context_set_mix_effects(ctx, runtime_fx, 2);
  if (rc != SBX_OK) fail("set_mix_effects failed");
  rc = sbx_context_sample_mix_effects(ctx, 5.0, NULL, 0, &fx_count);
  if (rc != SBX_OK) fail("sample_mix_effects count query failed");
  if (fx_count != 2) fail("sample_mix_effects count query mismatch");
  rc = sbx_context_sample_mix_effects(ctx, 5.0, fxv, 4, &fx_count);
  if (rc != SBX_OK) fail("sample_mix_effects runtime failed");
  if (fx_count != 2) fail("sample_mix_effects runtime count mismatch");
  if (fxv[0].type != SBX_MIXFX_BEAT || fxv[1].type != SBX_MIXFX_AM || !fxv[1].mixam_bind_program_beat)
    fail("sample_mix_effects runtime content mismatch");
  rc = sbx_context_set_mix_effects(ctx, NULL, 0);
  if (rc != SBX_OK) fail("clear runtime mix effects failed");

  if (sbx_parse_mix_fx_spec("mixbeat:4/0.2", SBX_WAVE_SINE, &timed_fx[0]) != SBX_OK)
    fail("parse timed mixbeat failed");
  if (sbx_parse_mix_fx_spec("mixpulse:7/0.4", SBX_WAVE_SINE, &timed_fx[1]) != SBX_OK)
    fail("parse timed mixpulse failed");
  {
    const char *sbg =
      "pad: mix/100 mixbeat:4/20 mixpulse:7/40\n"
      "00:00 pad\n"
      "00:10 mix/100\n";
    rc = sbx_context_load_sbg_timing_text(ctx, sbg, 0);
    if (rc != SBX_OK) fail("load_sbg_timing_text for mixfx sample failed");
  }
  rc = sbx_context_sample_mix_effects(ctx, 5.0, NULL, 0, &fx_count);
  if (rc != SBX_OK) fail("sample_mix_effects timed count query failed");
  if (fx_count != 2) fail("sample_mix_effects timed count mismatch");
  rc = sbx_context_sample_mix_effects(ctx, 5.0, fxv, 4, &fx_count);
  if (rc != SBX_OK) fail("sample_mix_effects timed failed");
  if (fxv[0].type != SBX_MIXFX_BEAT || fxv[1].type != SBX_MIXFX_PULSE)
    fail("sample_mix_effects timed content mismatch");

  {
    const char *sbg_multivoice_sampling_text =
      "duo: 180+10/20 260+2/15\n"
      "swap: 180+0/20 260+8/15\n"
      "00:00 duo ->\n"
      "00:00:10 swap\n";
    rc = sbx_context_load_sbg_timing_text(ctx, sbg_multivoice_sampling_text, 0);
    if (rc != SBX_OK) fail("load_sbg_timing_text multivoice sampling failed");
    if (sbx_context_voice_count(ctx) != 2)
      fail("multivoice sampling fixture should expose two voice lanes");
    rc = sbx_context_sample_tones_voice(ctx, 1, 0.0, 10.0, 3, ts, samples);
    if (rc != SBX_OK) fail("sample_tones_voice failed");
    if (!near(samples[0].carrier_hz, 260.0, 1e-9) ||
        !near(samples[0].beat_hz, 2.0, 1e-9) ||
        !near(samples[0].amplitude, 0.15, 1e-9))
      fail("sample_tones_voice start mismatch");
    if (!near(samples[1].beat_hz, 5.0, 1e-9) ||
        !near(samples[2].beat_hz, 8.0, 1e-9))
      fail("sample_tones_voice interpolation mismatch");
    rc = sbx_context_sample_program_beat_voice(ctx, 1, 0.0, 10.0, 3, ts, hz);
    if (rc != SBX_OK) fail("sample_program_beat_voice failed");
    if (!near(hz[0], 2.0, 1e-9) || !near(hz[1], 5.0, 1e-9) || !near(hz[2], 8.0, 1e-9))
      fail("sample_program_beat_voice mismatch");
    if (!near(sbx_context_time_sec(ctx), 0.0, 1e-12))
      fail("voice sampling must not advance context render time");
    if (sbx_parse_tone_spec("320+0/10", &aux[0]) != SBX_OK ||
        sbx_parse_tone_spec("white/5", &aux[1]) != SBX_OK)
      fail("parse aux tones for active tone eval failed");
    rc = sbx_context_set_aux_tones(ctx, aux, 2);
    if (rc != SBX_OK) fail("set_aux_tones failed");
    rc = sbx_context_eval_active_tones(ctx, 5.0, NULL, 0, &fx_count);
    if (rc != SBX_OK || fx_count != 4)
      fail("eval_active_tones count query failed for multivoice source");
    rc = sbx_context_eval_active_tones(ctx, 5.0, active, 6, &fx_count);
    if (rc != SBX_OK) fail("eval_active_tones failed");
    if (fx_count != 4)
      fail("eval_active_tones count mismatch");
    if (!near(active[0].carrier_hz, 180.0, 1e-9) || !near(active[0].beat_hz, 5.0, 1e-9))
      fail("eval_active_tones primary lane mismatch");
    if (!near(active[1].carrier_hz, 260.0, 1e-9) || !near(active[1].beat_hz, 5.0, 1e-9))
      fail("eval_active_tones secondary lane mismatch");
    if (!near(active[2].carrier_hz, 320.0, 1e-9) || !near(active[2].amplitude, 0.10, 1e-9))
      fail("eval_active_tones first aux mismatch");
    if (active[3].mode != SBX_TONE_WHITE_NOISE || !near(active[3].amplitude, 0.05, 1e-9))
      fail("eval_active_tones second aux mismatch");
    rc = sbx_context_sample_tones_voice(ctx, 2, 0.0, 10.0, 3, ts, samples);
    if (rc != SBX_EINVAL) fail("out-of-range sample_tones_voice should fail");
    rc = sbx_context_sample_program_beat_voice(ctx, 2, 0.0, 10.0, 3, ts, hz);
    if (rc != SBX_EINVAL) fail("out-of-range sample_program_beat_voice should fail");
    rc = sbx_context_set_aux_tones(ctx, NULL, 0);
    if (rc != SBX_OK) fail("clear aux tones failed");
  }

  if (sbx_parse_mix_fx_spec("mixam:1:m=cos:s=0:f=0.45", SBX_WAVE_SINE, &mixam) != SBX_OK)
    fail("parse mixam cosine spec failed");
  rc = sbx_sample_mixam_cycle(&mixam, 1.0, 5, ts, env, gain);
  if (rc != SBX_OK) fail("sample_mixam_cycle failed");
  if (!near(ts[0], 0.0, 1e-12) || !near(ts[4], 1.0, 1e-12))
    fail("mixam cycle time axis mismatch");
  if (!near(env[0], 1.0, 1e-9) || !near(env[2], 0.0, 1e-9) || !near(env[4], 1.0, 1e-9))
    fail("mixam cosine envelope mismatch");
  if (!near(gain[0], 1.0, 1e-9) || !near(gain[2], 0.45, 1e-9) || !near(gain[4], 1.0, 1e-9))
    fail("mixam cosine gain mismatch");
  rc = sbx_build_mixam_cycle_plot_desc(&mixam, 1.0, &dual_desc);
  if (rc != SBX_OK) fail("build_mixam_cycle_plot_desc failed");
  if (strcmp(dual_desc.x_label, "CYCLE") != 0 ||
      strcmp(dual_desc.top_y_label, "ENVELOPE") != 0 ||
      strcmp(dual_desc.bottom_y_label, "GAIN") != 0)
    fail("mixam plot labels mismatch");
  if (dual_desc.x_tick_count < 2 || dual_desc.top_y_tick_count < 2 ||
      dual_desc.bottom_y_tick_count < 2)
    fail("mixam plot tick counts too small");
  if (strstr(dual_desc.title, "MIXAM") == NULL ||
      strstr(dual_desc.line1, "H:m=cos") == NULL ||
      strstr(dual_desc.line2, "mixam cycle gain") == NULL)
    fail("mixam plot text mismatch");

  if (sbx_parse_tone_spec("200@1/100", &iso_tone) != SBX_OK)
    fail("parse isochronic tone failed");
  iso_tone.iso_start = 0.25;
  iso_tone.iso_attack = 0.0;
  iso_tone.iso_release = 0.0;
  iso_tone.iso_edge_mode = 0;
  sbx_default_iso_envelope_spec(&iso_env);
  if (!near(iso_env.start, 0.048493, 1e-12) ||
      !near(iso_env.duty, 0.403014, 1e-12) ||
      !near(iso_env.attack, 0.5, 1e-12) ||
      !near(iso_env.release, 0.5, 1e-12) ||
      iso_env.edge_mode != 2)
    fail("default isochronic envelope spec mismatch");
  rc = sbx_sample_isochronic_cycle(&iso_tone, NULL, 8, ts, env, wave);
  if (rc != SBX_OK) fail("sample_isochronic_cycle default env failed");
  if (!(env[0] == 0.0 && env[1] == 0.0 && env[2] > 0.0))
    fail("isochronic default-env sampling should honor tone envelope fields");
  iso_env.start = 0.1;
  iso_env.duty = 0.4;
  iso_env.attack = 0.25;
  iso_env.release = 0.25;
  max_env = 0.0;
  rc = sbx_sample_isochronic_cycle(&iso_tone, &iso_env, 8, ts, env, wave);
  if (rc != SBX_OK) fail("sample_isochronic_cycle failed");
  if (!near(ts[0], 0.0, 1e-12) || !near(ts[7], 1.0, 1e-12))
    fail("isochronic cycle time axis mismatch");
  for (i = 0; i < 8; i++) {
    if (env[i] > max_env) max_env = env[i];
    if (fabs(wave[i]) > 1.0 + 1e-9) fail("isochronic waveform out of range");
  }
  if (!(env[0] == 0.0 && env[1] > 0.0))
    fail("isochronic envelope should rise after delayed start");
  if (max_env < 0.5)
    fail("isochronic envelope peak unexpectedly low");
  if (fabs(wave[0]) > 1e-12)
    fail("isochronic waveform should start at zero when envelope is zero");

  {
    const char *sbg_custom_iso_text =
      "custom00: 0 0 1 1\n"
      "00:00 custom00:200@1/100\n";
    SbxProgramKeyframe custom_kf;
    rc = sbx_context_load_sbg_timing_text(ctx, sbg_custom_iso_text, 0);
    if (rc != SBX_OK) fail("load customNN isochronic timing failed");
    if (sbx_context_get_keyframe(ctx, 0, &custom_kf) != SBX_OK)
      fail("get customNN isochronic keyframe failed");
    rc = sbx_context_sample_isochronic_cycle(ctx, &custom_kf.tone, NULL, 8, ts, env, wave);
    if (rc != SBX_OK) fail("context_sample_isochronic_cycle customNN failed");
    if (!(env[0] == 0.0 && env[1] == 0.0 && env[4] > 0.9))
      fail("customNN isochronic cycle should preserve literal zero-based envelope");
  }
  {
    const char *sbg_custom_iso_smooth_text =
      "custom01: e=2 0 1 1 0\n"
      "00:00 custom01:200@1/100\n";
    SbxProgramKeyframe custom_kf;
    int edge_mode = -1;
    rc = sbx_context_load_sbg_timing_text(ctx, sbg_custom_iso_smooth_text, 0);
    if (rc != SBX_OK) fail("load smoothed customNN isochronic timing failed");
    if (sbx_context_get_envelope_edge_mode(ctx, SBX_ENV_WAVE_CUSTOM_BASE + 1, &edge_mode) != SBX_OK)
      fail("get smoothed customNN edge mode failed");
    if (edge_mode != 2)
      fail("customNN e=2 edge mode should be preserved on the context");
    if (sbx_context_get_keyframe(ctx, 0, &custom_kf) != SBX_OK)
      fail("get smoothed customNN isochronic keyframe failed");
    rc = sbx_context_sample_isochronic_cycle(ctx, &custom_kf.tone, NULL, 17, ts, env, wave);
    if (rc != SBX_OK) fail("context_sample_isochronic_cycle smoothed customNN failed");
    if (!(env[1] > 0.10 && env[1] < 0.20))
      fail("customNN e=2 should apply smoothstep segment interpolation");
    if (!(env[2] > 0.45 && env[2] < 0.55))
      fail("customNN e=2 should preserve midpoint level on rising segment");
  }
  {
    const char *sbg_custom_iso_transition_text =
      "custom00: e=2 0 0.1 0.9 1 1 0.9 0.1 0\n"
      "custom01: e=2 0 0.2 0.8 1 1 0.8 0.2 0\n"
      "t1: custom00:300@2.5/20\n"
      "t2: custom01:100@20/20\n"
      "NOW t1\n"
      "+00:05:00 t1 ->\n"
      "+00:06:00 t2\n";
    rc = sbx_context_load_sbg_timing_text(ctx, sbg_custom_iso_transition_text, 0);
    if (rc != SBX_OK) fail("load customNN transition timing failed");
    rc = sbx_context_sample_tones(ctx, 300.0, 360.0, 3, ts, samples);
    if (rc != SBX_OK) fail("sample_tones customNN transition failed");
    if (!near(samples[0].carrier_hz, 300.0, 1e-9) ||
        !near(samples[0].beat_hz, 2.5, 1e-9) ||
        samples[0].envelope_waveform != SBX_ENV_WAVE_CUSTOM_BASE)
      fail("customNN transition start mismatch");
    if (!near(samples[1].carrier_hz, 200.0, 1e-9) ||
        !near(samples[1].beat_hz, 11.25, 1e-9))
      fail("customNN transition midpoint should interpolate carrier/beat");
    if (samples[1].envelope_waveform != SBX_ENV_WAVE_CUSTOM_BASE)
      fail("customNN transition midpoint should retain source envelope until endpoint");
    if (!near(samples[2].carrier_hz, 100.0, 1e-9) ||
        !near(samples[2].beat_hz, 20.0, 1e-9) ||
        samples[2].envelope_waveform != SBX_ENV_WAVE_CUSTOM_BASE + 1)
      fail("customNN transition end mismatch");
  }
  {
    const char *sbg_custom_iso_alloff_text =
      "custom00: e=2 0 0.1 0.9 1 1 0.9 0.1 0\n"
      "t1: custom00:300@2.5/20\n"
      "alloff: -\n"
      "NOW t1\n"
      "+00:05:00 t1 ->\n"
      "+00:06:00 alloff\n";
    rc = sbx_context_load_sbg_timing_text(ctx, sbg_custom_iso_alloff_text, 0);
    if (rc != SBX_OK) fail("load customNN alloff timing failed");
    rc = sbx_context_sample_tones(ctx, 300.0, 360.0, 3, ts, samples);
    if (rc != SBX_OK) fail("sample_tones customNN alloff failed");
    if (!near(samples[0].carrier_hz, 300.0, 1e-9) ||
        !near(samples[0].beat_hz, 2.5, 1e-9) ||
        !near(samples[0].amplitude, 0.20, 1e-9))
      fail("customNN alloff start mismatch");
    if (!near(samples[1].carrier_hz, 300.0, 1e-9) ||
        !near(samples[1].beat_hz, 2.5, 1e-9) ||
        !near(samples[1].amplitude, 0.10, 1e-9) ||
        samples[1].envelope_waveform != SBX_ENV_WAVE_CUSTOM_BASE)
      fail("customNN alloff midpoint should fade source tone over full slide");
    if (samples[2].mode != SBX_TONE_NONE || !near(samples[2].amplitude, 0.0, 1e-9))
      fail("customNN alloff end should be silent");
  }
  {
    const char *sbg_custom_iso_fadein_text =
      "custom00: e=2 0 0.1 0.9 1 1 0.9 0.1 0\n"
      "t1: custom00:300@2.5/20\n"
      "alloff: -\n"
      "NOW alloff\n"
      "+00:05:00 alloff ->\n"
      "+00:06:00 t1\n";
    rc = sbx_context_load_sbg_timing_text(ctx, sbg_custom_iso_fadein_text, 0);
    if (rc != SBX_OK) fail("load customNN fade-in timing failed");
    rc = sbx_context_sample_tones(ctx, 300.0, 360.0, 3, ts, samples);
    if (rc != SBX_OK) fail("sample_tones customNN fade-in failed");
    if (samples[0].mode != SBX_TONE_NONE || !near(samples[0].amplitude, 0.0, 1e-9))
      fail("customNN fade-in start should be silent");
    if (!near(samples[1].carrier_hz, 300.0, 1e-9) ||
        !near(samples[1].beat_hz, 2.5, 1e-9) ||
        !near(samples[1].amplitude, 0.10, 1e-9) ||
        samples[1].envelope_waveform != SBX_ENV_WAVE_CUSTOM_BASE)
      fail("customNN fade-in midpoint should ramp source tone over full slide");
    if (!near(samples[2].carrier_hz, 300.0, 1e-9) ||
        !near(samples[2].beat_hz, 2.5, 1e-9) ||
        !near(samples[2].amplitude, 0.20, 1e-9) ||
        samples[2].envelope_waveform != SBX_ENV_WAVE_CUSTOM_BASE)
      fail("customNN fade-in end mismatch");
  }
  {
    const char *sbg_cross_type_transition_text =
      "custom00: e=2 0 0.1 0.9 1 1 0.9 0.1 0\n"
      "t1: custom00:300@2.5/20\n"
      "t2: 150+4/20\n"
      "NOW t1\n"
      "+00:05:00 t1\n"
      "+00:06:00 t2\n";
    rc = sbx_context_load_sbg_timing_text(ctx, sbg_cross_type_transition_text, 0);
    if (rc != SBX_OK) fail("load cross-type transition timing failed");
    rc = sbx_context_sample_tones(ctx, 300.0, 360.0, 5, ts, samples);
    if (rc != SBX_OK) fail("sample_tones cross-type transition failed");
    if (samples[1].mode != SBX_TONE_ISOCHRONIC || !near(samples[1].amplitude, 0.10, 1e-9))
      fail("cross-type transition first half should fade out the source tone");
    if (samples[3].mode != SBX_TONE_BINAURAL || !near(samples[3].amplitude, 0.10, 1e-9))
      fail("cross-type transition second half should fade in the target tone");
    if (samples[4].mode != SBX_TONE_BINAURAL ||
        !near(samples[4].carrier_hz, 150.0, 1e-9) ||
        !near(samples[4].beat_hz, 4.0, 1e-9) ||
        !near(samples[4].amplitude, 0.20, 1e-9))
      fail("cross-type transition end mismatch");
  }

  sbx_default_engine_config(&cfg);
  cfg.sample_rate = 1000.0;
  eng = sbx_engine_create(&cfg);
  if (!eng) fail("engine create failed");
  sbx_default_tone_spec(&iso_tone);
  iso_tone.mode = SBX_TONE_ISOCHRONIC;
  iso_tone.carrier_hz = 50.0;
  iso_tone.beat_hz = 1.0;
  iso_tone.amplitude = 1.0;
  iso_tone.waveform = SBX_WAVE_SINE;
  iso_tone.duty_cycle = 0.2;
  iso_tone.iso_start = 0.3;
  iso_tone.iso_attack = 0.0;
  iso_tone.iso_release = 0.0;
  iso_tone.iso_edge_mode = 0;
  rc = sbx_engine_set_tone(eng, &iso_tone);
  if (rc != SBX_OK) fail("engine set isochronic tone failed");
  rc = sbx_engine_render_f32(eng, iso_audio, 1000);
  if (rc != SBX_OK) fail("engine render isochronic failed");
  if (stereo_window_abs(iso_audio, 0, 200) > 1e-6)
    fail("iso engine should remain silent before custom start");
  if (stereo_window_abs(iso_audio, 350, 450) <= 1.0)
    fail("iso engine should emit energy inside custom on-window");
  sbx_engine_destroy(eng);

  sbx_context_destroy(ctx);
  printf("PASS: sbagenxlib plot sampling API checks\n");
  return 0;
}
