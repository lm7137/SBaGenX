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
  SbxIsoEnvelopeSpec iso;
  SbxMixFxSpec mixam;
  SbxMixModSpec mixmod;
  char err[256];
  int rc;

  sbx_default_iso_envelope_spec(&iso);
  rc = sbx_parse_iso_envelope_option_spec("s=0:d=1:e=3", &iso, err, sizeof(err));
  if (rc != SBX_OK) fail(err[0] ? err : "sbx_parse_iso_envelope_option_spec failed");
  if (fabs(iso.start - 0.0) > 1e-9 ||
      fabs(iso.duty - 1.0) > 1e-9 ||
      fabs(iso.attack - 0.5) > 1e-9 ||
      fabs(iso.release - 0.5) > 1e-9 ||
      iso.edge_mode != 3)
    fail("iso option defaults/overrides mismatch");

  rc = sbx_parse_iso_envelope_option_spec("a=0.2:r=0.3", &iso, err, sizeof(err));
  if (rc != SBX_OK) fail(err[0] ? err : "sbx_parse_iso_envelope_option_spec incremental override failed");
  if (fabs(iso.start - 0.0) > 1e-9 ||
      fabs(iso.duty - 1.0) > 1e-9 ||
      fabs(iso.attack - 0.2) > 1e-9 ||
      fabs(iso.release - 0.3) > 1e-9 ||
      iso.edge_mode != 3)
    fail("iso option incremental override mismatch");

  err[0] = 0;
  rc = sbx_parse_iso_envelope_option_spec("e=4", &iso, err, sizeof(err));
  if (rc != SBX_EINVAL)
    fail("expected invalid -I parse to fail");
  if (strcmp(err, "-I parameter e must be in range 0..3") != 0)
    fail("unexpected -I error message");

  sbx_default_mixam_envelope_spec(&mixam);
  rc = sbx_parse_mixam_envelope_option_spec("d=0.2:a=0.1:r=0.3:e=2:f=0.25",
                                            &mixam, err, sizeof(err));
  if (rc != SBX_OK) fail(err[0] ? err : "sbx_parse_mixam_envelope_option_spec failed");
  if (mixam.type != SBX_MIXFX_AM ||
      mixam.mixam_mode != SBX_MIXAM_MODE_PULSE ||
      fabs(mixam.mixam_duty - 0.2) > 1e-9 ||
      fabs(mixam.mixam_attack - 0.1) > 1e-9 ||
      fabs(mixam.mixam_release - 0.3) > 1e-9 ||
      mixam.mixam_edge_mode != 2 ||
      fabs(mixam.mixam_floor - 0.25) > 1e-9)
    fail("mixam option defaults/overrides mismatch");

  rc = sbx_parse_mixam_envelope_option_spec("m=cos:s=0.15", &mixam, err, sizeof(err));
  if (rc != SBX_OK) fail(err[0] ? err : "sbx_parse_mixam_envelope_option_spec incremental override failed");
  if (mixam.mixam_mode != SBX_MIXAM_MODE_COS ||
      fabs(mixam.mixam_start - 0.15) > 1e-9 ||
      fabs(mixam.mixam_floor - 0.25) > 1e-9)
    fail("mixam option incremental override mismatch");

  if (!sbx_is_mixam_envelope_option_spec("m=cos:s=0.1"))
    fail("mixam option detector should accept valid spec");
  if (sbx_is_mixam_envelope_option_spec("-foo"))
    fail("mixam option detector should reject dash-prefixed string");

  err[0] = 0;
  rc = sbx_parse_mixam_envelope_option_spec("f=2", &mixam, err, sizeof(err));
  if (rc != SBX_EINVAL)
    fail("expected invalid -H parse to fail");
  if (strcmp(err, "-H parameter f must be in range [0,1]") != 0)
    fail("unexpected -H error message");

  sbx_default_mix_mod_spec(&mixmod);
  rc = sbx_parse_mix_mod_option_spec("d=0.2:e=0.4:k=5:E=0.6",
                                     &mixmod, err, sizeof(err));
  if (rc != SBX_OK) fail(err[0] ? err : "sbx_parse_mix_mod_option_spec failed");
  if (!mixmod.active ||
      fabs(mixmod.delta - 0.2) > 1e-9 ||
      fabs(mixmod.epsilon - 0.4) > 1e-9 ||
      fabs(mixmod.period_sec - 300.0) > 1e-9 ||
      fabs(mixmod.end_level - 0.6) > 1e-9)
    fail("mix-mod option defaults/overrides mismatch");

  rc = sbx_parse_mix_mod_option_spec("E=0.5", &mixmod, err, sizeof(err));
  if (rc != SBX_OK) fail(err[0] ? err : "sbx_parse_mix_mod_option_spec incremental override failed");
  if (fabs(mixmod.delta - 0.2) > 1e-9 ||
      fabs(mixmod.epsilon - 0.4) > 1e-9 ||
      fabs(mixmod.period_sec - 300.0) > 1e-9 ||
      fabs(mixmod.end_level - 0.5) > 1e-9)
    fail("mix-mod option incremental override mismatch");

  if (!sbx_is_mix_mod_option_spec("d=0.3:e=0.4"))
    fail("mix-mod option detector should accept valid spec");
  if (sbx_is_mix_mod_option_spec("-foo"))
    fail("mix-mod option detector should reject dash-prefixed string");

  err[0] = 0;
  rc = sbx_parse_mix_mod_option_spec("d=2", &mixmod, err, sizeof(err));
  if (rc != SBX_EINVAL)
    fail("expected invalid -A parse to fail");
  if (strcmp(err, "-A parameter d must be in range 0..1") != 0)
    fail("unexpected -A error message");

  puts("PASS: sbagenxlib option-spec parsers handle -I/-H/-A semantics");
  return 0;
}
