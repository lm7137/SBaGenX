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

static void
expect_ok(int rc, const char *msg) {
  if (rc != SBX_OK) fail(msg);
}

int
main(void) {
  SbxEngineConfig cfg;
  SbxToneSpec tone;
  SbxToneSpec sample_tone;
  SbxLiveControlState live;
  SbxRuntimeTelemetry telem;
  SbxContext *ctx = 0;
  SbxProgramKeyframe kfs[2];
  float buf[512 * 2];
  double beat = 0.0;
  double mix = 0.0;
  size_t i;

  sbx_default_engine_config(&cfg);
  ctx = sbx_context_create(&cfg);
  if (!ctx) fail("sbx_context_create failed");

  sbx_default_live_control_state(&live);
  if (live.active || live.ramp_active || live.current_value != 0.0)
    fail("default live control state should be zeroed");

  sbx_default_tone_spec(&tone);
  tone.mode = SBX_TONE_BINAURAL;
  tone.carrier_hz = 200.0;
  tone.beat_hz = 10.0;
  tone.amplitude = 0.2;
  expect_ok(sbx_context_set_tone(ctx, &tone), "set static tone failed");

  expect_ok(sbx_context_set_live_control(ctx, SBX_LIVE_CONTROL_CARRIER_HZ, 240.0),
            "set live carrier failed");
  expect_ok(sbx_context_set_live_control(ctx, SBX_LIVE_CONTROL_BEAT_HZ, 7.0),
            "set live beat failed");
  expect_ok(sbx_context_set_live_control(ctx, SBX_LIVE_CONTROL_AMPLITUDE, 0.5),
            "set live amplitude failed");

  expect_ok(sbx_context_sample_tones(ctx, 0.0, 0.0, 1, NULL, &sample_tone),
            "sample static live-controlled tone failed");
  if (!near(sample_tone.carrier_hz, 240.0, 1e-9) ||
      !near(sample_tone.beat_hz, 7.0, 1e-9) ||
      !near(sample_tone.amplitude, 0.5, 1e-9))
    fail("live control should override sampled static tone");

  expect_ok(sbx_context_get_runtime_telemetry(ctx, &telem), "get runtime telemetry failed");
  if (!near(telem.primary_tone.carrier_hz, 240.0, 1e-9) ||
      !near(telem.primary_tone.beat_hz, 7.0, 1e-9) ||
      !near(telem.primary_tone.amplitude, 0.5, 1e-9))
    fail("telemetry should reflect live-controlled primary tone");

  expect_ok(sbx_context_render_f32(ctx, buf, 512), "render static live-controlled tone failed");
  for (i = 0; i < 512 * 2; i++) {
    if (fabs((double)buf[i]) > 1e-6)
      break;
  }
  if (i == 512 * 2)
    fail("rendered static live-controlled tone should contain energy");

  expect_ok(sbx_context_clear_live_control(ctx, SBX_LIVE_CONTROL_CARRIER_HZ),
            "clear live carrier failed");
  expect_ok(sbx_context_clear_live_control(ctx, SBX_LIVE_CONTROL_BEAT_HZ),
            "clear live beat failed");
  expect_ok(sbx_context_clear_live_control(ctx, SBX_LIVE_CONTROL_AMPLITUDE),
            "clear live amplitude failed");
  expect_ok(sbx_context_sample_tones(ctx, 0.0, 0.0, 1, NULL, &sample_tone),
            "sample restored static tone failed");
  if (!near(sample_tone.carrier_hz, 200.0, 1e-9) ||
      !near(sample_tone.beat_hz, 10.0, 1e-9) ||
      !near(sample_tone.amplitude, 0.2, 1e-9))
    fail("clearing live controls should restore base static tone");

  if (!near(sbx_context_mix_amp_at(ctx, 0.0), 100.0, 1e-9) ||
      !near(sbx_context_mix_amp_effective_at(ctx, 0.0), 100.0, 1e-9))
    fail("default mix amp should start at 100 percent");
  expect_ok(sbx_context_set_live_control(ctx, SBX_LIVE_CONTROL_MIX_AMP_PCT, 35.0),
            "set live mix amp failed");
  if (!near(sbx_context_mix_amp_effective_at(ctx, 0.0), 35.0, 1e-9))
    fail("live mix amp should override effective mix amp");
  if (!near(sbx_context_mix_amp_at(ctx, 0.0), 100.0, 1e-9))
    fail("live mix amp should not overwrite base mix amp profile");
  expect_ok(sbx_context_clear_live_control(ctx, SBX_LIVE_CONTROL_MIX_AMP_PCT),
            "clear live mix amp failed");

  sbx_default_tone_spec(&tone);
  tone.mode = SBX_TONE_BINAURAL;
  tone.carrier_hz = 200.0;
  tone.beat_hz = 10.0;
  tone.amplitude = 0.2;
  kfs[0].time_sec = 0.0;
  kfs[0].tone = tone;
  kfs[0].interp = SBX_INTERP_LINEAR;
  tone.beat_hz = 4.0;
  kfs[1].time_sec = 10.0;
  kfs[1].tone = tone;
  kfs[1].interp = SBX_INTERP_LINEAR;
  expect_ok(sbx_context_load_keyframes(ctx, kfs, 2, 0), "load keyframes failed");

  expect_ok(sbx_context_ramp_live_control(ctx, SBX_LIVE_CONTROL_BEAT_HZ, 8.0, 4.0),
            "ramp live beat failed");
  expect_ok(sbx_context_sample_program_beat(ctx, 0.0, 0.0, 1, NULL, &beat),
            "sample beat at ramp start failed");
  if (!near(beat, 10.0, 1e-9))
    fail("live beat ramp should start from current evaluated beat");
  expect_ok(sbx_context_sample_program_beat(ctx, 2.0, 2.0, 1, NULL, &beat),
            "sample beat at ramp midpoint failed");
  if (!near(beat, 9.0, 1e-9))
    fail("live beat ramp midpoint mismatch");
  expect_ok(sbx_context_sample_program_beat(ctx, 4.0, 4.0, 1, NULL, &beat),
            "sample beat at ramp end failed");
  if (!near(beat, 8.0, 1e-9))
    fail("live beat ramp end mismatch");
  expect_ok(sbx_context_sample_program_beat(ctx, 9.0, 9.0, 1, NULL, &beat),
            "sample beat after ramp failed");
  if (!near(beat, 8.0, 1e-9))
    fail("live beat ramp target should hold after the ramp ends");

  expect_ok(sbx_context_set_time_sec(ctx, 2.0), "set time for live control query failed");
  expect_ok(sbx_context_get_live_control(ctx, SBX_LIVE_CONTROL_BEAT_HZ, &live),
            "get live beat control failed");
  if (!live.active || !live.ramp_active || !near(live.current_value, 9.0, 1e-9) ||
      !near(live.target_value, 8.0, 1e-9) ||
      !near(live.start_time_sec, 0.0, 1e-9) ||
      !near(live.end_time_sec, 4.0, 1e-9))
    fail("live beat control state mismatch while ramping");
  expect_ok(sbx_context_set_time_sec(ctx, 5.0), "set time after live ramp failed");
  expect_ok(sbx_context_get_live_control(ctx, SBX_LIVE_CONTROL_BEAT_HZ, &live),
            "get live beat control after ramp failed");
  if (!live.active || live.ramp_active || !near(live.current_value, 8.0, 1e-9))
    fail("live beat control should settle after the ramp ends");
  sbx_context_clear_live_controls(ctx);
  expect_ok(sbx_context_sample_program_beat(ctx, 5.0, 5.0, 1, NULL, &beat),
            "sample beat after clearing live controls failed");
  if (!near(beat, 7.0, 1e-9))
    fail("clearing live controls should restore base keyframed beat");

  expect_ok(sbx_context_set_mix_amp_keyframes(ctx, NULL, 0, 80.0),
            "set default mix amp failed");
  expect_ok(sbx_context_set_time_sec(ctx, 0.0), "reset time before mix ramp failed");
  expect_ok(sbx_context_ramp_live_control(ctx, SBX_LIVE_CONTROL_MIX_AMP_PCT, 20.0, 4.0),
            "ramp live mix amp failed");
  mix = sbx_context_mix_amp_effective_at(ctx, 0.0);
  if (!near(mix, 80.0, 1e-9))
    fail("live mix amp ramp should start from current effective mix amp");
  mix = sbx_context_mix_amp_effective_at(ctx, 2.0);
  if (!near(mix, 50.0, 1e-9))
    fail("live mix amp ramp midpoint mismatch");
  mix = sbx_context_mix_amp_effective_at(ctx, 4.0);
  if (!near(mix, 20.0, 1e-9))
    fail("live mix amp ramp end mismatch");
  if (!near(sbx_context_mix_amp_at(ctx, 2.0), 80.0, 1e-9))
    fail("base mix amp profile should remain unchanged by live mix ramp");

  sbx_context_destroy(ctx);
  printf("PASS: live control api\n");
  return 0;
}
