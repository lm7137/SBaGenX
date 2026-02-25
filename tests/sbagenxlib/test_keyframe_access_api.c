#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "sbagenxlib.h"

static void
fail(const char *msg) {
  fprintf(stderr, "FAIL: %s\n", msg);
  exit(1);
}

int
main(void) {
  SbxEngineConfig cfg;
  SbxContext *ctx;
  SbxProgramKeyframe kf;
  const char *seq_text =
      "0s 200+10/50 linear\n"
      "60s 190+8/40 step\n"
      "120s 180+6/30 linear\n";

  sbx_default_engine_config(&cfg);
  cfg.sample_rate = 44100.0;
  cfg.channels = 2;

  ctx = sbx_context_create(&cfg);
  if (!ctx) fail("context create failed");

  if (sbx_context_load_sequence_text(ctx, seq_text, 0) != SBX_OK)
    fail("load_sequence_text failed");

  if (sbx_context_keyframe_count(ctx) != 3)
    fail("unexpected keyframe count");

  if (sbx_context_get_keyframe(ctx, 0, &kf) != SBX_OK)
    fail("get keyframe[0] failed");
  if (fabs(kf.time_sec - 0.0) > 1e-9) fail("keyframe[0] time mismatch");
  if (kf.interp != SBX_INTERP_LINEAR) fail("keyframe[0] interp mismatch");

  if (sbx_context_get_keyframe(ctx, 1, &kf) != SBX_OK)
    fail("get keyframe[1] failed");
  if (fabs(kf.time_sec - 60.0) > 1e-9) fail("keyframe[1] time mismatch");
  if (kf.interp != SBX_INTERP_STEP) fail("keyframe[1] interp mismatch");

  if (sbx_context_get_keyframe(ctx, 2, &kf) != SBX_OK)
    fail("get keyframe[2] failed");
  if (fabs(kf.time_sec - 120.0) > 1e-9) fail("keyframe[2] time mismatch");
  if (kf.interp != SBX_INTERP_LINEAR) fail("keyframe[2] interp mismatch");

  if (sbx_context_get_keyframe(ctx, 3, &kf) != SBX_EINVAL)
    fail("out-of-range index should fail");
  if (sbx_context_get_keyframe(ctx, 0, NULL) != SBX_EINVAL)
    fail("null out pointer should fail");
  if (sbx_context_get_keyframe(NULL, 0, &kf) != SBX_EINVAL)
    fail("null context should fail");

  if (sbx_context_load_tone_spec(ctx, "200+8/40") != SBX_OK)
    fail("load_tone_spec failed");
  if (sbx_context_keyframe_count(ctx) != 0)
    fail("keyframe count should be zero for static tone");

  sbx_context_destroy(ctx);
  printf("PASS: sbagenxlib keyframe access API checks\n");
  return 0;
}
