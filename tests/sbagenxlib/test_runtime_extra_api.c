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
  SbxImmediateParseConfig cfg;
  SbxRuntimeExtraSpec spec;
  char err[256];
  int rc;

  sbx_default_immediate_parse_config(&cfg);
  cfg.default_waveform = SBX_WAVE_TRIANGLE;
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
  rc = sbx_parse_runtime_extra_text(
      " mix/87 mixam:beat:d=0.5:a=0.1:r=0.1:e=3:f=0.2 200@4/30 pink/20 unsupported_token ",
      &cfg, &spec, err, sizeof(err));
  if (rc != SBX_OK) fail(err[0] ? err : "sbx_parse_runtime_extra_text failed");
  if (!spec.have_mix || fabs(spec.mix_amp_pct - 87.0) > 1e-9)
    fail("mix/<amp> not preserved");
  if (spec.aux_count != 2)
    fail("expected two runtime extra tones");
  if (spec.mix_fx_count != 1)
    fail("expected one runtime extra mix effect");
  if (!spec.unsupported || strcmp(spec.bad_token, "unsupported_token") != 0)
    fail("unsupported token tracking mismatch");
  if (spec.aux_tones[0].mode != SBX_TONE_ISOCHRONIC ||
      fabs(spec.aux_tones[0].duty_cycle - 1.0) > 1e-9 ||
      spec.aux_tones[0].iso_edge_mode != 3)
    fail("iso override not applied to runtime extras");
  if (spec.aux_tones[1].mode != SBX_TONE_PINK_NOISE)
    fail("pink noise extra not preserved");
  if (spec.mix_fx[0].type != SBX_MIXFX_AM ||
      !spec.mix_fx[0].mixam_bind_program_beat ||
      fabs(spec.mix_fx[0].mixam_start - 0.1) > 1e-9 ||
      fabs(spec.mix_fx[0].mixam_floor - 0.25) > 1e-9)
    fail("mixam override not applied to runtime extras");
  if (!sbx_runtime_extra_has_mixam(&spec))
    fail("mixam helper should report present AM extra");

  sbx_default_runtime_extra_spec(&spec);
  if (spec.have_mix || spec.aux_count || spec.mix_fx_count || spec.mix_amp_pct != 100.0)
    fail("default runtime extra spec mismatch");

  puts("PASS: sbagenxlib runtime extra parser handles built-in extra token tails");
  return 0;
}
