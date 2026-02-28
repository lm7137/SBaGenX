#include <stdio.h>
#include <stdlib.h>

#include "sbagenxlib.h"

int
main(void) {
  SbxEngineConfig cfg;
  SbxContext *ctx;
  SbxProgramKeyframe kf[2];
  SbxMixAmpKeyframe mk[2];
  SbxToneSpec tone;
  SbxMixFxSpec fxv[4];
  double beat_hz[5];
  double mix_pct[5];
  size_t fx_count = 0;

  sbx_default_engine_config(&cfg);
  ctx = sbx_context_create(&cfg);
  if (!ctx) {
    fprintf(stderr, "failed to create context\n");
    return 1;
  }

  sbx_default_tone_spec(&kf[0].tone);
  sbx_default_tone_spec(&kf[1].tone);
  kf[0].time_sec = 0.0;
  kf[0].tone.carrier_hz = 200.0;
  kf[0].tone.beat_hz = 10.0;
  kf[0].tone.amplitude = 0.25;
  kf[0].interp = SBX_INTERP_LINEAR;
  kf[1] = kf[0];
  kf[1].time_sec = 30.0;
  kf[1].tone.beat_hz = 2.5;

  if (sbx_context_load_keyframes(ctx, kf, 2, 0) != SBX_OK) {
    fprintf(stderr, "load keyframes failed: %s\n", sbx_context_last_error(ctx));
    sbx_context_destroy(ctx);
    return 1;
  }

  mk[0].time_sec = 0.0;
  mk[0].amp_pct = 100.0;
  mk[0].interp = SBX_INTERP_LINEAR;
  mk[1].time_sec = 30.0;
  mk[1].amp_pct = 70.0;
  mk[1].interp = SBX_INTERP_LINEAR;

  if (sbx_context_set_mix_amp_keyframes(ctx, mk, 2, 100.0) != SBX_OK) {
    fprintf(stderr, "set mix amp keyframes failed: %s\n", sbx_context_last_error(ctx));
    sbx_context_destroy(ctx);
    return 1;
  }

  if (sbx_parse_mix_fx_spec("mixam:beat:m=cos:s=0:f=0.45", SBX_WAVE_SINE, &fxv[0]) != SBX_OK) {
    fprintf(stderr, "parse mixam failed\n");
    sbx_context_destroy(ctx);
    return 1;
  }
  if (sbx_context_set_mix_effects(ctx, fxv, 1) != SBX_OK) {
    fprintf(stderr, "set mix effects failed: %s\n", sbx_context_last_error(ctx));
    sbx_context_destroy(ctx);
    return 1;
  }

  if (sbx_context_sample_program_beat(ctx, 0.0, 30.0, 5, NULL, beat_hz) != SBX_OK) {
    fprintf(stderr, "sample beat failed: %s\n", sbx_context_last_error(ctx));
    sbx_context_destroy(ctx);
    return 1;
  }
  if (sbx_context_sample_mix_amp(ctx, 0.0, 30.0, 5, NULL, mix_pct) != SBX_OK) {
    fprintf(stderr, "sample mix amp failed: %s\n", sbx_context_last_error(ctx));
    sbx_context_destroy(ctx);
    return 1;
  }
  if (sbx_context_sample_mix_effects(ctx, 15.0, fxv, 4, &fx_count) != SBX_OK) {
    fprintf(stderr, "sample mix effects failed: %s\n", sbx_context_last_error(ctx));
    sbx_context_destroy(ctx);
    return 1;
  }

  sbx_context_set_time_sec(ctx, 15.0);
  if (sbx_context_sample_tones(ctx, 15.0, 15.0, 1, NULL, &tone) != SBX_OK) {
    fprintf(stderr, "sample tone failed: %s\n", sbx_context_last_error(ctx));
    sbx_context_destroy(ctx);
    return 1;
  }

  printf("api=%d version=%s\n", sbx_api_version(), sbx_version());
  printf("beat0=%.3f beat4=%.3f mix0=%.3f mix4=%.3f fx=%lu carrier@15=%.3f beat@15=%.3f\n",
         beat_hz[0], beat_hz[4],
         mix_pct[0], mix_pct[4],
         (unsigned long)fx_count,
         tone.carrier_hz, tone.beat_hz);

  sbx_context_destroy(ctx);
  return 0;
}
