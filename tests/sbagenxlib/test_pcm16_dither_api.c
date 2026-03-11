#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sbagenxlib.h"

static void
fail(const char *msg) {
  fprintf(stderr, "FAIL: %s\n", msg);
  exit(1);
}

int
main(void) {
  float zero[64];
  short out0[64], out1[64], out2[64];
  float clip_in[4] = { 2.0f, -2.0f, 1.0f, -1.0f };
  short clip_out[4];
  SbxPcm16DitherState st1, st2;
  size_t i;
  int nonzero = 0, have_pos = 0, have_neg = 0;

  memset(zero, 0, sizeof(zero));
  if (sbx_api_version() != SBX_API_VERSION)
    fail("api version mismatch");

  if (sbx_convert_f32_to_s16(zero, out0, 64, NULL) != SBX_OK)
    fail("convert without dither failed");
  for (i = 0; i < 64; i++)
    if (out0[i] != 0)
      fail("zero input without dither should stay zero");

  sbx_seed_pcm16_dither_state(&st1, 1u);
  sbx_seed_pcm16_dither_state(&st2, 1u);
  if (sbx_convert_f32_to_s16(zero, out1, 64, &st1) != SBX_OK)
    fail("convert with dither failed");
  if (sbx_convert_f32_to_s16(zero, out2, 64, &st2) != SBX_OK)
    fail("repeat convert with same seed failed");
  if (memcmp(out1, out2, sizeof(out1)) != 0)
    fail("same dither seed should be deterministic");

  for (i = 0; i < 64; i++) {
    if (out1[i] != 0) nonzero = 1;
    if (out1[i] > 0) have_pos = 1;
    if (out1[i] < 0) have_neg = 1;
  }
  if (!nonzero || !have_pos || !have_neg)
    fail("TPDF dither should create signed non-zero output around silence");

  if (sbx_convert_f32_to_s16(clip_in, clip_out, 4, NULL) != SBX_OK)
    fail("clip convert failed");
  if (clip_out[0] != 32767 || clip_out[1] != -32768)
    fail("hard clipping at out-of-range inputs mismatch");
  if (clip_out[2] != 32767 || clip_out[3] != -32767)
    fail("unity endpoint conversion mismatch");

  sbx_default_pcm16_dither_state(&st1);
  if (st1.rng_state != 0x12345678u)
    fail("default dither seed mismatch");

  puts("PASS: pcm16 dither conversion API");
  return 0;
}
