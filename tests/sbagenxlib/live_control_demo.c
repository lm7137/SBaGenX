#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sbagenxlib.h"

static void
usage(const char *prog) {
  fprintf(stderr,
          "usage: %s [--carrier HZ] [--beat HZ] [--amp VALUE] [--mix PCT] [--duration SEC]\n",
          prog);
}

static int
parse_double_arg(int argc, char **argv, int *index, const char *name, double *out) {
  if (*index + 1 >= argc) {
    fprintf(stderr, "missing value for %s\n", name);
    return 0;
  }
  *out = atof(argv[*index + 1]);
  *index += 1;
  return 1;
}

static void
print_snapshot(SbxContext *ctx) {
  SbxRuntimeTelemetry telem;

  if (sbx_context_get_runtime_telemetry(ctx, &telem) != SBX_OK) {
    fprintf(stderr, "failed to query runtime telemetry\n");
    exit(1);
  }

  printf("%6.2f  %9.3f  %7.3f  %8.3f  %8.3f\n",
         telem.time_sec,
         telem.primary_tone.carrier_hz,
         telem.primary_tone.beat_hz,
         telem.primary_tone.amplitude,
         sbx_context_mix_amp_effective_at(ctx, telem.time_sec));
}

int
main(int argc, char **argv) {
  SbxEngineConfig cfg;
  SbxToneSpec tone;
  SbxContext *ctx;
  double carrier_target = 212.0;
  double beat_target = 6.0;
  double amp_target = 0.35;
  double mix_target = 70.0;
  double duration_sec = 12.0;
  int i;
  int have_carrier = 0;
  int have_beat = 0;
  int have_amp = 0;
  int have_mix = 0;

  for (i = 1; i < argc; ++i) {
    if (!strcmp(argv[i], "--carrier")) {
      if (!parse_double_arg(argc, argv, &i, "--carrier", &carrier_target))
        return 2;
      have_carrier = 1;
    } else if (!strcmp(argv[i], "--beat")) {
      if (!parse_double_arg(argc, argv, &i, "--beat", &beat_target))
        return 2;
      have_beat = 1;
    } else if (!strcmp(argv[i], "--amp")) {
      if (!parse_double_arg(argc, argv, &i, "--amp", &amp_target))
        return 2;
      have_amp = 1;
    } else if (!strcmp(argv[i], "--mix")) {
      if (!parse_double_arg(argc, argv, &i, "--mix", &mix_target))
        return 2;
      have_mix = 1;
    } else if (!strcmp(argv[i], "--duration")) {
      if (!parse_double_arg(argc, argv, &i, "--duration", &duration_sec))
        return 2;
    } else if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) {
      usage(argv[0]);
      return 0;
    } else {
      fprintf(stderr, "unknown option: %s\n", argv[i]);
      usage(argv[0]);
      return 2;
    }
  }

  if (!have_carrier && !have_beat && !have_amp && !have_mix) {
    have_carrier = have_beat = have_amp = have_mix = 1;
  }

  sbx_default_engine_config(&cfg);
  ctx = sbx_context_create(&cfg);
  if (!ctx) {
    fprintf(stderr, "sbx_context_create failed\n");
    return 1;
  }

  sbx_default_tone_spec(&tone);
  tone.mode = SBX_TONE_BINAURAL;
  tone.carrier_hz = 200.0;
  tone.beat_hz = 10.0;
  tone.amplitude = 0.20;
  if (sbx_context_set_tone(ctx, &tone) != SBX_OK) {
    fprintf(stderr, "sbx_context_set_tone failed\n");
    sbx_context_destroy(ctx);
    return 1;
  }
  if (sbx_context_set_mix_amp_keyframes(ctx, NULL, 0, 100.0) != SBX_OK) {
    fprintf(stderr, "sbx_context_set_mix_amp_keyframes failed\n");
    sbx_context_destroy(ctx);
    return 1;
  }

  if (have_carrier &&
      sbx_context_ramp_live_control(ctx, SBX_LIVE_CONTROL_CARRIER_HZ, carrier_target, duration_sec) != SBX_OK) {
    fprintf(stderr, "carrier live control failed\n");
    sbx_context_destroy(ctx);
    return 1;
  }
  if (have_beat &&
      sbx_context_ramp_live_control(ctx, SBX_LIVE_CONTROL_BEAT_HZ, beat_target, duration_sec) != SBX_OK) {
    fprintf(stderr, "beat live control failed\n");
    sbx_context_destroy(ctx);
    return 1;
  }
  if (have_amp &&
      sbx_context_ramp_live_control(ctx, SBX_LIVE_CONTROL_AMPLITUDE, amp_target, duration_sec) != SBX_OK) {
    fprintf(stderr, "amplitude live control failed\n");
    sbx_context_destroy(ctx);
    return 1;
  }
  if (have_mix &&
      sbx_context_ramp_live_control(ctx, SBX_LIVE_CONTROL_MIX_AMP_PCT, mix_target, duration_sec) != SBX_OK) {
    fprintf(stderr, "mix live control failed\n");
    sbx_context_destroy(ctx);
    return 1;
  }

  printf("# live control demo\n");
  printf("# base tone: carrier=200.000 beat=10.000 amplitude=0.200 mix=100.000\n");
  printf("# ramp duration: %.3f sec\n", duration_sec);
  printf("#\n");
  printf("#  t_sec  carrier_hz  beat_hz  amplitude   mix_pct\n");

  for (i = 0; i <= 4; ++i) {
    double t_sec = duration_sec * (double)i / 4.0;
    if (sbx_context_set_time_sec(ctx, t_sec) != SBX_OK) {
      fprintf(stderr, "failed to seek to %.3f sec\n", t_sec);
      sbx_context_destroy(ctx);
      return 1;
    }
    print_snapshot(ctx);
  }

  sbx_context_destroy(ctx);
  return 0;
}
