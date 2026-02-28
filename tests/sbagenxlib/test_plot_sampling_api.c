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
  SbxEngineConfig cfg;
  SbxContext *ctx;
  SbxProgramKeyframe kf[2];
  SbxToneSpec samples[8];
  SbxToneSpec iso_tone;
  SbxIsoEnvelopeSpec iso_env;
  SbxMixFxSpec mixam;
  SbxMixFxSpec fxv[4];
  SbxMixFxSpec runtime_fx[2];
  SbxMixFxSpec timed_fx[2];
  SbxMixAmpKeyframe mix_kf[2];
  double ts[8];
  double hz[8];
  double mix_pct[8];
  double env[8];
  double gain[8];
  double wave[8];
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

  rc = sbx_context_sample_tones(ctx, 0.0, 1.0, 0, ts, samples);
  if (rc != SBX_EINVAL) fail("sample_count=0 should fail");
  rc = sbx_context_sample_program_beat(ctx, 0.0, 1.0, 0, ts, hz);
  if (rc != SBX_EINVAL) fail("sample_program_beat sample_count=0 should fail");

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
    rc = sbx_context_sample_tones_voice(ctx, 2, 0.0, 10.0, 3, ts, samples);
    if (rc != SBX_EINVAL) fail("out-of-range sample_tones_voice should fail");
    rc = sbx_context_sample_program_beat_voice(ctx, 2, 0.0, 10.0, 3, ts, hz);
    if (rc != SBX_EINVAL) fail("out-of-range sample_program_beat_voice should fail");
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

  if (sbx_parse_tone_spec("200@1/100", &iso_tone) != SBX_OK)
    fail("parse isochronic tone failed");
  sbx_default_iso_envelope_spec(&iso_env);
  iso_env.start = 0.1;
  iso_env.duty = 0.4;
  iso_env.attack = 0.25;
  iso_env.release = 0.25;
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

  sbx_context_destroy(ctx);
  printf("PASS: sbagenxlib plot sampling API checks\n");
  return 0;
}
