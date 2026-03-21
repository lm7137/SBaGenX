#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sbagenxlib.h"

static void fail(const char *msg) {
  fprintf(stderr, "FAIL: %s\n", msg);
  exit(1);
}

int main(void) {
  const char *tokens[] = {
      "mix/87",
      "triangle:200+8/20",
      "mixam:beat:d=0.5:a=0.1:r=0.1:e=3:f=0.2",
      "200@4/30"
  };
  const char *bad_tokens[] = {
      "mix/87",
      "mixbeat:3/40"
  };
  SbxImmediateParseConfig cfg;
  SbxImmediateSpec spec;
  char err[256];
  int rc;

  sbx_default_immediate_parse_config(&cfg);
  cfg.have_iso_override = 1;
  cfg.iso_env.start = 0.0;
  cfg.iso_env.duty = 1.0;
  cfg.iso_env.attack = 0.5;
  cfg.iso_env.release = 0.5;
  cfg.iso_env.edge_mode = 3;
  cfg.have_mixam_override = 1;
  cfg.mixam_mode = SBX_MIXAM_MODE_PULSE;
  cfg.mixam_start = 0.1;
  cfg.mixam_duty = 0.3;
  cfg.mixam_attack = 0.2;
  cfg.mixam_release = 0.4;
  cfg.mixam_edge_mode = 2;
  cfg.mixam_floor = 0.25;

  err[0] = 0;
  rc = sbx_parse_immediate_tokens(tokens, 4, &cfg, &spec, err, sizeof(err));
  if (rc != SBX_OK) fail(err[0] ? err : "sbx_parse_immediate_tokens failed");
  if (!spec.have_mix || fabs(spec.mix_amp_pct - 87.0) > 1e-9)
    fail("mix/<amp> not preserved");
  if (spec.tone_count != 2)
    fail("expected two immediate tones");
  if (spec.mix_fx_count != 1)
    fail("expected one mix effect");
  if (spec.tones[0].waveform != SBX_WAVE_TRIANGLE ||
      spec.tones[0].mode != SBX_TONE_BINAURAL)
    fail("first tone mismatch");
  if (spec.tones[1].mode != SBX_TONE_ISOCHRONIC ||
      fabs(spec.tones[1].iso_start - 0.0) > 1e-9 ||
      fabs(spec.tones[1].duty_cycle - 1.0) > 1e-9 ||
      spec.tones[1].iso_edge_mode != 3)
    fail("iso override not applied");
  if (spec.mix_fx[0].type != SBX_MIXFX_AM ||
      !spec.mix_fx[0].mixam_bind_program_beat ||
      fabs(spec.mix_fx[0].mixam_start - 0.1) > 1e-9 ||
      fabs(spec.mix_fx[0].mixam_floor - 0.25) > 1e-9)
    fail("mixam override not applied");

  err[0] = 0;
  rc = sbx_parse_immediate_tokens(bad_tokens, 2, &cfg, &spec, err, sizeof(err));
  if (rc != SBX_EINVAL)
    fail("expected failure when no tone tokens are present");
  if (strcmp(err, "no sbagenxlib-compatible immediate tone tokens were found") != 0)
    fail("unexpected no-tone error message");

  puts("PASS: sbagenxlib immediate token API parses and normalizes token lists");
  return 0;
}
